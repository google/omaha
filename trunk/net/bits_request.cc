// Copyright 2007-2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================

//
// There is an implicit assumption that one bits job contains only one file.
//
// TODO(omaha): assert somewhere on all the job invariants.
//
// TODO(omaha): no caching at all is implemented, as far as sharing downloads
// between different bits users downloading the same file.
// TODO(omaha): same user downloading same file in different logon session is
// not handled.
// TODO(omaha): generally speaking, impersonation scenarios are not
// yet handled by the code. This is important when creating or opening an
// existing job.

#include "omaha/net/bits_request.h"

#include <winhttp.h>
#include <atlbase.h>
#include <atlstr.h>
#include <functional>
#include "omaha/common/const_addresses.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"
#include "omaha/net/bits_utils.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_request.h"
#include "omaha/net/proxy_auth.h"

namespace omaha {

namespace {

const TCHAR* const kJobDescription = _T("Google Update");

// TODO(omaha): expose polling interval, as it is likely that polling for
// downloading files must be different than polling for retrieving smaller
// files.
const int kPollingIntervalMs = 1000;

// Returns the job priority or -1 in case of errors.
int GetJobPriority(IBackgroundCopyJob* job) {
  ASSERT1(job);
  BG_JOB_PRIORITY priority = BG_JOB_PRIORITY_FOREGROUND;
  return SUCCEEDED(job->GetPriority(&priority)) ? priority : -1;
}

}   // namespace


BitsRequest::BitsRequest()
    : request_buffer_(NULL),
      request_buffer_length_(0),
      low_priority_(false),
      is_canceled_(false),
      callback_(NULL),
      minimum_retry_delay_(-1),
      no_progress_timeout_(-1),
      current_auth_scheme_(0),
      creds_set_scheme_unknown_(false) {
  VERIFY1(SUCCEEDED(GetBitsManager(&bits_manager_)));
}

// Once this instance connects to a BITS job, it either completes the job
// or it cleans it up to avoid leaving junk in the BITS queue.
BitsRequest::~BitsRequest() {
  Close();
  callback_ = NULL;

  // TODO(omaha): for unknown reasons, qmgrprxy.dll gets unloaded at some point
  // during program execution and subsequent calls to BITS crash. This
  // indicates a ref count problem somewhere. The work around is to not
  // call the IUnknown::Release if the module is not in memory.
  if (::GetModuleHandle(_T("qmgrprxy.dll")) == NULL) {
    bits_manager_.Detach();
  }
}

HRESULT BitsRequest::Close() {
  NET_LOG(L3, (_T("[BitsRequest::Close]")));
  __mutexBlock(lock_) {
    if (request_state_.get()) {
      VERIFY1(SUCCEEDED(CancelBitsJob(request_state_->bits_job)));
    }
    request_state_.reset();
  }
  return S_OK;
}

HRESULT BitsRequest::Cancel() {
  NET_LOG(L3, (_T("[BitsRequest::Cancel]")));
  __mutexBlock(lock_) {
    is_canceled_ = true;
    if (request_state_.get()) {
      VERIFY1(SUCCEEDED(CancelBitsJob(request_state_->bits_job)));
    }
  }
  return S_OK;
}

HRESULT BitsRequest::Send() {
  NET_LOG(L3, (_T("[BitsRequest::Send][%s]"), url_));

  ASSERT1(!url_.IsEmpty());

  __mutexBlock(lock_) {
    if (request_state_.get()) {
      VERIFY1(SUCCEEDED(CancelBitsJob(request_state_->bits_job)));
    }
    request_state_.reset(new TransientRequestState);
  }

  bool is_created = false;
  HRESULT hr = BitsRequest::CreateOrOpenJob(filename_,
                                            &request_state_->bits_job,
                                            &is_created);
  if (FAILED(hr)) {
    return hr;
  }

  // The job id is used for logging purposes only.
  request_state_->bits_job->GetId(&request_state_->bits_job_id);

  NET_LOG(L3, (_T("[BITS job %s]"), GuidToString(request_state_->bits_job_id)));

  if (is_created) {
    hr = SetInvariantJobProperties();
    if (FAILED(hr)) {
      HRESULT hr_cancel_bits_job(CancelBitsJob(request_state_->bits_job));
      if (FAILED(hr_cancel_bits_job)) {
        NET_LOG(LW, (_T("[CancelBitsJob failed][0x%08x]"), hr_cancel_bits_job));
      }
      request_state_->bits_job = NULL;
      return hr;
    }
  }

  return DoSend();
}

HRESULT BitsRequest::QueryHeadersString(uint32, const TCHAR*, CString*) const {
  return E_NOTIMPL;
}

CString BitsRequest::GetResponseHeaders() const {
  return CString();
}


HRESULT BitsRequest::CreateOrOpenJob(const TCHAR* display_name,
                                     IBackgroundCopyJob** bits_job,
                                     bool* is_created) {
  ASSERT1(display_name);
  ASSERT1(bits_job);
  ASSERT1(*bits_job == NULL);
  ASSERT1(is_created);

  CComPtr<IBackgroundCopyManager> bits_manager;
  HRESULT hr = GetBitsManager(&bits_manager);
  if (FAILED(hr)) {
    return hr;
  }

  // Try to find if we already have the job in the BITS queue.
  // By convention, the display name of the job is the same as the file name.
  CComPtr<IBackgroundCopyJob> job;
  hr = FindBitsJobIf(std::bind2nd(JobDisplayNameEqual(), display_name),
                     bits_manager,
                     &job);
  if (SUCCEEDED(hr)) {
    NET_LOG(L3, (_T("[found BITS job][%s]"), display_name));
    *bits_job = job.Detach();
    *is_created = false;
    return S_OK;
  }

  GUID guid = {0};
  hr = bits_manager->CreateJob(display_name, BG_JOB_TYPE_DOWNLOAD, &guid, &job);
  if (SUCCEEDED(hr)) {
    *bits_job = job.Detach();
    *is_created = true;
    return S_OK;
  }

  *bits_job = NULL;
  return hr;
}

HRESULT BitsRequest::SetInvariantJobProperties() {
  ASSERT1(request_state_.get());
  ASSERT1(request_state_->bits_job);
  HRESULT hr = request_state_->bits_job->AddFile(url_, filename_);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[IBackgroundCopyJob::AddFile failed][0x%08x]"), hr));
    return hr;
  }
  hr = request_state_->bits_job->SetDescription(kJobDescription);
  if (FAILED(hr)) {
    return hr;
  }
  return S_OK;
}

