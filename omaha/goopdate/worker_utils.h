// Copyright 2009 Google Inc.
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

#ifndef OMAHA_GOOPDATE_WORKER_UTILS_H_
#define OMAHA_GOOPDATE_WORKER_UTILS_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "omaha/common/install_manifest.h"

namespace omaha {

class NetworkRequest;

namespace worker_utils {

// Formats an error message for network errors. Returns true if the error has
// a specific message. Otherwise, formats a generic network connection message
// and returns false.
bool FormatMessageForNetworkError(HRESULT error,
                                  const CString& language,
                                  CString* msg);

// Adds http request details to the event log.
void AddHttpRequestDataToEventLog(HRESULT hr,
                                  HRESULT ssl_hr,
                                  int http_status_code,
                                  const CString& http_trace,
                                  bool is_machine);

bool GetInstallActionForEvent(
    const std::vector<xml::InstallAction>& install_actions,
    xml::InstallAction::InstallEvent install_event,
    const xml::InstallAction** action);

}  // namespace worker_utils

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_WORKER_UTILS_H_
