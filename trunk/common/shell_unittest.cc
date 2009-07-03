// Copyright 2003-2009 Google Inc.
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
// Shell functions

#include <shlobj.h>
#include <map>
#include "base/basictypes.h"
#include "omaha/common/dynamic_link_kernel32.h"
#include "omaha/common/file.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/shell.h"
#include "omaha/common/system_info.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(ShellTest, ShellLink) {
  CString desktop;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_DESKTOP, &desktop));
  CString link(desktop + _T("\\Shell Unittest.lnk"));
  CString install_dir;
  ASSERT_SUCCEEDED(Shell::GetSpecialFolder(CSIDL_PROGRAM_FILES,
                                           true,
                                           &install_dir));
  install_dir += _T("\\Shell Unittest");
  CString exe = install_dir + _T("\\foo.bar.exe");
  ASSERT_FALSE(File::Exists(link));
  ASSERT_SUCCEEDED(Shell::CreateLink(exe,
                                     link,
                                     install_dir,
                                     _T(""),
                                     _T("Google Update Unit Test"),
                                     'W',
                                     HOTKEYF_ALT | HOTKEYF_CONTROL,
                                     NULL));
  ASSERT_TRUE(File::Exists(link));
  ASSERT_SUCCEEDED(Shell::RemoveLink(link));
  ASSERT_FALSE(File::Exists(link));
}

struct Folders {
    DWORD csidl;
    CString name;
};

TEST(ShellTest, GetSpecialFolder) {
  Folders folders[] = {
    { CSIDL_COMMON_APPDATA,
      CString("C:\\Documents and Settings\\All Users\\Application Data") },
    { CSIDL_FONTS,
      CString("C:\\WINDOWS\\Fonts") },
    { CSIDL_PROGRAM_FILES,
      CString("C:\\Program Files") },
  };

  if (SystemInfo::IsRunningOnVistaOrLater()) {
    folders[0].name = _T("C:\\ProgramData");
  }

  // Override the program files location, which changes for 32-bit processes
  // running on 64-bit systems.
  BOOL isWow64 = FALSE;
  EXPECT_SUCCEEDED(Kernel32::IsWow64Process(GetCurrentProcess(), &isWow64));
  if (isWow64) {
    folders[2].name += _T(" (x86)");
  }

  for (size_t i = 0; i != arraysize(folders); ++i) {
    CString folder_name;
    EXPECT_SUCCEEDED(Shell::GetSpecialFolder(folders[i].csidl,
                                             false,
                                             &folder_name));
    // This should work, but CmpHelperSTRCASEEQ is not overloaded for wchars.
    // EXPECT_STRCASEEQ(folder_name, folders[i].name);
    EXPECT_EQ(folder_name.CompareNoCase(folders[i].name), 0);
  }
}

TEST(ShellTest, GetSpecialFolderKeywordsMapping) {
  typedef std::map<CString, CString> mapping;
  mapping folder_map;
  ASSERT_SUCCEEDED(Shell::GetSpecialFolderKeywordsMapping(&folder_map));
}

}  // namespace omaha

