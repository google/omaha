// Copyright 2007-2010 Google Inc.
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

#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/base/time.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/oem_install_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR* const kVistaSetupStateKey =
    _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State");
const TCHAR* const kXpSystemSetupKey = _T("HKLM\\System\\Setup");

}  // namespace

class OemInstallTest : public testing::Test {
 protected:
  OemInstallTest() : cm_(ConfigManager::Instance()) {
  }

  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
    OverrideRegistryHives(kRegistryHiveOverrideRoot);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
  }

  ConfigManager* cm_;
};

class AuditModeTest : public OemInstallTest {
 protected:
  virtual void SetUp() {
    OemInstallTest::SetUp();

    if (vista_util::IsVistaOrLater()) {
      EXPECT_SUCCEEDED(RegKey::SetValue(kVistaSetupStateKey,
                                        _T("ImageState"),
                                        _T("IMAGE_STATE_UNDEPLOYABLE")));
    } else {
      EXPECT_SUCCEEDED(RegKey::SetValue(kXpSystemSetupKey,
                                        _T("AuditInProgress"),
                                        static_cast<DWORD>(1)));
    }

    EXPECT_TRUE(ConfigManager::Instance()->IsWindowsInstalling());
  }
};

TEST_F(OemInstallTest, SetOemInstallState_User) {
  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            oem_install_utils::SetOemInstallState(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, kRegValueOemInstallTimeSec));
  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

TEST_F(AuditModeTest, SetOemInstallState_User) {
  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            oem_install_utils::SetOemInstallState(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, kRegValueOemInstallTimeSec));
  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

TEST_F(OemInstallTest, SetOemInstallState_Machine) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not cover expected path because not an admin.")
               << std::endl;
  }
  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            oem_install_utils::SetOemInstallState(true));
  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

TEST_F(AuditModeTest, SetOemInstallState_Machine) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }
  EXPECT_SUCCEEDED(oem_install_utils::SetOemInstallState(true));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_TRUE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
  const uint32 install_time = GetDwordValue(MACHINE_REG_UPDATE,
                                            kRegValueOemInstallTimeSec);
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
  EXPECT_SUCCEEDED(oem_install_utils::ResetOemInstallState(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE,
                                kRegValueOemInstallTimeSec));
  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(AuditModeTest, SetOemInstallState_Machine_NeedsElevation) {
  if (vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user IS an admin.")
               << std::endl;
    return;
  }

  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            oem_install_utils::SetOemInstallState(true));
  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

//
// IsOemInstalling tests.
//

TEST_F(OemInstallTest, IsOemInstalling_Machine_Normal) {
  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest, IsOemInstalling_User_Normal) {
  EXPECT_FALSE(oem_install_utils::IsOemInstalling(false));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTimeNow_NotAuditMode) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest, IsOemInstalling_Machine_OemInstallTimeNow_AuditMode) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime71HoursAgo_NotAuditMode) {
  const DWORD kDesiredDifferenceSeconds = 71 * 60 * 60;  // 71 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GT(now_seconds, kDesiredDifferenceSeconds);
  const DWORD install_time_seconds = now_seconds - kDesiredDifferenceSeconds;
  EXPECT_LT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime71HoursAgo_AuditMode) {
  const DWORD kDesiredDifferenceSeconds = 71 * 60 * 60;  // 71 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GT(now_seconds, kDesiredDifferenceSeconds);
  const DWORD install_time_seconds = now_seconds - kDesiredDifferenceSeconds;
  EXPECT_LT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime73HoursAgo_NotAuditMode) {
  const DWORD kDesiredDifferenceSeconds = 73 * 60 * 60;  // 73 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GT(now_seconds, kDesiredDifferenceSeconds);
  const DWORD install_time_seconds = now_seconds - kDesiredDifferenceSeconds;
  EXPECT_LT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime73HoursAgo_AuditMode) {
  const DWORD kDesiredDifferenceSeconds = 73 * 60 * 60;  // 73 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GT(now_seconds, kDesiredDifferenceSeconds);
  const DWORD install_time_seconds = now_seconds - kDesiredDifferenceSeconds;
  EXPECT_LT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime71HoursInFuture_NotAuditMode) {
  const DWORD kDesiredDifferenceSeconds = 71 * 60 * 60;  // 71 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  const DWORD install_time_seconds = now_seconds + kDesiredDifferenceSeconds;
  EXPECT_GT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime71HoursInFuture_AuditMode) {
  const DWORD kDesiredDifferenceSeconds = 71 * 60 * 60;  // 71 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  const DWORD install_time_seconds = now_seconds + kDesiredDifferenceSeconds;
  EXPECT_GT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_TRUE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime73HoursInFuture_NotAuditMode) {
  const DWORD kDesiredDifferenceSeconds = 73 * 60 * 60;  // 73 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  const DWORD install_time_seconds = now_seconds + kDesiredDifferenceSeconds;
  EXPECT_GT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTime73HoursInFuture_AuditMode) {
  const DWORD kDesiredDifferenceSeconds = 73 * 60 * 60;  // 73 hours.
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  const DWORD install_time_seconds = now_seconds + kDesiredDifferenceSeconds;
  EXPECT_GT(install_time_seconds, now_seconds);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    install_time_seconds));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTimeZero_NotAuditMode) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    static_cast<DWORD>(0)));

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTimeZero_AuditMode) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    static_cast<DWORD>(0)));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTimeWrongType_NotAuditMode) {
  const uint32 now_seconds = Time64ToInt32(GetCurrent100NSTime());
  const CString now_string = itostr(now_seconds);
  EXPECT_FALSE(now_string.IsEmpty());

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_string));

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_Machine_OemInstallTimeWrongType_AuditMode) {
  const uint32 now_seconds = Time64ToInt32(GetCurrent100NSTime());
  const CString now_string = itostr(now_seconds);
  EXPECT_FALSE(now_string.IsEmpty());

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_string));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest, IsOemInstalling_Machine_NoOemInstallTime_AuditMode) {
  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(true));
}

TEST_F(OemInstallTest,
       IsOemInstalling_User_OemInstallTimeNow_NotAuditMode) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(false));
}

TEST_F(OemInstallTest, IsOemInstalling_User_OemInstallTimeNow_AuditMode) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));

  if (vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    EXPECT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  EXPECT_TRUE(cm_->IsWindowsInstalling());

  EXPECT_FALSE(oem_install_utils::IsOemInstalling(false));
}

}  // namespace omaha
