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
// Enumeration of the user accounts on the PC.

#ifndef OMAHA_COMMON_ACCOUNTS_H__
#define OMAHA_COMMON_ACCOUNTS_H__

#include <windows.h>
#include <atlcoll.h>
#include <atlstr.h>

namespace omaha {

namespace accounts {

// Populates sid_array with string SIDs for all users, that have profiles
// on PC. Includes only user SIDs, no groups, computers or aliases.
HRESULT GetAllUserSids(CSimpleArray<CString>* sid_array);

// Looks up account info for given SID.
// sid - SID to look up account info for.
// On success populates:
// name - name on the account
// domain - domain name for account
// user_type - the type of the passed SID, possible values:
//   SidTypeUser
//   SidTypeGroup
//   SidTypeDomain
//   SidTypeAlias
//   SidTypeWellKnownGroup
//   SidTypeDeletedAccount
//   SidTypeInvalid
//   SidTypeUnknown
//   SidTypeComputer
HRESULT GetUserInfo(const wchar_t* sid,
                    CString* name,
                    CString* domain,
                    SID_NAME_USE* user_type);

}  // namespace accounts

}  // namespace omaha

#endif  // OMAHA_COMMON_ACCOUNTS_H__
