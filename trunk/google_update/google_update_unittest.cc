// Copyright 2009 Google Inc.
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
#include "omaha/common/file.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(GoogleUpdateTest, ShellVersion) {
  CPath actual_shell_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(actual_shell_path.Append(kGoopdateFileName));

  ASSERT_TRUE(File::Exists(actual_shell_path));
  const ULONGLONG actual_shell_version =
      app_util::GetVersionFromFile(actual_shell_path);

  EXPECT_TRUE(actual_shell_version);
#if TEST_CERTIFICATE
  EXPECT_EQ(OMAHA_BUILD_VERSION, actual_shell_version);
  EXPECT_STREQ(OMAHA_BUILD_VERSION_STRING,
               StringFromVersion(actual_shell_version));
#else
  // This version must be updated whenever the saved shell is updated.
  EXPECT_STREQ(_T("1.2.183.21"), StringFromVersion(actual_shell_version));
#endif
}

}  // namespace omaha
