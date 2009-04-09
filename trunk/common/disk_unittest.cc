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

#include <shlobj.h>
#include <psapi.h>

#include "omaha/common/app_util.h"
#include "omaha/common/const_utils.h"
#include "omaha/common/disk.h"
#include "omaha/common/file.h"
#include "omaha/common/vistautil.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(DiskTest, Disk) {
  uint64 bytes_available = 0;
  ASSERT_SUCCEEDED(GetFreeDiskSpace(_T("C:\\"), &bytes_available));

  bytes_available = 0;
  ASSERT_SUCCEEDED(GetFreeDiskSpace(CSIDL_PROGRAM_FILES, &bytes_available));

  TCHAR system_drive[MAX_PATH] = _T("%SystemDrive%");
  ASSERT_TRUE(::ExpandEnvironmentStrings(system_drive, system_drive, MAX_PATH));
  ASSERT_TRUE(::PathAddBackslash(system_drive));

  if (vista_util::IsUserAdmin()) {
    // System drive should not be hot-pluggable
    ASSERT_FALSE(IsHotPluggable(system_drive));
  }

  // System drive is expected to be > 4GB.
  ASSERT_TRUE(IsLargeDrive(system_drive));

  CString drive;
  ASSERT_SUCCEEDED(FindFirstLocalDriveWithEnoughSpace(
      kSpacePreferredToInstallDataDir, &drive));
  if (vista_util::IsUserAdmin()) {
    ASSERT_FALSE(IsHotPluggable(drive));
  }
  ASSERT_TRUE(IsLargeDrive(drive));
}

#if 0
// http://b/1076675: test fails when run on mapped drives
TEST(DiskTest, DevicePathToDosPath) {
  TCHAR image_name[MAX_PATH] = _T("");
  ASSERT_TRUE(::GetProcessImageFileName(::GetCurrentProcess(),
                                        image_name,
                                        arraysize(image_name)) != 0);

  CString dos_name;
  ASSERT_SUCCEEDED(DevicePathToDosPath(image_name, &dos_name));
  ASSERT_TRUE(File::Exists(dos_name));

  ASSERT_EQ(dos_name.CompareNoCase(app_util::GetCurrentModulePath()), 0);
}
#endif

}  // namespace omaha

