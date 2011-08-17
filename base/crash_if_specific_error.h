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

#ifndef OMAHA_BASE_CRASH_IF_SPECIFIC_ERROR_H_
#define OMAHA_BASE_CRASH_IF_SPECIFIC_ERROR_H_
#include <windows.h>

#if defined __cplusplus

namespace omaha {

extern HRESULT g_crash_specific_error;
extern void CrashIfSpecificError(HRESULT hr);

#define CRASH_IF_SPECIFIC_ERROR(hr) CrashIfSpecificError(hr)

inline bool CheckSuccessWithSpecificError(HRESULT hr) {
  CRASH_IF_SPECIFIC_ERROR(hr);
  return hr >= 0;
}

inline bool CheckFailureWithSpecificError(HRESULT hr) {
  CRASH_IF_SPECIFIC_ERROR(hr);
  return hr < 0;
}

}  // namespace omaha

#ifdef SUCCEEDED
#undef SUCCEEDED
#endif
#define SUCCEEDED(hr) omaha::CheckSuccessWithSpecificError(hr)

#ifdef FAILED
#undef FAILED
#endif
#define FAILED(hr) omaha::CheckFailureWithSpecificError(hr)

#else   // defined __cplusplus

#define CRASH_IF_SPECIFIC_ERROR(hr)

#endif  // defined __cplusplus

#endif  // OMAHA_BASE_CRASH_IF_SPECIFIC_ERROR_H_

