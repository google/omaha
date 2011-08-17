// Copyright 2010 Google Inc.
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

#include "omaha/base/constants.h"
#include "omaha/client/client_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace client_utils {

// TODO(omaha): These tests are of questionable value - they just check
// that the string resources for the language of your dev/build box
// matches a concatenation of the strings in main.scons.  This means
// that they're only useful for certain languages, and they add extra
// hurdles when you're attempting to customize Omaha in its open source
// form.  We should decide later whether to keep these tests at all -
// for now, we just enable them on official builds.

#ifdef GOOGLE_UPDATE_BUILD
TEST(ClientUtilsTest, GetDefaultApplicationName) {
  EXPECT_STREQ(SHORT_COMPANY_NAME _T(" Application"),
               GetDefaultApplicationName());
}

TEST(ClientUtilsTest, GetDefaultBundleName) {
  EXPECT_STREQ(SHORT_COMPANY_NAME _T(" Application"), GetDefaultBundleName());
}

TEST(ClientUtilsTest, GetUpdateAllAppsBundleName) {
  EXPECT_STREQ(SHORT_COMPANY_NAME _T(" Application"),
               GetUpdateAllAppsBundleName());
}

TEST(ClientUtilsTest, GetInstallerDisplayName_EmptyBundleName) {
  EXPECT_STREQ(SHORT_COMPANY_NAME _T(" Installer"),
               GetInstallerDisplayName(CString()));
}
#endif  // GOOGLE_UPDATE_BUILD

TEST(ClientUtilsTest, GetInstallerDisplayName_WithBundleName) {
  EXPECT_STREQ(_T("My App Installer"), GetInstallerDisplayName(_T("My App")));
}

}  // namespace client_utils

}  // namespace omaha
