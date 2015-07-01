// Copyright 2004-2009 Google Inc.
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

#ifndef OMAHA_BASE_CONST_DEBUG_H_
#define OMAHA_BASE_CONST_DEBUG_H_

#include <tchar.h>
#include "omaha/base/constants.h"

namespace omaha {

// kCiDebugDirectory is relative to the system drive if available or the
// current directory otherwise. The expectation is that %SystemDrive% is
// always avaialable, including for the code running as system.
#define kCiDebugDirectory     APP_NAME_IDENTIFIER _T("-debug")

// TODO(omaha): unify the debugging and logging support so that these files
// are created under the same directory as the log file.
#define kCiDebugLogFile                   _T("debug.log")
#define kCiAssertOccurredFile             _T("assert.log")
#define kCiAbortOccurredFile              _T("abort.log")

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_DEBUG_H_
