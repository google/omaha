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

#include "omaha/base/user_rights.h"
#include <lm.h>
#include <wtsapi32.h>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/system_info.h"
#include "omaha/base/vistautil.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

// Returns true if the owner of the current process is the primary logon token
// for the current interactive session: console, terminal services, or fast
// user switching.
static bool BelongsToGroup(HANDLE token, int group_id) {
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

}  // namespace

bool UserRights::TokenIsAdmin(HANDLE token) {
  return BelongsToGroup(token, DOMAIN_ALIAS_RID_ADMINS);
}

HRESULT UserRights::UserIsLoggedOnInteractively(bool* is_logged_on) {
  ASSERT1(is_logged_on);

  *is_logged_on = false;

  HRESULT hr = S_OK;

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

  PWTS_SESSION_INFOW session_info = NULL;
  const DWORD kVersion = 1;
  DWORD num_sessions = 0;
  if (!::WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE,
                              0,
                              kVersion,
                              &session_info,
                              &num_sessions)) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[WTSEnumerateSessions failed][0x%08x]"), hr));
    return hr;
  }
  ON_SCOPE_EXIT(::WTSFreeMemory, session_info);

  // Loop through all active sessions to see whether one of the sessions
  // belongs to current user. If so, regard this user as "logged-on".
  for (DWORD i = 0; i < num_sessions; ++i) {
    TCHAR* domain_name = NULL;
    DWORD domain_name_len = 0;
    if (!::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                      session_info[i].SessionId,
                                      WTSDomainName,
                                      &domain_name,
                                      &domain_name_len)) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LE, (_T("[WTSQuerySessionInformation failed][0x%08x]"), hr));
      continue;
    }
    ON_SCOPE_EXIT(::WTSFreeMemory, domain_name);

    TCHAR* user_name = NULL;
    DWORD user_name_len = 0;
    if (!::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                      session_info[i].SessionId,
                                      WTSUserName,
                                      &user_name,
                                      &user_name_len)) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LE, (_T("[WTSQuerySessionInformation failed][0x%08x]"), hr));
      continue;
    }
    ON_SCOPE_EXIT(::WTSFreeMemory, user_name);

    UTIL_LOG(L2, (_T("[ts domain=%s][ts user=%s][station=%s]"),
                  domain_name,
                  user_name,
                  session_info[i].pWinStationName));

    // Occasionally, the domain name and user name could not be retrieved when
    // the program is started just at logon time.
    if (!(domain_name && *domain_name && user_name && *user_name)) {
      hr = E_FAIL;
      continue;
    }

    if (_tcsicmp(user_info->wkui1_logon_domain, domain_name) == 0 &&
        _tcsicmp(user_info->wkui1_username, user_name) == 0) {
      *is_logged_on = true;
      return S_OK;
    }
  }

  return hr;
}

// Returns a token with TOKEN_ALL_ACCESS rights. At the moment, we only require
// TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, but requirements may change in the
// future.
HRESULT UserRights::GetCallerToken(CAccessToken* token) {
  ASSERT1(token);

  CComPtr<IUnknown> security_context;
  HRESULT hr = ::CoGetCallContext(IID_PPV_ARGS(&security_context));
  if (SUCCEEDED(hr)) {
    return token->OpenCOMClientToken(TOKEN_ALL_ACCESS) ? S_OK :
                                                         HRESULTFromLastError();
  } else if (hr != RPC_E_CALL_COMPLETE) {
    UTIL_LOG(LE, (_T("[::CoGetCallContext failed][0x%x]"), hr));
    return hr;
  }

  // RPC_E_CALL_COMPLETE indicates an in-proc intra-apartment call. Return the
  // current process token.
  return token->OpenThreadToken(TOKEN_ALL_ACCESS) ? S_OK :
                                                    HRESULTFromLastError();
}

bool UserRights::VerifyCallerIsAdmin() {
  CAccessToken impersonated_token;
  if (FAILED(GetCallerToken(&impersonated_token))) {
    return false;
  }
  return TokenIsAdmin(impersonated_token.GetHandle());
}

bool UserRights::VerifyCallerIsSystem() {
  CAccessToken impersonated_token;
  if (FAILED(GetCallerToken(&impersonated_token))) {
    return false;
  }

  CSid sid;
  if (!impersonated_token.GetUser(&sid)) {
    return false;
  }

  return sid == Sids::System();
}

}  // namespace omaha

