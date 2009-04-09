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

#ifndef OMAHA_COMMON_OMAHA_VERSION_H__
#define OMAHA_COMMON_OMAHA_VERSION_H__

#include <windows.h>
#include <tchar.h>

namespace omaha {

// Overloading on pointer types and integral types is not possible in this
// case and generally speaking not a good idea anyway. It leads to either
// compile time ambiguities or surprising results at runtime, such as
// which overload is being called if foobar(NULL).

// Initializes the version variables from the version resource of the module.
void InitializeVersionFromModule(HINSTANCE instance);

// Initializes the version variables from a ULONGLONG version.
void InitializeVersion(ULONGLONG version);

// Returns the version string as "major.minor.build.patch".
const TCHAR* GetVersionString();

// Returns the version string as a ULONGLONG.
ULONGLONG GetVersion();

}  // namespace omaha

#endif  // OMAHA_COMMON_OMAHA_VERSION_H__
