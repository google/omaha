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

#include "omaha/worker/application_usage_data.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

ApplicationUsageData::ApplicationUsageData(bool is_machine,
                                           bool check_low_integrity)
    : exists_(false),
      did_run_(false),
      is_machine_(is_machine),
      is_pre_update_check_(true),
      check_low_integrity_(check_low_integrity) {
}

ApplicationUsageData::~ApplicationUsageData() {
}

HRESULT ApplicationUsageData::ReadDidRun(const CString& app_guid) {
  CORE_LOG(L4, (_T("[ApplicationUsageData::ReadDidRun][%s]"), app_guid));
  is_pre_update_check_ = true;
  return ProcessDidRun(app_guid);
}

HRESULT ApplicationUsageData::ResetDidRun(const CString& app_guid) {
  CORE_LOG(L4, (_T("[ApplicationUsageData::ResetDidRun][%s]"), app_guid));
  is_pre_update_check_ = false;
  return ProcessDidRun(app_guid);
}

HRESULT ApplicationUsageData::ProcessDidRun(const CString& app_guid) {
  CORE_LOG(L4, (_T("[ApplicationUsageData::ProcessDidRun][%s]"), app_guid));
  return is_machine_ ? ProcessMachineDidRun(app_guid) :
                       ProcessUserDidRun(app_guid);
}

HRESULT ApplicationUsageData::ProcessMachineDidRun(const CString& app_guid) {
  ASSERT1(is_machine_);

  // Logic is as follows:
  // for each user under HKU\<sid>
  //   pre/post process HKU\<sid>
  //   if vista
  //     pre/post process HKU\<lowintegrity IE>\<sid>
  // pre/post process HKLM
  RegKey users_key;
  HRESULT hr = users_key.Open(USERS_KEY, KEY_READ);
  if (SUCCEEDED(hr)) {
    uint32 num_users = users_key.GetSubkeyCount();
    for (uint32 i = 0; i < num_users; ++i) {
      CString sub_key_name;
      hr = users_key.GetSubkeyNameAt(i, &sub_key_name);
      if (FAILED(hr)) {
        CORE_LOG(LEVEL_WARNING, (_T("[Key enum failed.][0x%08x][%d][%s]"),
                                 hr, i, USERS_KEY));
        continue;
      }

      CString temp_key = AppendRegKeyPath(USERS_KEY,
                                          sub_key_name,
                                          GOOPDATE_REG_RELATIVE_CLIENT_STATE);
      CString user_state_key_name = AppendRegKeyPath(temp_key, app_guid);
      hr = ProcessKey(user_state_key_name);
      if (FAILED(hr)) {
        CORE_LOG(L4, (_T("[ProcessKey failed][%s][0x%08x]"), app_guid, hr));
      }

      if (check_low_integrity_) {
        // If we are running on vista we need to also look at the low
        // integrity IE key where IE can write to. Note that we cannot
        // use the IEGetWriteableHKCU function since this function assumes
        // that we are running with the user's credentials.
        CString temp_key = AppendRegKeyPath(USERS_KEY,
                                            sub_key_name,
                                            USER_REG_VISTA_LOW_INTEGRITY_HKCU);
        CString li_hkcu_name = AppendRegKeyPath(
                                   AppendRegKeyPath(
                                       temp_key,
                                       sub_key_name,
                                       GOOPDATE_REG_RELATIVE_CLIENT_STATE),
                                   app_guid);
        hr = ProcessKey(li_hkcu_name);
        if (FAILED(hr)) {
          CORE_LOG(L4, (_T("[ProcessKey failed][%s][0x%08x]"), app_guid, hr));
        }
      }
    }  // End of for

    // Now Process the machine did run value also.
    CString machine_state_key_name =
        goopdate_utils::GetAppClientStateKey(true, app_guid);
    hr = ProcessBackWardCompatKey(machine_state_key_name);
    if (FAILED(hr)) {
      CORE_LOG(L4, (_T("[ProcessBackWardCompatKey failed][0x%08x][%s]"),
                    hr, machine_state_key_name));
    }
  } else {
    CORE_LOG(LW, (_T("[Key open failed.][0x%08x][%s]"), hr, USERS_KEY));
  }

  return S_OK;
}

HRESULT ApplicationUsageData::ProcessUserDidRun(const CString& app_guid) {
  ASSERT1(!is_machine_);

  // Logic:
  // Pre/Post process HKCU\
  // if vista:
  //    Pre/Post process HKCU\LowIntegrity
  CString state_key_name = goopdate_utils::GetAppClientStateKey(false,
                                                                app_guid);
  HRESULT hr = ProcessKey(state_key_name);
  if (FAILED(hr)) {
      CORE_LOG(L4, (_T("[ProcessKey failed][0x%08x][%s]"),
                    hr, state_key_name));
  }

  if (check_low_integrity_) {
    // If we are running on vista we need to also look at the low
    // integrity IE key where IE can write to. To avoid loading
    // ieframe.dll into our process, we just use the registry
    // key location directly instead of using IEGetWriteableHKCU
    CString sid;
    hr = user_info::GetCurrentUser(NULL, NULL, &sid);
    if (FAILED(hr)) {
      CORE_LOG(LEVEL_WARNING, (_T("[GetCurrentUser failed][0x%08x][%s]"),
                               hr, app_guid));
      return hr;
    }

    CString temp_name = AppendRegKeyPath(USER_KEY_NAME,
                                         USER_REG_VISTA_LOW_INTEGRITY_HKCU,
                                         sid);
    CString lowintegrity_hkcu_name = AppendRegKeyPath(
                                         temp_name,
                                         GOOPDATE_REG_RELATIVE_CLIENT_STATE,
                                         app_guid);
    hr = ProcessKey(lowintegrity_hkcu_name);
    if (FAILED(hr)) {
      CORE_LOG(LEVEL_WARNING, (_T("[Could not ProcessKey][0x%08x][%s]"),
                               hr, app_guid));
    }
  }

  return S_OK;
}

