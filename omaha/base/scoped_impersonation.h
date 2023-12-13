// Copyright 2008-2010 Google Inc.
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

#ifndef OMAHA_BASE_SCOPED_IMPERSONATION_H_
#define OMAHA_BASE_SCOPED_IMPERSONATION_H_

#include <windows.h>
#include <atlsecurity.h>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

inline void ImpersonateLoggedOnUserOrDie(HANDLE token) {
  if (!::ImpersonateLoggedOnUser(token)) {
    CORE_LOG(LE, (_T("[ImpersonateLoggedOnUser failed][0x%08x]"),
                  HRESULTFromLastError()));
    ::RaiseException(EXCEPTION_IMPERSONATION_FAILED,
                     EXCEPTION_NONCONTINUABLE,
                     0,
                     NULL);
  }
}

inline void RevertToSelfOrDie() {
  if (!::RevertToSelf()) {
    CORE_LOG(LE, (_T("[RevertToSelf failed][0x%08x]"), HRESULTFromLastError()));
    ::RaiseException(EXCEPTION_REVERT_IMPERSONATION_FAILED,
                     EXCEPTION_NONCONTINUABLE,
                     0,
                     NULL);
  }
}

inline HRESULT smart_impersonate_helper(HANDLE token) {
  if (!token) {
    return S_FALSE;
  }
  return ::ImpersonateLoggedOnUser(token) ? S_OK : HRESULTFromLastError();
}

inline void smart_unimpersonate_helper(HRESULT result) {
  if (result == S_OK) {
    RevertToSelfOrDie();
  }
}

typedef close_fun<void (*)(HRESULT), smart_unimpersonate_helper>
    close_impersonation;

typedef value_const<HRESULT, E_FAIL> impersonation_not_init;

typedef scoped_any<HRESULT, close_impersonation, impersonation_not_init>
    scoped_impersonation_close;

// Manages the calls to ImpersonateLoggedOnUser and RevertToSelf. The input
// token is allowed to be NULL in which case no impersonation/revert is
// performed.
struct scoped_impersonation {
  explicit scoped_impersonation(HANDLE token)
      : result_(smart_impersonate_helper(token)) {
    HRESULT hr = result();
    if (token && SUCCEEDED(hr)) {
      CORE_LOG(L3, (_T("[Impersonation succeeded]")));
    } else if (token && FAILED(hr)) {
      CORE_LOG(LW, (_T("[Impersonation failed][0x%x]"), hr));
    } else if (!token) {
      CORE_LOG(LW, (_T("[Impersonation requested but the token was null]")));
    }
  }

  HRESULT result() const { return get(result_); }

 private:
  const scoped_impersonation_close result_;
};

class scoped_revert_to_self {
 public:
  scoped_revert_to_self() {
    if (!token_.GetThreadToken(TOKEN_ALL_ACCESS)) {
      if (::GetLastError() != ERROR_NO_TOKEN) {
        ::RaiseException(EXCEPTION_FAILED_TO_GET_THREAD_TOKEN,
                         EXCEPTION_NONCONTINUABLE,
                         0,
                         NULL);
      }
      return;
    }

    if (token_.GetHandle()) {
      RevertToSelfOrDie();
    }
  }
  ~scoped_revert_to_self() {
    if (token_.GetHandle()) {
      ImpersonateLoggedOnUserOrDie(token_.GetHandle());
    }
  }
 private:
  CAccessToken token_;
};

// Calls a function member using the security context of the process, and
// re-impersonates after the call if the calling thread previously had a valid
// thread token.
// This is particularly useful when a thread running impersonated needs to
// revert to self, call a member function, and impersonate back. If the callee
// has a result type, it returns the result of the actual call as an out
// parameter. The function crashes the process if either revert to self or
// re-impersonation fails.

// Callers for function members without arguments.
template <typename T, typename R>
R CallAsSelfAndImpersonate0(T* object, R (T::*pm)()) {
  ASSERT1(object);
  ASSERT1(pm);

  scoped_revert_to_self revert_to_self;
  return (object->*pm)();
}

// Callers for __stdcall function members with one argument.
template <typename T, typename P1, typename R>
R StdCallAsSelfAndImpersonate1(T* object, R (__stdcall T::*pm)(P1), P1 p1) {
  ASSERT1(object);
  ASSERT1(pm);

  scoped_revert_to_self revert_to_self;
  return (object->*pm)(p1);
}

// Callers for function members with one argument.
template <typename T, typename P1, typename R>
R CallAsSelfAndImpersonate1(T* object, R (T::*pm)(P1), P1 p1) {
  ASSERT1(object);
  ASSERT1(pm);

  scoped_revert_to_self revert_to_self;
  return (object->*pm)(p1);
}

// Callers for function members with two arguments.
template <class T, typename P1, typename P2, class R>
R CallAsSelfAndImpersonate2(T* object, R (T::*pm)(P1, P2), P1 p1, P2 p2) {
  ASSERT1(object);
  ASSERT1(pm);

  scoped_revert_to_self revert_to_self;
  return (object->*pm)(p1, p2);
}

// Callers for function members with three arguments.
template <class T, typename P1, typename P2, typename P3, typename R>
R CallAsSelfAndImpersonate3(T* object, R (T::*pm)(P1, P2, P3),
                            P1 p1, P2 p2, P3 p3) {
  ASSERT1(object);
  ASSERT1(pm);

  scoped_revert_to_self revert_to_self;
  return (object->*pm)(p1, p2, p3);
}

}  // namespace omaha

#endif  // OMAHA_BASE_SCOPED_IMPERSONATION_H_
