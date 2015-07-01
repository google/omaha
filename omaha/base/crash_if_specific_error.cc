// Copyright 2011 Google Inc.
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

#include "omaha/base/crash_if_specific_error.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"

#undef SUCCEEDED
#undef FAILED

namespace omaha {

HRESULT g_crash_specific_error = 0;

void CrashIfSpecificError(HRESULT hr) {
  if (g_crash_specific_error != 0 && hr == g_crash_specific_error) {
    ::RaiseException(0, EXCEPTION_NONCONTINUABLE, 0, NULL);
  }
}

}  // namespace omaha

