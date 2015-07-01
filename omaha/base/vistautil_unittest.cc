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

// Tests the code returns true if Vista or later.
TEST(VistaUtilTest, IsUACMaybeOn) {
  if (!IsVistaOrLater()) {
    std::wcout << _T("\tSkipping test because not running on Vista or later.")
               << std::endl;
    return;
  }

  bool is_uac_maybe_on = false;

  bool is_split_token = false;
  if (SUCCEEDED(IsUserRunningSplitToken(&is_split_token)) && is_split_token) {
    is_uac_maybe_on = true;
  } else {
    const TCHAR* key_name = _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\")
                            _T("CurrentVersion\\Policies\\System");

    DWORD enable_lua = 0;
    is_uac_maybe_on =
        FAILED(RegKey::GetValue(key_name, _T("EnableLUA"), &enable_lua)) ||
        enable_lua;
  }

  EXPECT_EQ(is_uac_maybe_on, IsUACMaybeOn());
}

TEST(VistaUtilTest, IsElevatedWithUACMaybeOn) {
  EXPECT_EQ(IsUserAdmin() && IsVistaOrLater() && IsUACMaybeOn(),
            IsElevatedWithUACMaybeOn());
}

}  // namespace vista_util

}  // namespace omaha

