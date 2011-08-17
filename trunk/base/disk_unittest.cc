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

#include <shlobj.h>
#include <psapi.h>

#include "omaha/base/app_util.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/disk.h"
#include "omaha/base/file.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(DiskTest, GetFreeDiskSpace) {
  uint64 bytes_available = 0;
  EXPECT_SUCCEEDED(GetFreeDiskSpace(_T("C:\\"), &bytes_available));

  bytes_available = 0;
  EXPECT_SUCCEEDED(GetFreeDiskSpace(CSIDL_PROGRAM_FILES, &bytes_available));
}

TEST(DiskTest, Disk_SystemDrive) {
  TCHAR system_drive[MAX_PATH] = _T("%SystemDrive%");
  EXPECT_TRUE(::ExpandEnvironmentStrings(system_drive, system_drive, MAX_PATH));
  EXPECT_TRUE(::PathAddBackslash(system_drive));

  if (vista_util::IsUserAdmin()) {
    // System drive should not be hot-pluggable
    EXPECT_FALSE(IsHotPluggable(system_drive));
  }

  // System drive is expected to be > 4GB.
  EXPECT_TRUE(IsLargeDrive(system_drive));
}

TEST(DiskTest, Disk_FirstLargeLocalDrive) {
  // Preferred amount of disk space for data (choose first location if found).
  // Ideally this would be 1 GB, but some test VMs have little free space.
  const int kDesiredSpace = 100 * 1000 * 1000;  // 100 MB

  CString drive;
  EXPECT_SUCCEEDED(FindFirstLocalDriveWithEnoughSpace(kDesiredSpace, &drive));
  EXPECT_TRUE(IsLargeDrive(drive));
}

// TODO(omaha): Make this work on mapped drives. See http://b/1076675.
TEST(DiskTest, DISABLED_DevicePathToDosPath) {
  TCHAR image_name[MAX_PATH] = _T("");
  EXPECT_TRUE(::GetProcessImageFileName(::GetCurrentProcess(),
                                        image_name,
                                        arraysize(image_name)) != 0);

  CString dos_name;
  EXPECT_SUCCEEDED(DevicePathToDosPath(image_name, &dos_name));
  EXPECT_TRUE(File::Exists(dos_name));

  EXPECT_EQ(dos_name.CompareNoCase(app_util::GetCurrentModulePath()), 0);
}

}  // namespace omaha
