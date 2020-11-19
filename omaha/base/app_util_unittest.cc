// Copyright 2004-2010 Google Inc.
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
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"

const TCHAR* const kKernel32Name  = L"kernel32.dll";
const TCHAR* const kShell32Name   = L"shell32.dll";
const TCHAR* const kComCtl32Name  = L"comctl32.dll";

namespace omaha {

namespace app_util {

TEST(AppUtilTest, AppUtil) {
  HMODULE module = NULL;
  CString name;
  struct Local {
    static void Func() {}
  };

  // First test the functionality for EXE applications.

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

  // Test an address.
  module = GetCurrentModuleHandle();
  EXPECT_TRUE(IsAddressInCurrentModule(module + 0x1));
  EXPECT_TRUE(IsAddressInCurrentModule(&Local::Func));
  EXPECT_FALSE(IsAddressInCurrentModule(reinterpret_cast<void*>(0x1)));

  // Test the functionality for DLL modules.
  // Use kernel32.dll

  // Get the loading address of kernel32.
  HMODULE kernel32Module = LoadSystemLibrary(kKernel32Name);
  EXPECT_TRUE(kernel32Module != NULL);

  // Test the dll module handle using an address.
  module = GetModuleHandleFromAddress(::ReadFile);
  EXPECT_EQ(kernel32Module, module);

  // Test the dll module name.
  name = GetModuleName(module);
  EXPECT_EQ(0, name.CompareNoCase(kKernel32Name));

  // Other checks.
  EXPECT_FALSE(GetWindowsDir().IsEmpty());
  EXPECT_FALSE(GetHostName().IsEmpty());
  EXPECT_FALSE(GetTempDir().IsEmpty());
  EXPECT_TRUE(String_EndsWith(GetTempDir(), _T("\\"), false));

  // DLL versioning.
  // For the tests to succeed, shell32.dll must be loaded in memory.
  HMODULE shell32Module = LoadSystemLibrary(kShell32Name);
  EXPECT_NE(0, DllGetVersion(GetSystemDir() + L"\\" + kShell32Name));
  EXPECT_NE(0, DllGetVersion(kShell32Name));
  EXPECT_NE(0, SystemDllGetVersion(kShell32Name));

  // For the tests to succeed, comctl32.dll must be loaded in memory.
  // ComCtl32 may be loaded from a side-by-side (WinSxS) directory, so it is not
  // practical to do a full-path or SystemDllGetVersion test with it.
  HMODULE comctl32_module = LoadSystemLibrary(kComCtl32Name);
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
  CPath goopdate_path(GetCurrentModuleDirectory());
  ASSERT_TRUE(goopdate_path.Append(kUnittestName));
  ASSERT_TRUE(File::Exists(goopdate_path));

  EXPECT_EQ(OMAHA_BUILD_VERSION, GetVersionFromFile(goopdate_path));
}

TEST(AppUtilTest, GetTempDirForImpersonatedOrCurrentUser) {
  // TODO(omaha3): Is there a way that we could define a user on the OS that
  // can be expected to be used by the Omaha unit tests?  It'd be nice to have
  // a more meaningful unit test for this function.

  // The behavior should be the same when the code is not running impersonated.
  EXPECT_STREQ(GetTempDir(), GetTempDirForImpersonatedOrCurrentUser());

  // The behavior should be the same when the code impersonates as self.
  EXPECT_TRUE(::ImpersonateSelf(SecurityImpersonation));
  EXPECT_STREQ(GetTempDir(), GetTempDirForImpersonatedOrCurrentUser());

  EXPECT_TRUE(::RevertToSelf());
}

TEST(AppUtilTest, GetModuleHandleFromAddress) {
  // Get the address of a function in the local module.
  HMODULE module = GetModuleHandleFromAddress(
      reinterpret_cast<void*>(GetModuleHandleFromAddress));
  EXPECT_TRUE(module);
  EXPECT_EQ(GetModuleHandle(NULL), module);

  // Get the address of a function in kernel32.
  HMODULE kernel32_module = LoadSystemLibrary(_T("kernel32"));
  ASSERT_TRUE(kernel32_module);

  HMODULE readfile_module = GetModuleHandleFromAddress(
      reinterpret_cast<void*>(::ReadFile));
  EXPECT_TRUE(readfile_module);
  EXPECT_EQ(kernel32_module, readfile_module);

  ::FreeLibrary(kernel32_module);
}

TEST(AppUtilTest, GetModuleDirectory) {
  // Check that NULL generates the same output as the current EXE's module.
  HMODULE unittest_module = GetModuleHandleFromAddress(
      reinterpret_cast<void*>(GetModuleHandleFromAddress));
  ASSERT_TRUE(unittest_module);

  CString unittest_directory = GetModuleDirectory(unittest_module);
  CString thismodule_directory = GetModuleDirectory(NULL);
  EXPECT_FALSE(unittest_directory.IsEmpty());
  EXPECT_TRUE(File::IsDirectory(unittest_directory));
  EXPECT_STREQ(unittest_directory, thismodule_directory);

  // Test against a function in kernel32, which we assume to be in system32
  // or syswow64. But do not test against functions, such as ::LoadLibrary, that
  // AppVerifier has hooks to. Otherwise GetModuleHandleFromAddress() could
  // return a module from AppVerifier when it is turned on.
  HMODULE kernel32_module = GetModuleHandleFromAddress(
      reinterpret_cast<void*>(::QueryMemoryResourceNotification));
  ASSERT_TRUE(kernel32_module);

  CString kernel32_directory = GetModuleDirectory(kernel32_module);
  EXPECT_FALSE(kernel32_directory.IsEmpty());
  EXPECT_TRUE(File::IsDirectory(kernel32_directory));

  if (Process::IsWow64(::GetCurrentProcessId())) {
    // Calling the underlying ::GetModuleFileName is unreliable under WoW. It
    // can return system32 paths when it is supposed to return syswow64 paths.
    EXPECT_TRUE(
        GetSystemWow64Dir().CompareNoCase(kernel32_directory) == 0 ||
        GetSystemDir().CompareNoCase(kernel32_directory) == 0);
  } else {
    EXPECT_STREQ(GetSystemDir().MakeLower(), kernel32_directory.MakeLower());
  }
}

TEST(AppUtilTest, GetModulePath) {
  // Check that the current EXE module matches the hardwired unit test EXE name.
  HMODULE unittest_module = GetModuleHandleFromAddress(
      reinterpret_cast<void*>(GetModuleHandleFromAddress));
  ASSERT_TRUE(unittest_module);

  CString unittest_path = GetModulePath(unittest_module);
  EXPECT_FALSE(unittest_path.IsEmpty());
  EXPECT_TRUE(File::Exists(unittest_path));
  EXPECT_STREQ(kUnittestName, GetFileFromPath(unittest_path));

  // Check that NULL generates the same output as the current EXE's module.
  CString thismodule_path = GetModulePath(NULL);
  EXPECT_STREQ(unittest_path, thismodule_path);

  // Test against a function in kernel32, which we assume to be in system32.
  HMODULE kernel32_module = GetModuleHandleFromAddress(
      reinterpret_cast<void*>(::QueryMemoryResourceNotification));
  ASSERT_TRUE(kernel32_module);

  CString kernel32_path = GetModulePath(kernel32_module);
  EXPECT_FALSE(kernel32_path.IsEmpty());
  EXPECT_TRUE(File::Exists(kernel32_path));
  EXPECT_STREQ(_T("kernel32.dll"), GetFileFromPath(kernel32_path).MakeLower());
  if (Process::IsWow64(::GetCurrentProcessId())) {
    EXPECT_TRUE(
        GetSystemWow64Dir().
            CompareNoCase(GetDirectoryFromPath(kernel32_path)) == 0 ||
        GetSystemDir().
            CompareNoCase(GetDirectoryFromPath(kernel32_path)) == 0);
  } else {
    EXPECT_STREQ(GetSystemDir().MakeLower(),
                 GetDirectoryFromPath(kernel32_path).MakeLower());
  }
}

}  // namespace app_util

}  // namespace omaha
