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

#include "omaha/base/environment_utils.h"

#include "omaha/base/debug.h"

namespace omaha {

// Cannot simply look for ['\0','\0']; this breaks if |env_block| = "".
size_t GetEnvironmentBlockLengthInTchar(const TCHAR* env_block) {
  ASSERT1(env_block);
  const TCHAR* cur = env_block;
  while (*cur) {
    cur += _tcslen(cur) + 1;
  }
  return static_cast<size_t>(cur - env_block) + 1;  // Terminating '\0'.
}

// String in the environment block may have internal values like "=C:=C:\\".
// Rather than extracting "=C;", we consider this illegal, and return "".
CString ParseNameFromEnvironmentString(const TCHAR* env_str) {
  ASSERT1(env_str);
  const TCHAR* sep = _tcschr(env_str, _T('='));
  if (sep == NULL) {  // Not found.
    return CString();
  }

  return CString(env_str, static_cast<int>(sep - env_str));
}

bool CompareEnvironmentBlock(const TCHAR* env_block1, const TCHAR* env_block2) {
  ASSERT1(env_block1);
  ASSERT1(env_block2);
  size_t len = GetEnvironmentBlockLengthInTchar(env_block1);
  if (GetEnvironmentBlockLengthInTchar(env_block2) != len) {
    return false;
  }

  return memcmp(env_block1, env_block2, len * sizeof(TCHAR)) == 0;
}

}  // namespace omaha
