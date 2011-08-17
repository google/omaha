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
// This class finds out different user rights on the system.
// For example it can find out if the user is an administrator.

#ifndef OMAHA_BASE_USER_RIGHTS_H_
#define OMAHA_BASE_USER_RIGHTS_H_

#include <windows.h>
#include <atlsecurity.h>
#include "base/basictypes.h"

namespace omaha {

class UserRights {
 public:

  // Returns true if token is a member of the local Administrators group.
  static bool TokenIsAdmin(HANDLE token);

  // Returns true if the user belongs to the local Administrators group.
  static bool UserIsAdmin();

  // Returns true if the user belongs to the Users group.
  static bool UserIsUser();

  // Returns true if the user belongs to the Power User group.
  static bool UserIsPowerUser();

  // Returns true if the user is a Guest.
  static bool UserIsGuest();

  // Returns true if the owner of the current process has a restricted token.
  static bool UserIsRestricted();

  // Returns true if the owner of the current process runs under low or
  // untrusted integrity on Vista.
  static bool UserIsLowOrUntrustedIntegrity();

  // Returns true if the owner of the current process has an interactive
  // session: console, terminal services, or fast user switching.
  static HRESULT UserIsLoggedOnInteractively(bool* is_logged_on);

  // Gets the COM caller's impersonation token. If not in an inter-apartment COM
  // call, returns the current process token.
  static HRESULT GetCallerToken(CAccessToken* token);

  static bool VerifyCallerIsAdmin();

  static bool VerifyCallerIsSystem();

  // Returns true if the owner of the current process is the primary logon token
  // for the current interactive session: console, terminal services, or fast
  // user switching.
  static bool BelongsToGroup(HANDLE token, int group_id);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UserRights);
};

}  // namespace omaha

#endif  // OMAHA_BASE_USER_RIGHTS_H_

