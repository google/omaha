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

#ifndef OMAHA_GOOGLE_UPDATE_PRECOMPILE_H__
#define OMAHA_GOOGLE_UPDATE_PRECOMPILE_H__

#pragma runtime_checks("", off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shlwapi.h>
#include <tchar.h>

#pragma warning(push)
// C4310: cast truncates constant value
#pragma warning(disable : 4310)
#include "base/basictypes.h"
#pragma warning(pop)

#endif  // OMAHA_GOOGLE_UPDATE_PRECOMPILE_H__
