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
//
//
// *** Documentation for OEM Installs ***
//
// A /oem install requires:
//  * Per-machine install
//  * Running as admin
//  * Running in Windows audit mode (ConfigManager::IsWindowsInstalling())
//  * Offline installer (determined in LaunchHandoffProcess())
//
// If the first three conditions are met, SetOemInstallState() writes the
// OemInstallTime registry value, which is used by IsOemInstalling() along with
// other logic to determine whether Omaha is running in an OEM install
// environment. Other objects use IsOemInstalling() - not
// ConfigureManager::IsWindowsInstalling() - to determine whether to run in a
// disabled mode for OEM factory installations. For example, the core exits
// immediately without checking for updates or Code Red events, no instances
// ping, and persistent IDs are not saved.

#include "omaha/common/oem_install_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/time.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

namespace oem_install_utils {

HRESULT SetOemInstallState(bool is_machine) {
  bool is_windows_installing = ConfigManager::Instance()->IsWindowsInstalling();
  if (!is_machine || !vista_util::IsUserAdmin() || !is_windows_installing) {
    return GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE;
  }

  const DWORD now = Time64ToInt32(GetCurrent100NSTime());

  OPT_LOG(L1, (_T("[Beginning OEM install][%u]"), now));

  goopdate_utils::DeleteUserId(is_machine);

  return RegKey::SetValue(
      ConfigManager::Instance()->machine_registry_update(),
      kRegValueOemInstallTimeSec,
      now);
}

HRESULT ResetOemInstallState(bool is_machine) {
  if (!is_machine) {
    return E_INVALIDARG;
  }

  OPT_LOG(L1, (_T("[Reset OEM install state][%u]"),
      Time64ToInt32(GetCurrent100NSTime())));

  HRESULT hr = RegKey::DeleteValue(
      ConfigManager::Instance()->machine_registry_update(),
      kRegValueOemInstallTimeSec);

  return hr;
}

// Always returns false if !is_machine. This prevents ever blocking per-user
// instances.
// Returns true if OEM install time is present and it has been less than
// kMinOemModeSec since the OEM install.
// Non-OEM installs can never be blocked from updating because OEM install time
// will not be present.
bool IsOemInstalling(bool is_machine) {
  if (!is_machine) {
    return false;
  }

  DWORD oem_install_time_seconds = 0;
  if (FAILED(RegKey::GetValue(MACHINE_REG_UPDATE,
                              kRegValueOemInstallTimeSec,
                              &oem_install_time_seconds))) {
    CORE_LOG(L3, (_T("[IsOemInstalling][OemInstallTime not found]")));
    return false;
  }

  const uint32 now_seconds = Time64ToInt32(GetCurrent100NSTime());
  if (now_seconds < oem_install_time_seconds) {
    CORE_LOG(LW, (_T("[possible time warp detected][now %u][last checked %u]"),
                  now_seconds, oem_install_time_seconds));
  }
  const int time_difference_seconds =
      abs(static_cast<int>(now_seconds - oem_install_time_seconds));

  ASSERT1(0 <= time_difference_seconds);
  const bool result = time_difference_seconds < kMinOemModeSec ? true : false;

  CORE_LOG(L3, (_T("[now %u][OEM install time %u][time difference %u][%d]"),
                now_seconds, oem_install_time_seconds, time_difference_seconds,
                result));
  return result;
}

}  // namespace oem_install_utils

}  // namespace omaha
