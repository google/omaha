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

#include "omaha/goopdate/worker_utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/signatures.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/event_logger.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"
#include "omaha/net/network_request.h"

namespace omaha {

namespace worker_utils {

bool FormatMessageForNetworkError(HRESULT error,
                                  const CString& language,
                                  CString* msg) {
  ASSERT1(msg);
  StringFormatter formatter(language);

  switch (error) {
    case GOOPDATE_E_NO_NETWORK:
      VERIFY_SUCCEEDED(formatter.FormatMessage(msg,
                                                IDS_NO_NETWORK_PRESENT_ERROR,
                                                kOmahaShellFileName));
      break;
    case GOOPDATE_E_NETWORK_UNAUTHORIZED:
      VERIFY_SUCCEEDED(
          formatter.LoadString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED, msg));
      break;
    case GOOPDATE_E_NETWORK_FORBIDDEN:
      VERIFY_SUCCEEDED(
          formatter.LoadString(IDS_ERROR_HTTPSTATUS_FORBIDDEN, msg));
      break;
    case GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED:
      VERIFY_SUCCEEDED(
          formatter.LoadString(IDS_ERROR_HTTPSTATUS_PROXY_AUTH_REQUIRED, msg));
      break;
    default:
      VERIFY_SUCCEEDED(formatter.FormatMessage(msg,
                                                IDS_NO_NETWORK_PRESENT_ERROR,
                                                kOmahaShellFileName));
      return false;
  }

  return true;
}

void AddHttpRequestDataToEventLog(HRESULT hr,
                                  HRESULT ssl_hr,
                                  int http_status_code,
                                  const CString& http_trace,
                                  bool is_machine) {
  CString msg;
  SafeCStringFormat(&msg,
                    _T("Http Request Error.\r\n")
                    _T("Error: 0x%08x.  SSL error: 0x%08x.")
                    _T("Http status code: %d.\r\n%s"),
                    hr,
                    ssl_hr,
                    http_status_code,
                    http_trace);

  GoogleUpdateLogEvent http_request_event(EVENTLOG_INFORMATION_TYPE,
                                          kNetworkRequestEventId,
                                          is_machine);
  http_request_event.set_event_desc(msg);
  http_request_event.WriteEvent();
}

// TODO(omaha): there can be more install actions for each install event.
// Minor thing: the return value and the out params are redundant, meaning
// there is no need to have them both. This eliminates an assert at the call
// site.
bool GetInstallActionForEvent(
    const std::vector<xml::InstallAction>& install_actions,
    xml::InstallAction::InstallEvent install_event,
    const xml::InstallAction** action) {
  ASSERT1(action);

  for (size_t i = 0; i < install_actions.size(); ++i) {
    if (install_actions[i].install_event == install_event) {
      *action = &install_actions[i];
      return true;
    }
  }

  return false;
}

}  // namespace worker_utils

}  // namespace omaha

