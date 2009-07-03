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
//
// STA initializes a custom non-COM Single Threaded Apartment to facilitate
// calling of functions and object methods in the thread of the STA. This is
// useful when creating a simple threading model based on a main thread and
// several worker threads, especially for client components that have a UI or
// use the COM STA models.
//
// The current implementation only supports initializing the main STA, which is
// the STA created by the main thread of the process. This is usually the UI
// thread. Having multiple STA in a process is not possible yet.
//
// This custom STA does not interfere with the COM STAs.
//
// In order for the STA to work properly, the STA thread must keep processing
// messages and not block, just like in the COM STA case.

#ifndef OMAHA_COMMON_STA_H__
#define OMAHA_COMMON_STA_H__

#include <windows.h>
#include "omaha/common/debug.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

// Initializes the STA apartment. The 'reserved' parameter must be 0.
// InitializeApartment and UninitializeApartment are reference-counted.
HRESULT InitializeApartment(DWORD reserved);

// Uninitializes the STA apartment.
HRESULT UninitializeApartment();

// A scoped_sta smart pointer is provided to manage the calls to
// InitializeApartment and UninitializeApartment.
inline HRESULT smart_sta_init_helper(DWORD reserved) {
  return InitializeApartment(reserved);
}

inline void smart_uninit_helper(HRESULT result) {
  if (result == S_OK) {
    VERIFY1(SUCCEEDED(UninitializeApartment()));
  }
}

typedef close_fun<void (*)(HRESULT), smart_uninit_helper> close_sta;

typedef value_const<HRESULT, E_UNEXPECTED> sta_not_init;

typedef scoped_any<HRESULT, close_sta, sta_not_init> scoped_sta_close;

struct scoped_sta {
  explicit scoped_sta(DWORD reserved)
      : result_(smart_sta_init_helper(reserved)) {}

  HRESULT result() const { return get(result_); }

 private:
  const scoped_sta_close result_;
};

}  // namespace omaha

#endif  // OMAHA_COMMON_STA_H__

