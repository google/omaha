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


#include "omaha/common/user_info.h"

#include <windows.h>
#include <security.h>
#include <secext.h>
#include <sddl.h>
#include <lmcons.h>
#include <atlsecurity.h>
#include "base/scoped_ptr.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

namespace user_info {

HRESULT GetCurrentUser(CString* name, CString* domain, CString* sid) {
  CAccessToken token;
  CSid current_sid;
  if (!token.GetProcessToken(TOKEN_QUERY) || !token.GetUser(&current_sid)) {
    HRESULT hr = HRESULTFromLastError();
    ASSERT(false, (_T("[Failed to get current user sid[0x%x]"), hr));
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

HRESULT IsLocalSystemUser(bool* is_local_system, CString* user_sid) {
  ASSERT1(is_local_system);

  CString sid;
  HRESULT hr = GetCurrentUser(NULL, NULL, &sid);
  if (FAILED(hr)) {
    return hr;
  }
  *is_local_system = sid.CompareNoCase(kLocalSystemSid) == 0;
  if (user_sid) {
    user_sid->SetString(sid);
  }
  return S_OK;
}

HRESULT GetCurrentThreadUser(CString* sid) {
  ASSERT1(sid);
  CAccessToken access_token;
  CSid user_sid;
  if (access_token.GetThreadToken(TOKEN_READ) &&
      access_token.GetUser(&user_sid)) {
    sid->SetString(user_sid.Sid());
    return S_OK;
  } else {
    return HRESULTFromLastError();
  }
}

}  // namespace user_info

}  // namespace omaha

