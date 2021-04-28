// Copyright 2009-2010 Google Inc.
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
#include "omaha/base/error.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/client/install_self_internal.h"
#include "omaha/testing/omaha_unittest.h"
#include "omaha/testing/unit_test.h"

// Overriding HKLM causes HasXmlParser() to fail on Vista.
// These tests must be run independently because other tests may use the XML
// parser, making it available despite the HKLM overriding.
// TODO(omaha): Figure out a way so the test can run together with other tests
// and then reenable.

namespace omaha {

namespace {
  bool ShouldRunRegistryProtectedTests() {
    CString path;
    if (FAILED(GetFolderPath(CSIDL_PROGRAM_FILES, &path))) {
      std::wcout << _T("\tTest skipped, running under locked down environment.")
                 << std::endl;
      return false;
    }

    if (!vista_util::IsVistaOrLater()) {
      std::wcout << _T("\tTest did not run because it requires Vista or later.")
                 << std::endl;
      return false;
    }

    return true;
  }
}  // namespace

TEST_F(RegistryProtectedTest, DISABLED_InstallOmaha_XmlParserNotPresent) {
  if (!ShouldRunRegistryProtectedTests()) {
    return;
  }

  int extra_code1 = 0;
  EXPECT_EQ(GOOPDATE_E_RUNNING_INFERIOR_MSXML,
            install_self::internal::DoInstallSelf(false,
                                                  false,
                                                  false,
                                                  RUNTIME_MODE_NOT_SET,
                                                  &extra_code1));
  EXPECT_EQ(0, extra_code1);
  EXPECT_FALSE(RegKey::HasKey(USER_REG_GOOGLE));
}

// Overriding HKLM causes HasXmlParser() to fail on Vista.
TEST_F(RegistryProtectedTest,
       DISABLED_CheckSystemRequirements_XmlParserNotPresent) {
  if (!ShouldRunRegistryProtectedTests()) {
    return;
  }

  EXPECT_EQ(GOOPDATE_E_RUNNING_INFERIOR_MSXML,
            install_self::internal::CheckSystemRequirements());
}

// Overriding HKLM causes HasXmlParser() to fail on Vista.
TEST_F(RegistryProtectedTest, DISABLED_HasXmlParser_NotPresent) {
  if (!ShouldRunRegistryProtectedTests()) {
    return;
  }

  EXPECT_FALSE(install_self::internal::HasXmlParser());
}

}  // namespace omaha
