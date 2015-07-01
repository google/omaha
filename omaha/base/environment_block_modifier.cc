// Copyright 2004-2013 Google Inc.
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

#include "omaha/base/environment_block_modifier.h"

#include "omaha/base/debug.h"
#include "omaha/base/environment_utils.h"

namespace omaha {

bool EnvironmentBlockModifier::CompareCStringCaseInsensitive::operator()(
    const CString& s1,
    const CString& s2) const {
  return s1.CompareNoCase(s2) < 0;
};

EnvironmentBlockModifier::EnvironmentBlockModifier() {}

void EnvironmentBlockModifier::SetVar(const CString& name, const CString& val) {
  ASSERT1(!name.IsEmpty());
  // We need to erase the old name explicitly.  Otherwise if the new name is
  // equal to the old name except for cases (e.g., "NAME" vs. "name"), then
  // std::map would preserve the old name.
  env_var_.erase(name);
  env_var_[name] = val;
}

bool EnvironmentBlockModifier::IsEmpty() const {
  return env_var_.empty();
}

// Assumption: strings in |env_block| are sorted case-insensitive by names.
void EnvironmentBlockModifier::Create(const TCHAR* env_block,
                                      std::vector<TCHAR>* out) const {
  ASSERT1(env_block);
  ASSERT1(out);
  // Calculate the upper bound of the resulting buffer size.
  size_t orig_size = GetEnvironmentBlockLengthInTchar(env_block);
  size_t inc_size = 0;
  std::map<CString, CString, CompareCStringCaseInsensitive>::const_iterator it;
  for (it = env_var_.begin(); it != env_var_.end(); ++it) {
    // Count separator '=' and terminating '\0'.
    if (!it->second.IsEmpty()) {
      inc_size += it->first.GetLength() + 1 + it->second.GetLength() + 1;
    }
  }
  const TCHAR* prev_read = env_block;
  const TCHAR* read = env_block;
  out->clear();
  ASSERT1(orig_size >= 1);  // Always true even for empty blocks.
  out->resize(orig_size + inc_size, _T('\0'));
  TCHAR* base = &out->front();
  TCHAR* write = base;
  for (it = env_var_.begin(); it != env_var_.end(); ++it) {
    ASSERT1(!it->first.IsEmpty());
    // Skip all |env_block| strings whose names are alphabetically less than
    // it->first, noting ParseNameFromEnvironmentString() returns "" on error.
    while (*read &&
           ParseNameFromEnvironmentString(read).CompareNoCase(it->first) < 0) {
      read += _tcslen(read) + 1;
    }
    // Write |env_block| strings that got skipped.
    memcpy(write, prev_read, (read - prev_read) * sizeof(TCHAR));
    write += read - prev_read;
    if (!it->second.IsEmpty()) {  // Empty means deletion, so skip.
      // Write name and '='
      memcpy(write, it->first.GetString(),
             it->first.GetLength() * sizeof(TCHAR));
      write += it->first.GetLength();
      *(write++) = _T('=');
      // Write value and '\0'.
      memcpy(write, it->second.GetString(),
             it->second.GetLength() * sizeof(TCHAR));
      write += it->second.GetLength();
      *(write++) = _T('\0');
    }
    // Skip |env_block| strings that got replaced or deleted.
    while (*read &&
           ParseNameFromEnvironmentString(read).CompareNoCase(it->first) == 0) {
      read += _tcslen(read) + 1;
    }
    prev_read = read;
  }
  // Copy remaining portions of |env_block|, including the terminating '\0'.
  memcpy(write, prev_read, (env_block + orig_size - prev_read) * sizeof(TCHAR));
  write += env_block + orig_size - prev_read;
  ASSERT1(write - base <= static_cast<int>(orig_size + inc_size));
}

bool EnvironmentBlockModifier::CreateForUser(HANDLE user_token,
                                             std::vector<TCHAR>* out) const {
  void* env_block = NULL;
  if (!::CreateEnvironmentBlock(&env_block, user_token, TRUE)) {
    return false;
  }

  Create(static_cast<const TCHAR*>(env_block), out);
  ::DestroyEnvironmentBlock(env_block);
  return true;
}

bool EnvironmentBlockModifier::CreateForCurrentUser(
    std::vector<TCHAR>* out) const {
  TCHAR* env_block = ::GetEnvironmentStrings();
  if (env_block == NULL) {
    return false;
  }

  Create(env_block, out);
  ::FreeEnvironmentStrings(env_block);
  return true;
}

}  // namespace omaha
