// Copyright 2008-2009 Google Inc.
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

// TODO(omaha): deprecate and use ATL::CAccessToken instead.

#ifndef OMAHA_COMMON_SCOPED_IMPERSONATION_H__
#define OMAHA_COMMON_SCOPED_IMPERSONATION_H__

#include <windows.h>
#include "omaha/common/debug.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

inline DWORD smart_impersonate_helper(HANDLE token) {
  if (!token) {
    return ERROR_INVALID_HANDLE;
  }
  return ::ImpersonateLoggedOnUser(token) ? ERROR_SUCCESS : ::GetLastError();
}

inline void smart_unimpersonate_helper(DWORD result) {
  if (result == ERROR_SUCCESS) {
    VERIFY1(::RevertToSelf());
  }
}

typedef close_fun<void (*)(DWORD), smart_unimpersonate_helper>
    close_impersonation;

typedef value_const<DWORD, static_cast<DWORD>(-1)>
    impersonation_not_init;

typedef scoped_any<DWORD, close_impersonation, impersonation_not_init>
    scoped_impersonation_close;

// Manages the calls to ImpersonateLoggedOnUser and RevertToSelf.
struct scoped_impersonation {
  explicit scoped_impersonation(HANDLE token)
      : result_(smart_impersonate_helper(token)) {
  }

  HRESULT result() const { return get(result_); }

 private:
  const scoped_impersonation_close result_;
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SCOPED_IMPERSONATION_H__