HRESULT BitsRequest::SetJobProperties() {
  ASSERT1(request_state_.get());
  ASSERT1(request_state_->bits_job);
  BG_JOB_PRIORITY priority = low_priority_ ? BG_JOB_PRIORITY_NORMAL :
                                             BG_JOB_PRIORITY_FOREGROUND;
  HRESULT hr = request_state_->bits_job->SetPriority(priority);
  if (FAILED(hr)) {
    return hr;
  }
  if (minimum_retry_delay_ != -1) {
    ASSERT1(minimum_retry_delay_ >= 0);
    hr = request_state_->bits_job->SetMinimumRetryDelay(minimum_retry_delay_);
    if (FAILED(hr)) {
      return hr;
    }
  }
  if (no_progress_timeout_ != -1) {
    ASSERT1(no_progress_timeout_ >= 0);
    hr = request_state_->bits_job->SetNoProgressTimeout(no_progress_timeout_);
    if (FAILED(hr)) {
      return hr;
    }
  }
  return S_OK;
}

HRESULT BitsRequest::DetectManualProxy() {
  if (NetworkConfig::GetAccessType(network_config_) !=
      WINHTTP_ACCESS_TYPE_AUTO_DETECT) {
    return S_OK;
  }

  HttpClient::ProxyInfo proxy_info = {0};
  HRESULT hr = NetworkConfig::Instance().GetProxyForUrl(
      url_,
      network_config_.auto_config_url,
      &proxy_info);
  if (SUCCEEDED(hr) &&
      proxy_info.access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY) {
    network_config_.auto_detect = false;
    network_config_.auto_config_url.Empty();
    network_config_.proxy = proxy_info.proxy;
    network_config_.proxy_bypass = proxy_info.proxy_bypass;
  }

  ::GlobalFree(const_cast<wchar_t*>(proxy_info.proxy));
  ::GlobalFree(const_cast<wchar_t*>(proxy_info.proxy_bypass));

  NET_LOG(L3, (_T("[GetProxyForUrl returned][0x%08x]"), hr));
  return hr;
}

