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
#include "omaha/base/vistautil.h"
#include "omaha/client/install_self_internal.h"
#include "omaha/testing/omaha_unittest.h"
#include "omaha/testing/unit_test.h"

// Overriding HKLM causes HasXmlParser() to fail on Vista.
// These tests must be run independently because other tests may use the XML
// parser, making it available despite the HKLM overriding.

// This test links against code that requires the following symbols but the test
// does not use them. See the TODO in the build.scons. Rather than link against
// more libs, define these symbols here.
//
// From omaha3_idl.lib.
EXTERN_C const GUID IID_IGoogleUpdate3 = GUID_NULL;
EXTERN_C const GUID IID_IAppBundle = GUID_NULL;
EXTERN_C const GUID IID_IApp = GUID_NULL;
EXTERN_C const GUID IID_IAppVersion = GUID_NULL;
EXTERN_C const GUID IID_IPackage = GUID_NULL;
EXTERN_C const GUID IID_ICurrentState = GUID_NULL;
EXTERN_C const GUID IID_IGoogleUpdate3Web = GUID_NULL;
EXTERN_C const GUID IID_IGoogleUpdate3WebSecurity = GUID_NULL;
EXTERN_C const GUID IID_IAppBundleWeb = GUID_NULL;
EXTERN_C const GUID IID_IAppWeb = GUID_NULL;
EXTERN_C const GUID IID_IAppVersionWeb = GUID_NULL;
EXTERN_C const GUID CLSID_ProcessLauncherClass = GUID_NULL;
EXTERN_C const GUID IID_IGoogleUpdate = GUID_NULL;
EXTERN_C const GUID IID_IGoogleUpdateCore = GUID_NULL;
EXTERN_C const GUID IID_IProgressWndEvents = GUID_NULL;
EXTERN_C const GUID LIBID_GoogleUpdate3Lib = GUID_NULL;
EXTERN_C const GUID IID_ICoCreateAsync = GUID_NULL;
EXTERN_C const GUID IID_ICoCreateAsyncStatus = GUID_NULL;
EXTERN_C const GUID IID_IOneClickProcessLauncher = GUID_NULL;
EXTERN_C const GUID IID_ICredentialDialog = GUID_NULL;
EXTERN_C const GUID IID_IProcessLauncher = GUID_NULL;
// From bits.lib.
EXTERN_C const GUID IID_IBackgroundCopyCallback = GUID_NULL;
// From iphlpapi.lib.
#include <iphlpapi.h>  // NOLINT
DWORD WINAPI GetIfTable(PMIB_IFTABLE, PULONG, BOOL) { return 0; }

namespace omaha {

TEST_F(RegistryProtectedTest, InstallOmaha_XmlParserNotPresent) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tTest did not run because it requires Vista or later.")
               << std::endl;
    return;
  }
  int extra_code1 = 0;
  EXPECT_EQ(GOOPDATE_E_RUNNING_INFERIOR_MSXML,
            install_self::internal::DoInstallSelf(false,
                                                  false,
                                                  false,
                                                  false,
                                                  &extra_code1));
  EXPECT_EQ(0, extra_code1);
  EXPECT_FALSE(RegKey::HasKey(USER_REG_GOOGLE));
}

// Overriding HKLM causes HasXmlParser() to fail on Vista.
TEST_F(RegistryProtectedTest, CheckSystemRequirements_XmlParserNotPresent) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tTest did not run because it requires Vista or later.")
               << std::endl;
    return;
  }
  EXPECT_EQ(GOOPDATE_E_RUNNING_INFERIOR_MSXML,
            install_self::internal::CheckSystemRequirements());
}

// Overriding HKLM causes HasXmlParser() to fail on Vista.
TEST_F(RegistryProtectedTest, HasXmlParser_NotPresent) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tTest did not run because it requires Vista or later.")
               << std::endl;
    return;
  }
  EXPECT_FALSE(install_self::internal::HasXmlParser());
}

}  // namespace omaha
