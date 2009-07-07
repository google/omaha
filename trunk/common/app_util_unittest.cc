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

#include <atlpath.h>
#include "omaha/common/app_util.h"
#include "omaha/common/constants.h"
#include "omaha/common/file.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/testing/unit_test.h"

const TCHAR* const kKernel32Name  = L"kernel32.dll";
const TCHAR* const kShell32Name   = L"shell32.dll";
const TCHAR* const kComCtl32Name  = L"comctl32.dll";

namespace omaha {

namespace app_util {

TEST(AppUtilTest, AppUtil) {
  HMODULE module = NULL;
  CString name;

  // First test the functionality for EXE applications.

  // Test the exe module handle.
  module = GetCurrentModuleHandle();
  EXPECT_EQ(reinterpret_cast<HMODULE>(kExeLoadingAddress), module);

  // Test the exe module handle using an address inside this module.
  struct Local {
    static void Func() {}
  };
  module = GetModuleHandleFromAddress(&Local::Func);
  EXPECT_EQ(reinterpret_cast<HMODULE>(kExeLoadingAddress), module);

  // Test the app name.
  name = GetAppName();
  EXPECT_STREQ(kUnittestName, name);

  // Test the module name.
  name = GetCurrentModuleName();
  EXPECT_STREQ(kUnittestName, name);

  // Test the app name w/o extension.
  name = GetAppNameWithoutExtension() + L".exe";
  EXPECT_STREQ(kUnittestName, name);

  // Test the module name w/o extension.
  name = GetCurrentModulePath();
  EXPECT_STREQ(GetCurrentModuleDirectory() + L"\\" + kUnittestName, name);

  // Test the module path and directory.
  name = GetCurrentModuleName();
  EXPECT_STREQ(kUnittestName, name);

  // Test that the module is an exe.
  module = GetCurrentModuleHandle();
  EXPECT_FALSE(IsModuleDll(module));
  EXPECT_FALSE(IsCurrentModuleDll());

  // Test an address.
  module = GetCurrentModuleHandle();
  EXPECT_TRUE(IsAddressInCurrentModule(module + 0x1));
  EXPECT_TRUE(IsAddressInCurrentModule(&Local::Func));
  EXPECT_FALSE(IsAddressInCurrentModule(reinterpret_cast<void*>(0x1)));

  // Test the functionality for DLL modules.
  // Use kernel32.dll

  // Get the loading address of kernel32.
  HMODULE kernel32Module = ::LoadLibrary(kKernel32Name);
  EXPECT_TRUE(kernel32Module != NULL);

  // Test the dll module handle using an address.
  module = GetModuleHandleFromAddress(::ReadFile);
  EXPECT_EQ(kernel32Module, module);

  // Check that the module is a dll.
  EXPECT_TRUE(IsModuleDll(module));

  CString system_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_SYSTEMX86, &system_path));

  // Test the dll module directory.
  name = GetModuleDirectory(module);
  EXPECT_EQ(0, name.CompareNoCase(system_path));

  // Test the dll module path.
  name = GetModulePath(module);
  EXPECT_EQ(0, name.CompareNoCase(system_path + L"\\" + kKernel32Name));

  // Test the dll module name.
  name = GetModuleName(module);
  EXPECT_EQ(0, name.CompareNoCase(kKernel32Name));

  // Other checks.
  EXPECT_FALSE(GetWindowsDir().IsEmpty());
  EXPECT_FALSE(GetHostName().IsEmpty());
  EXPECT_FALSE(GetTempDir().IsEmpty());

  // DLL versioning.
  // For the tests to succeed, shell32.dll must be loaded in memory.
  HMODULE shell32Module = ::LoadLibrary(kShell32Name);
  EXPECT_NE(0, DllGetVersion(GetSystemDir() + L"\\" + kShell32Name));
  EXPECT_NE(0, DllGetVersion(kShell32Name));
  EXPECT_NE(0, SystemDllGetVersion(kShell32Name));

  // For the tests to succeed, comctl32.dll must be loaded in memory.
  // ComCtl32 may be loaded from a side-by-side (WinSxS) directory, so it is not
  // practical to do a full-path or SystemDllGetVersion test with it.
  HMODULE comctl32_module = ::LoadLibrary(kComCtl32Name);
  EXPECT_NE(0, DllGetVersion(kComCtl32Name));

  // kernel32 does not export DllGetVersion.
  EXPECT_EQ(0, SystemDllGetVersion(kKernel32Name));

  // Module clean-up.
  EXPECT_TRUE(::FreeLibrary(comctl32_module));
  EXPECT_TRUE(::FreeLibrary(shell32Module));
  EXPECT_TRUE(::FreeLibrary(kernel32Module));
}

TEST(AppUtilTest, GetVersionFromModule) {
  EXPECT_EQ(OMAHA_BUILD_VERSION, GetVersionFromModule(NULL));
}

TEST(AppUtilTest, GetVersionFromFile) {
  CPath goopdate_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(goopdate_path.Append(kGoopdateDllName));
  ASSERT_TRUE(File::Exists(goopdate_path));

  EXPECT_EQ(OMAHA_BUILD_VERSION, GetVersionFromFile(goopdate_path));
}

}  // namespace app_util

}  // namespace omaha

