// Copyright 2008-2009 Google Inc.
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
#include "omaha/base/reg_key.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace vista_util {

TEST(VistaUtilTest, IsUserAdmin) {
  bool is_admin = !!::IsUserAnAdmin();
  EXPECT_EQ(is_admin, IsUserAdmin());
}

TEST(VistaUtilTest, IsUACOn) {
  if (!IsVistaOrLater()) {
    std::wcout << _T("\tSkipping test because not running on Vista or later.")
               << std::endl;
    return;
  }

  bool is_uac_on(false);
  EXPECT_HRESULT_SUCCEEDED(IsUACOn(&is_uac_on));

  const TCHAR* key_name = _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\")
                          _T("CurrentVersion\\Policies\\System");

  DWORD enable_lua = 0;
  bool lua_indicates_uac_on =
      FAILED(RegKey::GetValue(key_name, _T("EnableLUA"), &enable_lua)) ||
      enable_lua;

  EXPECT_EQ(lua_indicates_uac_on, is_uac_on);
}

TEST(VistaUtilTest, IsElevatedWithUACOn) {
  bool is_elevated_with_uac_on(false);
  VERIFY_SUCCEEDED(vista_util::IsElevatedWithUACOn(&is_elevated_with_uac_on));
  EXPECT_EQ(IsElevatedWithEnableLUAOn(), is_elevated_with_uac_on);
}

TEST(VistaUtilTest, IsEnableLUAOn) {
  if (!IsVistaOrLater()) {
    std::wcout << _T("\tSkipping test because not running on Vista or later.")
               << std::endl;
    return;
  }

  const TCHAR* key_name = _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\")
                          _T("CurrentVersion\\Policies\\System");

  DWORD enable_lua = 0;
  bool is_enable_lua_on =
      FAILED(RegKey::GetValue(key_name, _T("EnableLUA"), &enable_lua)) ||
      enable_lua;

  EXPECT_EQ(is_enable_lua_on, IsEnableLUAOn());
}

TEST(VistaUtilTest, IsElevatedWithEnableLUAOn) {
  EXPECT_EQ(IsUserAdmin() && IsVistaOrLater() && IsEnableLUAOn(),
            IsElevatedWithEnableLUAOn());
}

TEST(VistaUtilTest, SetMandatorySacl) {
  CSecurityDesc sd1;
  EXPECT_SUCCEEDED(SetMandatorySacl(MandatoryLevelLow, &sd1));
  CString sddl1;
  sd1.ToString(&sddl1, LABEL_SECURITY_INFORMATION);
  EXPECT_STREQ(LOW_INTEGRITY_SDDL_SACL, sddl1);

  CSecurityDesc sd2;
  EXPECT_SUCCEEDED(SetMandatorySacl(MandatoryLevelMedium, &sd2));
  CString sddl2;
  sd2.ToString(&sddl2, LABEL_SECURITY_INFORMATION);
  EXPECT_STREQ(MEDIUM_INTEGRITY_SDDL_SACL, sddl2);
}

}  // namespace vista_util

}  // namespace omaha