HRESULT BitsRequest::SetJobProxyUsage() {
  ASSERT1(request_state_.get());
  ASSERT1(request_state_->bits_job);
  BG_JOB_PROXY_USAGE proxy_usage = BG_JOB_PROXY_USAGE_NO_PROXY;
  const TCHAR* proxy = NULL;
  const TCHAR* proxy_bypass = NULL;

  DetectManualProxy();

  int access_type = NetworkConfig::GetAccessType(network_config_);
  if (access_type == WINHTTP_ACCESS_TYPE_AUTO_DETECT) {
    proxy_usage = BG_JOB_PROXY_USAGE_AUTODETECT;
  } else if (access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY) {
    proxy_usage = BG_JOB_PROXY_USAGE_OVERRIDE;
    proxy = network_config_.proxy;
    proxy_bypass = network_config_.proxy_bypass;
  }
  HRESULT hr = request_state_->bits_job->SetProxySettings(proxy_usage,
                                                          proxy,
                                                          proxy_bypass);
  if (FAILED(hr)) {
    return hr;
  }
  if (proxy_usage == BG_JOB_PROXY_USAGE_AUTODETECT ||
      proxy_usage == BG_JOB_PROXY_USAGE_OVERRIDE) {
    // Set implicit credentials if we are going through a proxy, just in case
    // the proxy is requiring authentication. Continue on errors, maybe
    // the credentials won't be needed anyway. There will be one more chance
    // to set credentials when the proxy challenges and the job errors out.
    creds_set_scheme_unknown_ = false;
    hr = SetProxyAuthImplicitCredentials(request_state_->bits_job,
                                         BG_AUTH_SCHEME_NEGOTIATE);
    if (SUCCEEDED(hr)) {
      current_auth_scheme_ = BG_AUTH_SCHEME_NEGOTIATE;
    } else {
      OPT_LOG(LW, (_T("[failed to set BITS proxy credentials][0x%08x]"), hr));
    }
  }
  return S_OK;
}

HRESULT BitsRequest::DoSend() {
  ASSERT1(request_state_.get());
  ASSERT1(request_state_->bits_job);

  NET_LOG(L3, (_T("[BitsRequest::DoSend]")));

  if (is_canceled_) {
    return OMAHA_NET_E_REQUEST_CANCELLED;
  }

  HRESULT hr = SetJobProperties();
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetJobProxyUsage();
  if (FAILED(hr)) {
    return hr;
  }
  hr = request_state_->bits_job->Resume();
  if (FAILED(hr)) {
    return hr;
  }

  NET_LOG(L3, (_T("[job priority %d]"),
               GetJobPriority(request_state_->bits_job)));

  // Poll for state changes. The code executing on the state changes must be
  // idempotent, as the same state can be seen multiple times when looping.
  // There is only one important case, which is retrying the job when the
  // job is in the ERROR state. We attempt to handle the error, for
  // example retrying one more time or changing proxy credentials, and then
  // we resume the job. There is an assumption, so far true, that calling
  // Resume on a job, the state changes right away from SUSPENDED to QUEUED.

  for (;;) {
    if (is_canceled_) {
      return OMAHA_NET_E_REQUEST_CANCELLED;
    }

    BG_JOB_STATE job_state = BG_JOB_STATE_ERROR;
    hr = request_state_->bits_job->GetState(&job_state);
    if (FAILED(hr)) {
      return hr;
    }

    NET_LOG(L3, (_T("[job %s][state %s]"),
                 GuidToString(request_state_->bits_job_id),
                 JobStateToString(job_state)));

    switch (job_state) {
      case BG_JOB_STATE_QUEUED:
        break;

      case BG_JOB_STATE_CONNECTING:
        if (callback_) {
          callback_->OnProgress(0,
                                0,
                                WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER,
                                NULL);
        }
        break;

      case BG_JOB_STATE_TRANSFERRING:
        OnStateTransferring();
        break;

      case BG_JOB_STATE_TRANSIENT_ERROR:
        break;

      case BG_JOB_STATE_ERROR:
        hr = OnStateError();
        if (SUCCEEDED(hr)) {
          // The error handler dealt with the error. Countinue the loop.
          break;
        }

        // Give up.
        return request_state_->http_status_code ? S_OK : hr;

      case BG_JOB_STATE_TRANSFERRED:
        hr = request_state_->bits_job->Complete();
        if (SUCCEEDED(hr) || BG_S_UNABLE_TO_DELETE_FILES == hr) {
          // Assume the status code is 200 if the transfer completed. BITS does
          // ot provide access to the status code.
          request_state_->http_status_code = HTTP_STATUS_OK;

          if (creds_set_scheme_unknown_) {
            // Bits job completed successfully. If we have a valid username, we
            // record the auth scheme with the NetworkConfig, so it can be used
            // in the future within this process.
            ASSERT1(BitsToWinhttpProxyAuthScheme(current_auth_scheme_) !=
                    UNKNOWN_AUTH_SCHEME);
            bool is_https = String_StartsWith(url_, kHttpsProtoScheme, true);
            VERIFY1(SUCCEEDED(NetworkConfig::Instance().SetProxyAuthScheme(
                network_config_.proxy, is_https,
                BitsToWinhttpProxyAuthScheme(current_auth_scheme_))));
          }

          return S_OK;
        } else {
          return hr;
        }

      case BG_JOB_STATE_SUSPENDED:
        // Pausing downloads is not supported yet.
        ASSERT1(false);
        return E_FAIL;

      case BG_JOB_STATE_ACKNOWLEDGED:
        ASSERT1(false);
        return S_OK;

      case BG_JOB_STATE_CANCELLED:
        return OMAHA_NET_E_REQUEST_CANCELLED;
    };

    ::Sleep(kPollingIntervalMs);
  }
}

