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
#include "omaha/base/string.h"
#include "omaha/base/system_info.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(SystemInfoTest, GetSystemVersion) {
  int major_version(0);
  int minor_version(0);
  int service_pack_major(0);
  int service_pack_minor(0);

  ASSERT_TRUE(SystemInfo::GetSystemVersion(&major_version,
                                           &minor_version,
                                           &service_pack_major,
                                           &service_pack_minor));
  EXPECT_NE(0, major_version);
  EXPECT_EQ(0, service_pack_minor);
}

TEST(SystemInfoTest, Is64BitWindows) {
  const CString arch = SystemInfo::GetArchitecture();

  if (arch == kArchIntel) {
    EXPECT_FALSE(SystemInfo::Is64BitWindows());
  } else {
    EXPECT_TRUE(SystemInfo::Is64BitWindows());
  }
}

TEST(SystemInfoTest, GetArchitecture) {
  const CString arch = SystemInfo::GetArchitecture();

  EXPECT_TRUE(arch == kArchIntel || arch == kArchAmd64) << arch;
}

TEST(SystemInfoTest, IsArchitectureSupported) {
  const CString arch = SystemInfo::GetArchitecture();

  EXPECT_TRUE(SystemInfo::IsArchitectureSupported(arch)) << arch;
}

TEST(SystemInfoTest, CompareOSVersions_SameAsCurrent) {
  OSVERSIONINFOEX this_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&this_os));

  EXPECT_TRUE(SystemInfo::CompareOSVersions(&this_os, VER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&this_os, VER_GREATER_EQUAL));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&this_os, VER_GREATER));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&this_os, VER_LESS));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&this_os, VER_LESS_EQUAL));
}

TEST(SystemInfoTest, CompareOSVersions_NewBuildNumber) {
  OSVERSIONINFOEX prior_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&prior_os));
  ASSERT_GT(prior_os.dwBuildNumber, 0UL);
  --prior_os.dwBuildNumber;

  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS_EQUAL));
}

TEST(SystemInfoTest, CompareOSVersions_NewMajor) {
  OSVERSIONINFOEX prior_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&prior_os));
  ASSERT_GT(prior_os.dwMajorVersion, 0UL);
  --prior_os.dwMajorVersion;

  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS_EQUAL));
}

TEST(SystemInfoTest, CompareOSVersions_NewMinor) {
  OSVERSIONINFOEX prior_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&prior_os));

  // This test is meaningful only if the current OS has a minor version.
  // For example, Vista does not have a minor version. Its version is 6.0.
  if (prior_os.dwMinorVersion >= 1) {
    --prior_os.dwMinorVersion;

    EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_EQUAL));
    EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER_EQUAL));
    EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER));
    EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS));
    EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS_EQUAL));
  }
}

TEST(SystemInfoTest, CompareOSVersions_NewMajorWithLowerMinor) {
  OSVERSIONINFOEX prior_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&prior_os));
  ASSERT_GT(prior_os.dwMajorVersion, 0UL);
  --prior_os.dwMajorVersion;
  ++prior_os.dwMinorVersion;

  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER_EQUAL));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS_EQUAL));
}

TEST(SystemInfoTest, CompareOSVersions_OldMajor) {
  OSVERSIONINFOEX prior_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&prior_os));
  ++prior_os.dwMajorVersion;

  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_EQUAL));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER_EQUAL));
  EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS));
  EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS_EQUAL));
}

TEST(SystemInfoTest, CompareOSVersions_OldMajorWithHigherMinor) {
  OSVERSIONINFOEX prior_os = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&prior_os));

  // This test is meaningful only if the current OS has a minor version.
  // For example, Vista does not have a minor version. Its version is 6.0.
  if (prior_os.dwMinorVersion >= 1) {
    ++prior_os.dwMajorVersion;
    --prior_os.dwMinorVersion;

    EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_EQUAL));
    EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER_EQUAL));
    EXPECT_FALSE(SystemInfo::CompareOSVersions(&prior_os, VER_GREATER));
    EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS));
    EXPECT_TRUE(SystemInfo::CompareOSVersions(&prior_os, VER_LESS_EQUAL));
  }
}

TEST(SystemInfoTest, IsRunningOnW81OrLater) {
  OSVERSIONINFOEX os_version_info = {};
  EXPECT_SUCCEEDED(SystemInfo::GetOSVersion(&os_version_info));

  if (os_version_info.dwMajorVersion > 6 ||
      (os_version_info.dwMajorVersion == 6 &&
       os_version_info.dwMinorVersion >= 3)) {
    EXPECT_TRUE(SystemInfo::IsRunningOnW81OrLater());
  } else {
    EXPECT_FALSE(SystemInfo::IsRunningOnW81OrLater());
  }
}

TEST(SystemInfoTest, GetKernel32OSVersion) {
  EXPECT_GT(SystemInfo::GetKernel32OSVersion().GetLength(), 0);

  OSVERSIONINFOEX os_version_info = {};
  EXPECT_SUCCEEDED(SystemInfo::GetOSVersion(&os_version_info));
  CString os_version;
  os_version.Format(_T("%u.%u"),
                    os_version_info.dwMajorVersion,
                    os_version_info.dwMinorVersion);
  EXPECT_TRUE(
      String_StartsWith(SystemInfo::GetKernel32OSVersion(), os_version, true));
}

TEST(SystemInfoTest, GetOSVersionType) {
  EXPECT_LT(SystemInfo::GetOSVersionType(), SUITE_LAST);
}

TEST(SystemInfoTest, GetSerialNumber) {
  EXPECT_FALSE(SystemInfo::GetSerialNumber().IsEmpty());
}

}  // namespace omaha
