// Copyright 2006-2013 Google Inc.
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
// Utility functions for processing environment blocks.

#ifndef OMAHA_BASE_ENVIRONMENT_UTILS_H_
#define OMAHA_BASE_ENVIRONMENT_UTILS_H_

#include <windows.h>
#include <tchar.h>
#include <atlstr.h>

namespace omaha {

// Returns the number of TCHAR characters used to store |env_block|, counting
// the extra _T('\0') at the end.
// Multiply the result by sizeof(TCHAR) to get number of bytes.
size_t GetEnvironmentBlockLengthInTchar(const TCHAR* env_block);

// Parses "name" from "name=value".  Returns empty string on error.
CString ParseNameFromEnvironmentString(const TCHAR* env_str);

// Returns true if the two environment blocks are equal.
bool CompareEnvironmentBlock(const TCHAR* env_block1, const TCHAR* env_block2);

}  // namespace omaha

#endif  // OMAHA_BASE_ENVIRONMENT_UTILS_H_
