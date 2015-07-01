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

#include "omaha/base/error.h"
#include <winhttp.h>
#include "base/basictypes.h"
#include "omaha/base/crash_if_specific_error.h"
#include "omaha/base/debug.h"

namespace omaha {

int g_error_extra_code1 = 0;

int error_extra_code1() {
  return g_error_extra_code1;
}

void set_error_extra_code1(int extra_code) {
  g_error_extra_code1 = extra_code;
}

HRESULT HRESULTFromLastError() {
  DWORD error_code = ::GetLastError();
  ASSERT1(error_code != NO_ERROR);
  HRESULT hr = error_code != NO_ERROR ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
  CRASH_IF_SPECIFIC_ERROR(hr);
  return hr;
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

bool IsCupError(HRESULT hr) {
  return hr >= OMAHA_NET_E_CUP_FIRST && hr <= OMAHA_NET_E_CUP_LAST;
}

}  // namespace omaha

