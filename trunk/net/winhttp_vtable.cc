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

#include "omaha/net/winhttp_vtable.h"

namespace omaha {

bool WinHttpVTable::Load() {
  if (IsLoaded()) {
    return true;
  }
  Clear();
  library_ = ::LoadLibrary(_T("winhttp"));
  if (!library_) {
    library_ = ::LoadLibrary(_T("winhttp5"));
    if (!library_) {
      return false;
    }
  }
  bool is_valid =
     GPA("WinHttpAddRequestHeaders",       &WinHttpAddRequestHeaders_pointer) &&
     GPA("WinHttpCheckPlatform",           &WinHttpCheckPlatform_pointer) &&
     GPA("WinHttpCloseHandle",             &WinHttpCloseHandle_pointer) &&
     GPA("WinHttpConnect",                 &WinHttpConnect_pointer) &&
     GPA("WinHttpCrackUrl",                &WinHttpCrackUrl_pointer) &&
     GPA("WinHttpCreateUrl",               &WinHttpCreateUrl_pointer) &&
     GPA("WinHttpDetectAutoProxyConfigUrl", &WinHttpDetectAutoProxyConfigUrl_pointer) &&    // NOLINT
     GPA("WinHttpGetIEProxyConfigForCurrentUser", &WinHttpGetIEProxyConfigForCurrentUser_pointer) &&    // NOLINT
     GPA("WinHttpGetDefaultProxyConfiguration", &WinHttpGetDefaultProxyConfiguration_pointer) &&    // NOLINT
     GPA("WinHttpGetProxyForUrl",          &WinHttpGetProxyForUrl_pointer) &&
     GPA("WinHttpOpen",                    &WinHttpOpen_pointer) &&
     GPA("WinHttpOpenRequest",             &WinHttpOpenRequest_pointer) &&
     GPA("WinHttpQueryAuthSchemes",        &WinHttpQueryAuthSchemes_pointer) &&
     GPA("WinHttpQueryDataAvailable",      &WinHttpQueryDataAvailable_pointer) &&   // NOLINT
     GPA("WinHttpQueryHeaders",            &WinHttpQueryHeaders_pointer) &&
     GPA("WinHttpQueryOption",             &WinHttpQueryOption_pointer) &&
     GPA("WinHttpReadData",                &WinHttpReadData_pointer) &&
     GPA("WinHttpReceiveResponse",         &WinHttpReceiveResponse_pointer) &&
     GPA("WinHttpSendRequest",             &WinHttpSendRequest_pointer) &&
     GPA("WinHttpSetDefaultProxyConfiguration", &WinHttpSetDefaultProxyConfiguration_pointer) &&    // NOLINT
     GPA("WinHttpSetCredentials",          &WinHttpSetCredentials_pointer) &&
     GPA("WinHttpSetOption",               &WinHttpSetOption_pointer) &&
     GPA("WinHttpSetStatusCallback",       &WinHttpSetStatusCallback_pointer) &&
     GPA("WinHttpSetTimeouts",             &WinHttpSetTimeouts_pointer) &&
     GPA("WinHttpWriteData",               &WinHttpWriteData_pointer);

  if (!is_valid) {
    Unload();
  }
  return is_valid;
}

void WinHttpVTable::Unload() {
  if (library_) {
    ::FreeLibrary(library_);
    library_ = NULL;
  }
  Clear();
}

}   // namespace omaha

