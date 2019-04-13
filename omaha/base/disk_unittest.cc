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

#include "omaha/base/disk.h"

#include <windows.h>
#include <psapi.h>

#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

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
