// Copyright 2004-2009 Google Inc.
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

#include "omaha/base/registry_hive.h"
#include "omaha/base/accounts.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"

namespace omaha {

RegistryHive::RegistryHive()
    : hive_holding_key_(NULL) {
}

RegistryHive::~RegistryHive() {
  if ( !hive_name_.IsEmpty() ) {
    UnloadHive();
  }
}

// Loads hive for requested SID. SID should be in format "S-X-X....."
// name is a name under which the hive will be added to the HKEY_USERS,
// you could use persons name here. This parameter should not have '\\'
// characters!
// It is not recommended to load more than one hive at once - LoadHive,
// manipulate hive, UnloadHive, and then work on the next one.
//
// Hive could be already loaded: you logged out from other user but system
// had open key in your current user. In that case I open hive_holding_key_ to
// prevent system from unloading hive and do not load/unload hive - the system
// will do it.
HRESULT RegistryHive::LoadHive(TCHAR const * sid, TCHAR const *name) {
  ASSERT1(sid);
  ASSERT1(name);
  ASSERT1(hive_name_.IsEmpty());
  ASSERT1(String_FindChar(name, _T('\\')) == -1);

  CString profile_key;
  CString hive_path;

  // Need set SE_RESTORE_NAME/SE_BACKUP_NAME priveleges to current process
  // otherwise loading of the hive will fail
  RET_IF_FAILED(System::AdjustPrivilege(SE_RESTORE_NAME, true));
  RET_IF_FAILED(System::AdjustPrivilege(SE_BACKUP_NAME, true));

  SafeCStringFormat(&profile_key, kProfileKeyFormat, sid);

  if ( FAILED(RegKey::GetValue(profile_key, kProfilePathValue, &hive_path)) ) {
    return E_FAIL;
  }


  wchar_t temporary_buffer[MAX_PATH];
  DWORD ret = ExpandEnvironmentStrings(hive_path, temporary_buffer, MAX_PATH);

  if ( !ret || ret >= MAX_PATH ) {
    return E_FAIL;
  }
  hive_path = temporary_buffer;

  hive_path.Append(_T("\\"));
  hive_path.Append(kHiveName);

  hive_name_ = name;

  LONG res = RegLoadKey(HKEY_USERS, hive_name_, hive_path);

  if ( ERROR_SHARING_VIOLATION == res ) {
    // It is quite possible that the hive is still held by system.
    hive_name_ = sid;

    // if it is the case, this call will succeeed, and the system will not
    // unload the hive while there are outstanding keys opened.
    res = RegOpenKeyEx(HKEY_USERS,
                       hive_name_,
                       0,
                       KEY_ALL_ACCESS,
                       &hive_holding_key_);
  }

  return (res == ERROR_SUCCESS) ? S_OK : E_FAIL;
}

// Loads hive for requested SID, but only if the SID is another user
// (since we don't need to do anything if the sid is ours)
HRESULT RegistryHive::LoadHive(TCHAR const * user_sid) {
  ASSERT1(user_sid != NULL);
  bool other_user = false;

  // Determine if the SID passed in is really another user
  CString current_user_sid;
  HRESULT hr = omaha::user_info::GetProcessUser(NULL, NULL, &current_user_sid);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[RegistryHive::LoadHive - failed to get current user")));
    return hr;
  }

  // user_sid is the current user - no need to load the hive
  if (lstrcmpi(current_user_sid, user_sid) == 0)
    return S_FALSE;

  // Get info on the sid we're being asked to load for
  CString name;
  CString domain;
  SID_NAME_USE user_type;
  hr = accounts::GetUserInfo(user_sid, &name, &domain, &user_type);
  if ( FAILED(hr) || user_type != SidTypeUser ) {
    // Either Sid no longer exists or Sid is not a user Sid
    // (There is other possibility: Sid could be for roaming profile on domain
    // which is currently down, but we do not support roaming profiles)
    return FAILED(hr) ? hr : E_FAIL;
  }

  hr = LoadHive(user_sid, name);  // Use user name as a temporary key name.
  if ( FAILED(hr) ) {
    // Hive no longer present.
    return E_FAIL;
  }

  return S_OK;
}


// Unloads and saves loaded hive
HRESULT RegistryHive::UnloadHive() {
  if (hive_name_.IsEmpty())
    return S_OK;

  LONG res;
  if ( hive_holding_key_ ) {
    res = RegCloseKey(hive_holding_key_);
    hive_holding_key_ = NULL;
    // no need to unload hive. System will do it.
  } else {
    res = RegUnLoadKey(HKEY_USERS, hive_name_);
  }
  hive_name_.Empty();
  return (res == ERROR_SUCCESS) ? S_OK : E_FAIL;
}

// Does it recursively. The name should be relative to HKEY_CURRENT_USER.
HRESULT RegistryHive::DeleteUserKey(TCHAR const * key_name) {
  ASSERT(key_name && *key_name, (L""));
  if ( !key_name || !*key_name ) {
    return E_FAIL;
  }
  CString key(key_name);
  ExpandKeyName(&key);

  if ( !RegKey::SafeKeyNameForDeletion(key) ) {
    return E_FAIL;
  }

  return RegKey::DeleteKey(key);
}

void RegistryHive::ExpandKeyName(CString * str) {
  ASSERT1(str);

  // If we haven't loaded another user's hive, use HKCU instead of
  // HKEY_USERS
  CString key_name;
  if (hive_name_.IsEmpty()) {
    key_name = _T("HKCU\\");
  } else {
    key_name = _T("HKEY_USERS\\");
    key_name.Append(hive_name_ + _T("\\"));
  }

  key_name.Append(*str);
  *str = key_name;
}

// Load a user registry
int LoadUserRegistry(const TCHAR* user_sid,
                     ProcessUserRegistryFunc* handler,
                     LONG_PTR param) {
  ASSERT1(user_sid && *user_sid);
  ASSERT1(handler);

  // Get current user SID
  CString curr_user_sid;
  HRESULT hr = omaha::user_info::GetProcessUser(NULL, NULL, &curr_user_sid);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[LoadUserRegistry - can't get current user][0x%x]"), hr));
    return 0;
  }

  // Is current user?
  bool other_user = curr_user_sid.CompareNoCase(user_sid) != 0;

  // Get the hive for this user
  RegistryHive user_hive;
  if (other_user) {
    // Get the info about this user
    SID_NAME_USE user_type = SidTypeInvalid;
    CString name, domain;
    hr = accounts::GetUserInfo(user_sid, &name, &domain, &user_type);
    if (FAILED(hr) || user_type != SidTypeUser) {
      // Either SID no longer exists or SID is not a user Sid
      // (There is other possibility: SID could be for roaming profile on domain
      // which is currently down, but we do not support roaming profiles)
      UTIL_LOG(LEVEL_WARNING,
               (_T("[LoadUserRegistry - SID %s invalid or unsupported][0x%x]"),
                user_sid, hr));
      return 0;
    }

    // Load the hive
    hr = user_hive.LoadHive(user_sid, domain + _T("_") + name);
    if (FAILED(hr)) {
      // Hive no longer present.
      UTIL_LOG(LW, (_T("[LoadUserRegistry]")
                    _T("[hive not present for %s][0x%x]"), user_sid, hr));
      return 0;
    }
  }

  // Get the registry key path
  CString user_reg_path;
  user_hive.ExpandKeyName(&user_reg_path);

  // Call the handler
  int res = (*handler)(user_sid, user_reg_path, param);

  // Unload the hive
  if (other_user) {
    hr = user_hive.UnloadHive();
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[LoadUserRegistry]")
                    _T("[failed to save hive for %s][0x%x]"), user_sid, hr));
    }
  }

  return res;
}

// Enumerate all user registries
void EnumerateAllUserRegistries(ProcessUserRegistryFunc* handler,
                                LONG_PTR param) {
  ASSERT1(handler);

  CSimpleArray<CString> sid_array;
  accounts::GetAllUserSids(&sid_array);
  for (int i = 0 ; i < sid_array.GetSize() ; ++i) {
    if (LoadUserRegistry(sid_array[i], handler, param)) {
      return;
    }
  }
}

}  // namespace omaha

