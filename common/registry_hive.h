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
//
// Other persons' registry hive manipulations.
//
#ifndef OMAHA_COMMON_REGISTRY_HIVE_H_
#define OMAHA_COMMON_REGISTRY_HIVE_H_

#include <windows.h>
#include <atlstr.h>

namespace omaha {

#define kProfileKeyFormat \
    _T("HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\%s")
#define kProfilePathValue _T("ProfileImagePath")
#define kHiveName         _T("Ntuser.dat")

// !!! Warning !!!
// You MUST unload hive, or this hive becomes unavailable until PC is restarted
// This means the person cannot login.
class RegistryHive {
 public:
  RegistryHive();
  ~RegistryHive();

  // Loads hive for requested SID. SID should be in format "S-X-X....."
  // name is a name under which the hive will be added to the HKEY_USERS,
  // you could use persons name here. This parameter should not have '\\'
  // characters!
  // It is not recommended to load more than one hive at once - LoadHive,
  // manipulate hive, UnloadHive, and then work on the next one.
  HRESULT LoadHive(TCHAR const * sid, TCHAR const *name);
  // Loads hive for requested SID, but only if the SID is another user
  // (since we don't need to do anything if the sid is ours)
  HRESULT LoadHive(TCHAR const * sid);
  // Unloads and saves loaded hive
  HRESULT UnloadHive();
  // Does it recursively. The name should be relative to HKEY_CURRENT_USER.
  HRESULT DeleteUserKey(TCHAR const * key_name);
  // Expands key name for hive:
  // "Software\Google" => "HKEY_USERS\[hive_name_]\Software\Google"
  void ExpandKeyName(CString * str);

 private:
  CString hive_name_;
  HKEY hive_holding_key_;
};

// Function pointer
// Return non-zero to abort the enumeration
typedef int ProcessUserRegistryFunc(const TCHAR* user_sid,
                                    const TCHAR* user_reg_key,
                                    LONG_PTR param);

// Enumerate all user registries
void EnumerateAllUserRegistries(ProcessUserRegistryFunc* handler,
                                LONG_PTR param);

}  // namespace omaha

#endif  // OMAHA_COMMON_REGISTRY_HIVE_H_
