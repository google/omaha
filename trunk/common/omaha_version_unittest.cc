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

#include "omaha/common/omaha_version.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(OmahaVersionTest, InitializeVersion) {
  CString version_string = GetVersionString();
  ULONGLONG version = GetVersion();

  EXPECT_STREQ(OMAHA_BUILD_VERSION_STRING, version_string);
  EXPECT_EQ(OMAHA_BUILD_VERSION, version);

  InitializeVersion(MAKEDLLVERULL(0, 0, 0, 0));
  EXPECT_STREQ(_T("0.0.0.0"), GetVersionString());
  EXPECT_EQ(0, GetVersion());

  InitializeVersion(MAKEDLLVERULL(1, 2, 3, 4));
  EXPECT_STREQ(_T("1.2.3.4"), GetVersionString());
  EXPECT_EQ(0x0001000200030004, GetVersion());

  InitializeVersion(MAKEDLLVERULL(0x7fff, 0x7fff, 0x7fff, 0x7fff));
  EXPECT_STREQ(_T("32767.32767.32767.32767"), GetVersionString());
  EXPECT_EQ(0x7fff7fff7fff7fff, GetVersion());

  InitializeVersion(MAKEDLLVERULL(0xffff, 0xffff, 0xffff, 0xffff));
  EXPECT_STREQ(_T("65535.65535.65535.65535"), GetVersionString());
  EXPECT_EQ(0xffffffffffffffff, GetVersion());

  // Sets back the initial version.
  InitializeVersion(VersionFromString(version_string));
  EXPECT_STREQ(version_string, GetVersionString());
  EXPECT_EQ(version, GetVersion());
}

}  // namespace omaha
