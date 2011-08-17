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
//
// Tests the constants that vary depending on the customization of Omaha.
// The test checks for the Google Update variations, but can be modified for
// your purposes.

#include "omaha/plugins/update/config.h"
#include "omaha/testing/omaha_customization_test.h"

namespace omaha {

TEST(OmahaCustomizationUpdateTest, Constants_BuildFiles) {
// The plugin version may or may not match in non-Google Update builds.
#ifdef GOOGLE_UPDATE_BUILD
  EXPECT_STREQ(_T("3"), kUpdate3WebPluginVersion);
  EXPECT_STREQ(_T("9"), kOneclickPluginVersion);
#else
  std::wcout << _T("Did not test kPluginVersions.") << std::endl;
#endif

  EXPECT_GU_STREQ(
      _T("Google.Update3WebControl.") _T(UPDATE_PLUGIN_VERSION_ANSI),
      kUpdate3WebControlProgId);
  EXPECT_GU_STREQ(
      _T("Google.OneClickCtrl.") _T(ONECLICK_PLUGIN_VERSION_ANSI),
      kOneclickControlProgId);
}

}  // namespace omaha