HRESULT ApplicationUsageData::ProcessKey(const CString& key_name) {
  return is_pre_update_check_ ? ProcessPreUpdateCheck(key_name) :
                                ProcessPostUpdateCheck(key_name);
}

HRESULT ApplicationUsageData::ProcessPreUpdateCheck(const CString& key_name) {
  // Read in the regkey value if it exists, and or it with the previous value.
  RegKey key;
  HRESULT hr = key.Open(key_name, KEY_READ);
  if (FAILED(hr)) {
    CORE_LOG(L4, (_T("[failed to open key][%s][0x%08x]"), key_name, hr));
    return hr;
  }

  // Now that we have the key, we should try and read the value of the
  // did run key.
  CString did_run_str(_T("0"));
  hr = key.GetValue(kRegValueDidRun, &did_run_str);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[RegKey::GetValue failed][0x%08x][%s][%s]"),
                  hr, key_name, kRegValueDidRun));
    return hr;
  }

  if (did_run_str == _T("1")) {
    did_run_ |= true;
  }
  exists_ |= true;

  return hr;
}

HRESULT ApplicationUsageData::ProcessBackWardCompatKey(
    const CString& key_name) {
  // This method exists to support the installers that have not been
  // updated to write to the HKCU key. Remove when we have all the installers
  // correcly updated.
  if (is_pre_update_check_) {
    // Read in the regkey value if it exists, and or it with the previous value.
    RegKey key;
    HRESULT hr = key.Open(key_name, KEY_READ);
    if (FAILED(hr)) {
      CORE_LOG(L4, (_T("[failed to open key][%s][0x%08x]"), key_name, hr));
      return hr;
    }

    // Now that we have the key, we should try and read the value of the
    // did run key.
    CString did_run_str(_T("0"));
    hr = key.GetValue(kRegValueDidRun, &did_run_str);
    if (FAILED(hr)) {
      CORE_LOG(L3, (_T("[RegKey::GetValue failed][0x%08x][%s][%s]"),
                    hr, key_name, kRegValueDidRun));
      return hr;
    }

    if (did_run_str == _T("1")) {
      did_run_ |= true;
    }
    exists_ |= true;

    return hr;
  } else {
    RegKey key;
    HRESULT hr = key.Open(key_name);
    if (FAILED(hr)) {
      CORE_LOG(L4, (_T("[failed to open key][%s][0x%08x]"), key_name, hr));
      return hr;
    }

    // If the value exists, then it means that the installer has been updated,
    // and we delete the machine value.
    if (exists_) {
      hr = RegKey::DeleteValue(key_name, kRegValueDidRun);
      if (FAILED(hr)) {
        CORE_LOG(LEVEL_WARNING, (_T("[RegKey::DeleteValue failed][0x%08x][%s]"),
                                 hr, key_name));
        return hr;
      }
    } else {
      // Since the value does not exist else where, we reset the value in the
      // HKLM key to zero.
      exists_ |= true;
      CString did_run_str(_T("0"));
      if (SUCCEEDED(key.GetValue(kRegValueDidRun, &did_run_str))) {
        hr = key.SetValue(kRegValueDidRun, _T("0"));
        if (FAILED(hr)) {
          CORE_LOG(LEVEL_WARNING, (_T("[RegKey::SetValue failed][0x%08x][%s]"),
                                   hr, key_name));
          return hr;
        }
      }
    }
  }

  return S_OK;
}

HRESULT ApplicationUsageData::ProcessPostUpdateCheck(const CString& key_name) {
  RegKey key;
  HRESULT hr = key.Open(key_name);
  if (FAILED(hr)) {
    CORE_LOG(L4, (_T("[failed to open key][%s][0x%08x]"), key_name, hr));
    return hr;
  }

  CString did_run_str(_T("0"));
  if (SUCCEEDED(key.GetValue(kRegValueDidRun, &did_run_str))) {
    exists_ |= true;
    hr = key.SetValue(kRegValueDidRun, _T("0"));
    if (FAILED(hr)) {
      CORE_LOG(LEVEL_WARNING, (_T("[RegKey::SetValue failed][0x%08x][%s]"),
                               hr, key_name));
      return hr;
    }
  }
  return S_OK;
}

}  // namespace omaha
