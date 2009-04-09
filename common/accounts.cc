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
// Enumeration of the user accounts on the PC.

#include "omaha/common/accounts.h"

#include <sddl.h>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/reg_key.h"

namespace omaha {

namespace accounts {

const wchar_t kActiveProfilesKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";

HRESULT GetAllUserSids(CSimpleArray<CString> *sid_array) {
  ASSERT(sid_array, (L""));

  RegKey key_profiles;
  HRESULT hr = key_profiles.Open(HKEY_LOCAL_MACHINE, kActiveProfilesKey);
  if (FAILED(hr)) {
    return hr;
  }

  sid_array->RemoveAll();

  uint32 total_keys = key_profiles.GetSubkeyCount();

  for (uint32 i = 0 ; i < total_keys ; ++i) {
    CString possible_user_sid, name, domain;
    SID_NAME_USE user_type;

    if (SUCCEEDED(key_profiles.GetSubkeyNameAt(i, &possible_user_sid))) {
      if (SUCCEEDED(GetUserInfo(possible_user_sid, &name,
                                &domain, &user_type)) &&
          user_type == SidTypeUser) {
        sid_array->Add(possible_user_sid);
      }
    }
  }

  return hr;
}

HRESULT GetUserInfo(const wchar_t *sid_str, CString *name,
                    CString *domain, SID_NAME_USE *user_type) {
  ASSERT(sid_str, (L""));
  ASSERT(name, (L""));
  ASSERT(domain, (L""));
  ASSERT(user_type, (L""));

  PSID sid = NULL;
  HRESULT ret = E_FAIL;
  if (ConvertStringSidToSid(sid_str, &sid)) {
    DWORD name_size = 0, domain_size = 0;
    if (!LookupAccountSid(NULL, sid, NULL, &name_size, NULL,
                          &domain_size, user_type) &&
        ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
      ret = GetCurError();
      LocalFree(sid);
      return ret;
    }

    ASSERT(name_size, (L""));
    ASSERT(domain_size, (L""));
    if (!domain_size || !name_size) {
      LocalFree(sid);
      return E_UNEXPECTED;
    }

    wchar_t* c_name = new wchar_t[name_size];
    ASSERT(c_name, (L""));
    if (!c_name) {
      LocalFree(sid);
      return E_OUTOFMEMORY;
    }

    wchar_t* c_domain = new wchar_t[domain_size];
    ASSERT(c_domain, (L""));
    if (!c_domain) {
      delete[] c_name;
      LocalFree(sid);
      return E_OUTOFMEMORY;
    }

    if (LookupAccountSid(NULL, sid, c_name, &name_size, c_domain,
                         &domain_size, user_type)) {
      ret = S_OK;
      name->SetString(c_name);
      domain->SetString(c_domain);
    } else {
      ret = GetCurError();
    }

    delete[] c_name;
    delete[] c_domain;
    LocalFree(sid);
  }

  return ret;
}

}  // namespace accounts

}  // namespace omaha

