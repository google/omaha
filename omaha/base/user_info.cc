// Copyright 2004-2010 Google Inc.
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


#include "omaha/base/user_info.h"

#include <windows.h>
#include <security.h>
#include <secext.h>
#include <sddl.h>
#include <lmcons.h>
#include <atlsecurity.h>

#include "omaha/base/utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace user_info {

HRESULT GetProcessUser(CString* name, CString* domain, CString* sid) {
  CSid current_sid;

  HRESULT hr = GetProcessUserSid(&current_sid);
  if (FAILED(hr)) {
    return hr;
  }

  if (sid != NULL) {
    *sid = current_sid.Sid();
  }
  if (name != NULL) {
    *name = current_sid.AccountName();
  }
  if (domain != NULL) {
    *domain = current_sid.Domain();
  }
  return S_OK;
}

HRESULT GetProcessUserSid(CSid* sid) {
  ASSERT1(sid);

  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY) || !token.GetUser(sid)) {
    HRESULT hr = HRESULTFromLastError();

    // Assert only if thread_sid is populated. This is to eliminate other
    // reasons for GetProcessToken/GetUser to fail.
    CString thread_sid;
    ASSERT(FAILED(GetThreadUserSid(&thread_sid)),
           (_T("[Did you mean to call GetThreadUserSid?][0x%x][%s]"),
            hr, thread_sid));

    return hr;
  }

  return S_OK;
}

HRESULT IsLocalSystemUser(bool* is_local_system, CString* user_sid) {
  ASSERT1(is_local_system);

  CString sid;
  HRESULT hr = GetProcessUser(NULL, NULL, &sid);
  if (FAILED(hr)) {
    return hr;
  }
  *is_local_system = sid.CompareNoCase(kLocalSystemSid) == 0;
  if (user_sid) {
    user_sid->SetString(sid);
  }
  return S_OK;
}

HRESULT GetThreadUserSid(CString* sid) {
  ASSERT1(sid);
  CAccessToken access_token;
  CSid user_sid;
  if (access_token.GetThreadToken(TOKEN_READ) &&
      access_token.GetUser(&user_sid)) {
    sid->SetString(user_sid.Sid());
    return S_OK;
  } else {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L2, (_T("[GetThreadUserSid failed][0x%x]"), hr));
    return hr;
  }
}

HRESULT GetEffectiveUserSid(CString* sid) {
  HRESULT hr = GetThreadUserSid(sid);
  return SUCCEEDED(hr) ? hr : GetProcessUser(NULL, NULL, sid);
}

bool IsRunningAsSystem() {
  CString sid;
  return SUCCEEDED(GetEffectiveUserSid(&sid)) ? IsLocalSystemSid(sid) : false;
}

bool IsThreadImpersonating() {
  CAccessToken access_token;
  return access_token.GetThreadToken(TOKEN_READ);
}

HRESULT GetUserAccountAndDomainNames(CString* account_name,
                                     CString* domain_name) {
  ASSERT1(account_name);
  ASSERT1(domain_name);

  CString sid_string;
  HRESULT hr = GetEffectiveUserSid(&sid_string);
  if (FAILED(hr)) {
    return hr;
  }

  PSID psid = NULL;
  if (!::ConvertStringSidToSid(sid_string, &psid)) {
     return HRESULTFromLastError();
  }
  CSid csid(static_cast<const SID*>(psid));
  ::LocalFree(psid);

  domain_name->SetString(csid.Domain());
  account_name->SetString(csid.AccountName());
  return S_OK;
}

}  // namespace user_info

}  // namespace omaha