HRESULT BitsRequest::OnStateTransferring() {
  if (!callback_) {
    return S_OK;
  }
  BG_JOB_PROGRESS progress = {0};
  HRESULT hr = request_state_->bits_job->GetProgress(&progress);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(progress.FilesTotal == 1);
  ASSERT1(progress.BytesTransferred <= INT_MAX);
  ASSERT1(progress.BytesTotal <= INT_MAX);
  callback_->OnProgress(static_cast<int>(progress.BytesTransferred),
                        static_cast<int>(progress.BytesTotal),
                        WINHTTP_CALLBACK_STATUS_READ_COMPLETE,
                        NULL);
  return S_OK;
}

HRESULT BitsRequest::OnStateError() {
  CComPtr<IBackgroundCopyError> error;
  HRESULT hr = request_state_->bits_job->GetError(&error);
  if (FAILED(hr)) {
    return hr;
  }
  BG_ERROR_CONTEXT error_context = BG_ERROR_CONTEXT_NONE;
  HRESULT error_code = E_FAIL;
  hr = error->GetError(&error_context, &error_code);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(FAILED(error_code));

  NET_LOG(L3, (_T("[handle bits error][0x%08x]"), error_code));

  request_state_->http_status_code = GetHttpStatusFromBitsError(error_code);

  if (error_code == BG_E_HTTP_ERROR_407) {
    hr = !creds_set_scheme_unknown_ ? HandleProxyAuthenticationError() :
                                      HandleProxyAuthenticationErrorCredsSet();
    if (SUCCEEDED(hr)) {
      return S_OK;
    }
  }

  // We could not handle this error. The control will return to the caller.
  return error_code;
}

HRESULT BitsRequest::GetProxyCredentials() {
  CString username;
  CString password;
  uint32 auth_scheme = UNKNOWN_AUTH_SCHEME;
  bool is_https = String_StartsWith(url_, kHttpsProtoScheme, true);

  if (!NetworkConfig::Instance().GetProxyCredentials(true, false,
          network_config_.proxy, is_https, &username, &password,
          &auth_scheme)) {
    OPT_LOG(LE, (_T("[BitsRequest::GetProxyCredentials failed]")));
    return E_ACCESSDENIED;
  }

  if (auth_scheme != UNKNOWN_AUTH_SCHEME) {
    current_auth_scheme_ = WinHttpToBitsProxyAuthScheme(auth_scheme);
    OPT_LOG(L3, (_T("[BitsRequest::GetProxyCredentials][%s]"),
                 BitsAuthSchemeToString(current_auth_scheme_)));
    return SetProxyAuthCredentials(request_state_->bits_job,
               CStrBuf(username), CStrBuf(password),
               static_cast<BG_AUTH_SCHEME>(current_auth_scheme_));
  }

  OPT_LOG(L3, (_T("[BitsRequest::GetProxyCredentials][Auth scheme unknown]")));
  // We do not know the scheme beforehand. So we set credentials on all the
  // schemes except BASIC and try them out in seqence. We could have used BASIC
  // as well, however, we do not want to leak passwords by mistake.
  for (int scheme = BG_AUTH_SCHEME_DIGEST; scheme <= BG_AUTH_SCHEME_NEGOTIATE;
       ++scheme) {
    HRESULT hr = SetProxyAuthCredentials(request_state_->bits_job,
                                         CStrBuf(username), CStrBuf(password),
                                         static_cast<BG_AUTH_SCHEME>(scheme));
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[BitsRequest::GetProxyCredentials][0x%08x][%s]"),
                   hr, BitsAuthSchemeToString(scheme)));
      return hr;
    }
  }

  current_auth_scheme_ = BG_AUTH_SCHEME_NEGOTIATE;
  creds_set_scheme_unknown_ = true;
  return S_OK;
}

