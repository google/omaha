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


#include <shlwapi.h>

#include "omaha/goopdate/main.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(MainTest, EntryPoint) {
  TCHAR path[MAX_PATH] = {0};
  ASSERT_TRUE(::GetModuleFileName(NULL, path, MAX_PATH));
  ::PathRemoveFileSpec(path);
  ASSERT_TRUE(::PathAppend(path, omaha::kGoopdateDllName));

  HMODULE module(::LoadLibraryEx(path, NULL, 0));
  ASSERT_TRUE(module);

  DllEntry dll_entry = reinterpret_cast<DllEntry>(
      ::GetProcAddress(module, omaha::kGoopdateDllEntryAnsi));
  ASSERT_TRUE(dll_entry);

  ::FreeLibrary(module);
}

}  // namespace omaha

