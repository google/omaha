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

#include <atlstr.h>
#include "omaha/common/system_info.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(SystemInfoTest, SystemInfo) {
  SystemInfo::OSVersionType os_type = SystemInfo::OS_WINDOWS_UNKNOWN;
  DWORD service_pack = 0;
  ASSERT_SUCCEEDED(SystemInfo::CategorizeOS(&os_type, &service_pack));
}

TEST(SystemInfoTest, GetSystemVersion) {
  int major_version(0);
  int minor_version(0);
  int service_pack_major(0);
  int service_pack_minor(0);

  CString name;
  ASSERT_TRUE(SystemInfo::GetSystemVersion(&major_version,
                                           &minor_version,
                                           &service_pack_major,
                                           &service_pack_minor,
                                           CStrBuf(name, MAX_PATH),
                                           MAX_PATH));
  EXPECT_NE(0, major_version);
  EXPECT_EQ(0, service_pack_minor);

  EXPECT_FALSE(name.IsEmpty());
}

}  // namespace omaha

