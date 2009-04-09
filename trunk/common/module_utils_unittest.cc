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
//
// Unit test for module utility functions.

#include "omaha/common/constants.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/path.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(ModuleUtilsTest, ModuleUtils) {
  // ModuleFromStatic
  HMODULE module = ModuleFromStatic(reinterpret_cast<void*>(ModuleFromStatic));
  ASSERT_TRUE(module);

  // GetModuleDirectory
  TCHAR directory1[MAX_PATH] = {0};
  TCHAR directory2[MAX_PATH] = {0};
  ASSERT_TRUE(GetModuleDirectory(module, directory1));
  ASSERT_TRUE(GetModuleDirectory(NULL, directory2));
  EXPECT_STREQ(directory1, directory2);

  // GetModuleFileName
  CString path1;
  CString path2;
  ASSERT_SUCCEEDED(GetModuleFileName(module, &path1));
  ASSERT_SUCCEEDED(GetModuleFileName(NULL, &path2));
  EXPECT_STREQ(path1, path2);

  // Verify values, as much as we can.
  CString file(GetFileFromPath(path1));
  EXPECT_STREQ(file, kUnittestName);

  CString dir(GetDirectoryFromPath(path1));
  EXPECT_STREQ(dir, directory1);
}

}  // namespace omaha
