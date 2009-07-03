// Copyright 2003-2009 Google Inc.
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

#include "omaha/common/error.h"
#include <winhttp.h>
#include "base/basictypes.h"
#include "omaha/common/debug.h"

namespace omaha {

HRESULT HRESULTFromLastError() {
  DWORD error_code = ::GetLastError();
  ASSERT1(error_code != NO_ERROR);
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

HRESULT HRESULTFromHttpStatusCode(int status_code) {
  COMPILE_ASSERT(GOOPDATE_E_NETWORK_FIRST + HTTP_STATUS_LAST <
                 GOOPDATE_E_NETWORK_LAST,
                 GOOPDATE_E_NETWORK_LAST_too_small);
  HRESULT hr = S_OK;
  if (HTTP_STATUS_FIRST <= status_code && status_code <= HTTP_STATUS_LAST) {
    hr = GOOPDATE_E_NETWORK_FIRST + status_code;
  } else {
    hr = GOOPDATE_E_NETWORK_LAST;
  }
  ASSERT1(HRESULT_FACILITY(hr) == FACILITY_ITF);
  return hr;
}

}  // namespace omaha

