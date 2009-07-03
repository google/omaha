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
// BitsRequest provides http transactions using BITS, with an optional
// number of retries using a specified network configuration.
//
// BITS is sending the following string as user agent:
//    User-Agent: Microsoft BITS/6.6
// where the version seems to be the version of %windir%\System32\QMgr.dll.
// The user agent of BITS can't be controlled programmatically.
//
// TODO(omaha): the class interface is not stable yet, as a few more
// getters and setters are still needed.

#ifndef OMAHA_NET_BITS_REQUEST_H__
#define OMAHA_NET_BITS_REQUEST_H__

#include <windows.h>
#include <bits.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/utils.h"
#include "omaha/net/http_request.h"

namespace omaha {

class BitsRequest : public HttpRequestInterface {
 public:
  BitsRequest();
  virtual ~BitsRequest();

  virtual HRESULT Close();

  virtual HRESULT Send();

  virtual HRESULT Cancel();

  virtual std::vector<uint8> GetResponse() const {
    return std::vector<uint8>();
  }

  // TODO(omaha): BITS provides access to headers on Windows Vista.
  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const;
  virtual CString GetResponseHeaders() const;

  // Returns the http status code in case of errors or 200 when the file is
  // successfully transferred. BITS does not provide access to the status code
  // directly; in some conditions the status code can be deduced from the error.
  virtual int GetHttpStatusCode() const {
    return request_state_.get() ? request_state_->http_status_code : 0;
  }

  virtual CString ToString() const { return _T("BITS"); }

  virtual void set_session_handle(HINTERNET session_handle) {
    session_handle_ = session_handle;
  }

  virtual void set_url(const CString& url) { url_ = url; }

  virtual void set_request_buffer(const void* buffer, size_t buffer_length) {
    request_buffer_ = buffer;
    request_buffer_length_ = buffer_length;
  }

  virtual void set_network_configuration(const Config& network_config) {
    network_config_ = network_config;
  }

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename) { filename_ = filename; }

  virtual void set_low_priority(bool low_priority) {
    low_priority_ = low_priority;
  }

  virtual void set_callback(NetworkRequestCallback* callback) {
    callback_ = callback;
  }

  virtual void set_additional_headers(const CString& additional_headers) {
    additional_headers_ = additional_headers;
  }

  virtual CString user_agent() const { return user_agent_; }

  virtual void set_user_agent(const CString& user_agent) {
    user_agent_ = user_agent;
  }

  // Sets the minimum length of time that BITS waits after encountering a
  // transient error condition before trying to transfer the file.
  // The default value is 600 seconds.
  void set_minimum_retry_delay(int minimum_retry_delay) {
    minimum_retry_delay_ = minimum_retry_delay;
  }

  // Sets the length of time that BITS tries to transfer the file after a
  // transient error condition occurs. If BITS does not make progress during
  // the retry period, it moves the state of the job from transient error
  // to the error state. The default value is 14 days.
  void set_no_progress_timeout(int no_progress_timeout) {
    no_progress_timeout_ = no_progress_timeout;
  }

 private:
  // Sets invariant job properties, such as the filename and the description.
  // These parameters can't change over the job life time.
  HRESULT SetInvariantJobProperties();

  // Sets non-invariant job properties.
  HRESULT SetJobProperties();

  // Uses the SimpleRequest HttpClient to detect the proxy for the current
  // request.
  HRESULT DetectManualProxy();

  // Specifies how a job connects to the Internet.
  HRESULT SetJobProxyUsage();

  // Runs a polling loop waiting for the job to transition in one of its
  // final states.
  HRESULT DoSend();

  // Handles the BG_JOB_STATE_ERROR. It returns S_OK when the error has
  // been handled, otherwise, it returns the job error code and it makes the
  // control return to the caller of Send.
  HRESULT OnStateError();

  // Handles the BG_JOB_STATE_TRANSFERRING.
  HRESULT OnStateTransferring();

  // Gets username and password through NetworkConfig. If successful, sets the
  // credentials on the BITS job.
  HRESULT GetProxyCredentials();

  // Handles 407 errors. Tries autologon schemes.
  HRESULT HandleProxyAuthenticationError();

  // Handles 407 errors by cycling through the auth schemes, when credentials
  // are already set on the BITS job.
  HRESULT HandleProxyAuthenticationErrorCredsSet();

  int WinHttpToBitsProxyAuthScheme(uint32 winhttp_scheme);
  uint32 BitsToWinhttpProxyAuthScheme(int bits_scheme);

  // Creates or opens an existing job.
  // 'is_created' is true if the job has been created or false if the job
  // has been opened.
  static HRESULT CreateOrOpenJob(const TCHAR* display_name,
                                 IBackgroundCopyJob** bits_job,
                                 bool* is_created);

  // Returns major.minor.0.0 BITS version.
  static ULONGLONG GetBitsVersion();

  // Holds the transient state corresponding to a BITS request.
  struct TransientRequestState {
    TransientRequestState() : http_status_code(0) {
      SetZero(bits_job_id);
    }

    int http_status_code;
    CComPtr<IBackgroundCopyJob> bits_job;
    GUID bits_job_id;
  };

  LLock lock_;
  CString url_;
  CString filename_;
  const void* request_buffer_;          // Contains the request body for POST.
  size_t      request_buffer_length_;   // Length of the request body.
  CString additional_headers_;
  CString user_agent_;
  Config network_config_;
  bool low_priority_;
  bool is_canceled_;
  HINTERNET session_handle_;  // Not owned by this class.
  NetworkRequestCallback* callback_;
  int minimum_retry_delay_;
  int no_progress_timeout_;
  int current_auth_scheme_;

  // For manual proxy authentication, if we do not know the auth scheme that the
  // proxy is using, we set the username/password on all the schemes and try
  // them out in sequence.
  bool creds_set_scheme_unknown_;

  scoped_ptr<TransientRequestState> request_state_;

  // See http://b/1189928
  CComPtr<IBackgroundCopyManager> bits_manager_;

  DISALLOW_EVIL_CONSTRUCTORS(BitsRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_BITS_REQUEST_H__


