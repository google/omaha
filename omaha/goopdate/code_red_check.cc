// Copyright 2007-2010 Google Inc.
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

#include "omaha/goopdate/code_red_check.h"
#include "omaha/base/constants.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/goopdate_metrics.h"
#include "omaha/net/network_request.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/recovery/client/google_update_recovery.h"

namespace omaha {

namespace {

bool IsThreadImpersonatingUser() {
  CAccessToken access_token;
  return access_token.GetThreadToken(TOKEN_READ);
}

HRESULT DownloadCodeRedFile(const TCHAR* url,
                            const TCHAR* file_path,
                            void*,
                            int* http_status_code) {
  ASSERT1(url);
  ASSERT1(file_path);
  ASSERT1(http_status_code);
  *http_status_code = 0;
  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  HRESULT hr = network_manager.GetUserNetworkConfig(&network_config);
  if (FAILED(hr)) {
    return hr;
  }
  NetworkRequest network_request(network_config->session());

  network_request.AddHttpRequest(new SimpleRequest);

  // BITS takes the job to BG_JOB_STATE_TRANSIENT_ERROR when the server returns
  // 204. After the "no progress time out", the BITS job errors out. Since
  // BITS follows the WinHTTP in the fallback chain, the code is expected to
  // execute only if WinHTTP fails to get a response from the server.

  // BITS transfers files only when the job owner is logged on.
  bool is_logged_on(false);
  if (IsThreadImpersonatingUser()) {
    // Code red download thread only impersonates to logged on user. So when
    // impersonation happens, it means the user is logged on.
    is_logged_on = true;
  } else {
    // Assumes the caller is not logged on if the function failed.
    hr = IsUserLoggedOn(&is_logged_on);
    ASSERT1(SUCCEEDED(hr) || !is_logged_on);
  }
  if (is_logged_on) {
    BitsRequest* bits_request(new BitsRequest);
    bits_request->set_minimum_retry_delay(kSecPerMin);
    bits_request->set_no_progress_timeout(5 * kSecPerMin);
    network_request.AddHttpRequest(bits_request);
  }

  hr = network_request.DownloadFile(CString(url), CString(file_path));
  if (FAILED(hr)) {
    return E_FAIL;
  }

  *http_status_code = network_request.http_status_code();
  switch (network_request.http_status_code()) {
    case HTTP_STATUS_OK:
      return S_OK;
    case HTTP_STATUS_NO_CONTENT:
      return E_FAIL;
    default:
      return E_FAIL;
  }
}

HRESULT CreateUniqueTempFileForLoggedOnUser(CString* target_file) {
  ASSERT1(target_file);
  CAccessToken access_token;
  TCHAR buffer[MAX_PATH] = {0};
  VERIFY1(access_token.GetThreadToken(TOKEN_READ));
  HRESULT hr = ::SHGetFolderPath(NULL,
                                 CSIDL_LOCAL_APPDATA,
                                 access_token.GetHandle(),
                                 SHGFP_TYPE_CURRENT,
                                 buffer);
  if (FAILED(hr)) {
    return hr;
  }

  return GetNewFileNameInDirectory(buffer, target_file);
}

HRESULT DownloadCodeRedFileAsLoggedOnUser(const TCHAR* url,
                                      const TCHAR* file_path,
                                      void* callback_argument,
                                      int* http_status_code) {
  ASSERT1(http_status_code);
  *http_status_code = 0;
  scoped_handle logged_on_user_token(
      goopdate_utils::GetImpersonationTokenForMachineProcess(true));
  if (!valid(logged_on_user_token)) {
    return E_FAIL;
  }

  HRESULT hr = S_OK;
  CString download_target_path;
  {
    scoped_impersonation impersonate_user(get(logged_on_user_token));
    hr = HRESULT_FROM_WIN32(impersonate_user.result());
    if (FAILED(hr)) {
      return hr;
    }

    hr = CreateUniqueTempFileForLoggedOnUser(&download_target_path);
    if (FAILED(hr)) {
      return hr;
    }
    if (download_target_path.IsEmpty()) {
      return E_FAIL;
    }

    hr = DownloadCodeRedFile(url,
                             download_target_path,
                             callback_argument,
                             http_status_code);
    if (FAILED(hr)) {
      ::DeleteFile(download_target_path);
      return hr;
    }
  }

  if (!::CopyFile(download_target_path, file_path, FALSE)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
  }

  {
    scoped_impersonation impersonate_user(get(logged_on_user_token));
    HRESULT hr_impersonation = HRESULT_FROM_WIN32(impersonate_user.result());
    if (FAILED(hr_impersonation)) {
      return hr_impersonation;
    }

    ::DeleteFile(download_target_path);
  }
  return hr;
}

// Download Callback for Code Red.
// Returns S_OK when the download of the Code Red file succeeds and E_FAIL
// otherwise.
HRESULT CodeRedDownloadCallback(const TCHAR* url,
                                const TCHAR* file_path,
                                void* callback_argument) {
  ++metric_cr_callback_total;

  int http_status_code = 0;
  HRESULT hr = DownloadCodeRedFileAsLoggedOnUser(url,
                                                 file_path,
                                                 callback_argument,
                                                 &http_status_code);
  if (FAILED(hr) && (http_status_code != HTTP_STATUS_NO_CONTENT)) {
    hr = DownloadCodeRedFile(url,
                             file_path,
                             callback_argument,
                             &http_status_code);
  }

  switch (http_status_code) {
    case HTTP_STATUS_OK:
      ++metric_cr_callback_status_200;
      break;
    case HTTP_STATUS_NO_CONTENT:
      ++metric_cr_callback_status_204;
      break;
    default:
      ++metric_cr_callback_status_other;
      break;
  }

  return hr;
}

}  // namespace

HRESULT CheckForCodeRed(bool is_machine, const CString& omaha_version) {
  bool is_period_overridden = false;
  const int update_interval =
      ConfigManager::Instance()->GetLastCheckPeriodSec(&is_period_overridden);
  if (is_period_overridden && 0 == update_interval) {
    OPT_LOG(L1, (_T("[GetLastCheckPeriodSec is 0][code red checks disabled]")));
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DISABLED_BY_POLICY);
  }

  HRESULT hr = FixGoogleUpdate(kGoogleUpdateAppId,
                               omaha_version,
                               _T(""),     // Omaha doesn't have a language.
                               is_machine,
                               &CodeRedDownloadCallback,
                               NULL);
  CORE_LOG(L2, (_T("[FixGoogleUpdate returned 0x%08x]"), hr));
  return hr;
}

}  // namespace omaha
