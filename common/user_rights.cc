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

#include "omaha/common/user_rights.h"
#include <lm.h>
#include <wtsapi32.h>
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/system_info.h"
#include "omaha/common/vistautil.h"

namespace omaha {

bool UserRights::TokenIsAdmin(HANDLE token) {
  return BelongsToGroup(token, DOMAIN_ALIAS_RID_ADMINS);
}

bool UserRights::UserIsAdmin() {
  return BelongsToGroup(NULL, DOMAIN_ALIAS_RID_ADMINS);
}

bool UserRights::UserIsUser() {
  return BelongsToGroup(NULL, DOMAIN_ALIAS_RID_USERS);
}

bool UserRights::UserIsPowerUser() {
  return BelongsToGroup(NULL, DOMAIN_ALIAS_RID_POWER_USERS);
}

bool UserRights::UserIsGuest() {
  return BelongsToGroup(NULL, DOMAIN_ALIAS_RID_GUESTS);
}

bool UserRights::BelongsToGroup(HANDLE token, int group_id) {
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID group = NULL;

  BOOL check = ::AllocateAndInitializeSid(&nt_authority,
                                          2,
                                          SECURITY_BUILTIN_DOMAIN_RID,
                                          group_id,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          &group);
  if (check) {
    if (!::CheckTokenMembership(token, group, &check)) {
      check = false;
    }
    ::FreeSid(group);
  }
  return !!check;
}

bool UserRights::UserIsRestricted() {
  scoped_handle token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, address(token))) {
    UTIL_LOG(LE, (_T("[UserRights::UserIsRestricted - OpenProcessToken failed]")
                  _T("[0x%08x]"), HRESULTFromLastError()));
    return true;
  }

  return !!::IsTokenRestricted(get(token));
}

bool UserRights::UserIsLowOrUntrustedIntegrity() {
  if (SystemInfo::IsRunningOnVistaOrLater()) {
    MANDATORY_LEVEL integrity_level = MandatoryLevelUntrusted;
    if (FAILED(vista_util::GetProcessIntegrityLevel(0, &integrity_level)) ||
        integrity_level == MandatoryLevelUntrusted ||
        integrity_level == MandatoryLevelLow) {
      return true;
    }
  }

  return false;
}

HRESULT UserRights::UserIsLoggedOnInteractively(bool* is_logged_on) {
  ASSERT1(is_logged_on);

  *is_logged_on = false;

  HRESULT hr = S_OK;

  TCHAR* domain_name = NULL;
  DWORD domain_name_len = 0;
  if (!::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                    WTS_CURRENT_SESSION,
                                    WTSDomainName,
                                    &domain_name,
                                    &domain_name_len)) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[WTSQuerySessionInformation failed][0x%08x]"), hr));
    return hr;
  }
  ON_SCOPE_EXIT(::WTSFreeMemory, domain_name);

  TCHAR* user_name = NULL;
  DWORD user_name_len = 0;
  if (!::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                    WTS_CURRENT_SESSION,
                                    WTSUserName,
                                    &user_name,
                                    &user_name_len)) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[WTSQuerySessionInformation failed][0x%08x]"), hr));
    return hr;
  }
  ON_SCOPE_EXIT(::WTSFreeMemory, user_name);

  UTIL_LOG(L2, (_T("[ts domain=%s][ts user=%s]"), domain_name, user_name));

  // Occasionally, the domain name and user name could not be retrieved when
  // the program is started just at logon time.
  if (!(domain_name && *domain_name && user_name && *user_name)) {
    return E_FAIL;
  }

  // Get the user associated with the current process.
  WKSTA_USER_INFO_1* user_info = NULL;
  NET_API_STATUS status = ::NetWkstaUserGetInfo(
                              NULL,
                              1,
                              reinterpret_cast<uint8**>(&user_info));
  if (status != NERR_Success || user_info == NULL) {
    UTIL_LOG(LE, (_T("[NetWkstaUserGetInfo failed][%u]"), status));
    return HRESULT_FROM_WIN32(status);
  }
  ON_SCOPE_EXIT(::NetApiBufferFree, user_info);

  UTIL_LOG(L2, (_T("[wks domain=%s][wks user=%s]"),
                user_info->wkui1_logon_domain, user_info->wkui1_username));

  *is_logged_on = _tcsicmp(user_info->wkui1_logon_domain, domain_name) == 0 &&
                  _tcsicmp(user_info->wkui1_username, user_name) == 0;
  return S_OK;
}

HRESULT UserRights::GetCallerToken(HANDLE* token) {
  ASSERT1(token);

  scoped_handle smart_token;
  HRESULT hr = ::CoImpersonateClient();
  if (FAILED(hr)) {
    return hr;
  }

  hr = ::OpenThreadToken(::GetCurrentThread(),
                         TOKEN_QUERY,
                         true,
                         address(smart_token)) ? S_OK : HRESULTFromLastError();
  if (FAILED(hr)) {
    return hr;
  }

  hr = ::CoRevertToSelf();
  if (FAILED(hr)) {
    return hr;
  }

  *token = release(smart_token);
  return S_OK;
}

bool UserRights::ImpersonateAndVerifyCallerIsAdmin() {
  scoped_handle impersonated_token;
  if (FAILED(GetCallerToken(address(impersonated_token)))) {
    return false;
  }
  return TokenIsAdmin(get(impersonated_token));
}

}  // namespace omaha