HRESULT BitsRequest::HandleProxyAuthenticationError() {
  ASSERT1(!creds_set_scheme_unknown_);
  HRESULT hr = E_ACCESSDENIED;

  if (current_auth_scheme_ == 0) {
    current_auth_scheme_ = BG_AUTH_SCHEME_NEGOTIATE;
    hr = SetProxyAuthImplicitCredentials(request_state_->bits_job,
                                         BG_AUTH_SCHEME_NEGOTIATE);
  } else if (current_auth_scheme_ == BG_AUTH_SCHEME_NEGOTIATE) {
    current_auth_scheme_ = BG_AUTH_SCHEME_NTLM;
    hr = SetProxyAuthImplicitCredentials(request_state_->bits_job,
                                         BG_AUTH_SCHEME_NTLM);
  } else {
    hr = GetProxyCredentials();
  }

  OPT_LOG(L3, (_T("[BitsRequest::HandleProxyAuthenticationError][0x%08x][%s]"),
               hr, BitsAuthSchemeToString(current_auth_scheme_)));
  return SUCCEEDED(hr) ? request_state_->bits_job->Resume() : hr;
}

HRESULT BitsRequest::HandleProxyAuthenticationErrorCredsSet() {
  ASSERT1(creds_set_scheme_unknown_);

  if (current_auth_scheme_ == BG_AUTH_SCHEME_NEGOTIATE) {
    current_auth_scheme_ = BG_AUTH_SCHEME_NTLM;
  } else if (current_auth_scheme_ == BG_AUTH_SCHEME_NTLM) {
    current_auth_scheme_ = BG_AUTH_SCHEME_DIGEST;
  } else {
    OPT_LOG(LE, (_T("[HandleProxyAuthenticationErrorCredsSet][Failure]")));
    return E_ACCESSDENIED;
  }

  OPT_LOG(L3, (_T("[BitsRequest::HandleProxyAuthenticationErrorCredsSet][%s]"),
               BitsAuthSchemeToString(current_auth_scheme_)));
  return request_state_->bits_job->Resume();
}

int BitsRequest::WinHttpToBitsProxyAuthScheme(uint32 winhttp_scheme) {
  if (winhttp_scheme == WINHTTP_AUTH_SCHEME_NEGOTIATE) {
    return BG_AUTH_SCHEME_NEGOTIATE;
  }
  if (winhttp_scheme == WINHTTP_AUTH_SCHEME_NTLM) {
    return BG_AUTH_SCHEME_NTLM;
  }
  if (winhttp_scheme == WINHTTP_AUTH_SCHEME_DIGEST) {
    return BG_AUTH_SCHEME_DIGEST;
  }
  if (winhttp_scheme == WINHTTP_AUTH_SCHEME_BASIC) {
    return BG_AUTH_SCHEME_BASIC;
  }

  ASSERT1(false);
  return UNKNOWN_AUTH_SCHEME;
}

uint32 BitsRequest::BitsToWinhttpProxyAuthScheme(int bits_scheme) {
  if (bits_scheme == BG_AUTH_SCHEME_NEGOTIATE) {
    return WINHTTP_AUTH_SCHEME_NEGOTIATE;
  }
  if (bits_scheme == BG_AUTH_SCHEME_NTLM) {
    return WINHTTP_AUTH_SCHEME_NTLM;
  }
  if (bits_scheme == BG_AUTH_SCHEME_DIGEST) {
    return WINHTTP_AUTH_SCHEME_DIGEST;
  }
  if (bits_scheme == BG_AUTH_SCHEME_BASIC) {
    return WINHTTP_AUTH_SCHEME_BASIC;
  }

  ASSERT1(false);
  return UNKNOWN_AUTH_SCHEME;
}

}   // namespace omaha

