// Copyright 2007-2010 Google Inc.
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

#include "omaha/base/constants.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/user_info.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

// We can't make any assumption about the context the unit test runs, however
// we expect the calls to succeed.
namespace omaha {

namespace {

const TCHAR kNtNonUniqueIdPrefix[] = _T("S-1-5-21-");
const int kNtNonUniqueIdPrefixLength = arraysize(kNtNonUniqueIdPrefix) - 1;

}  // namespace

TEST(UserInfoTest, GetProcessUser) {
  CString name, domain, sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetProcessUser(&name, &domain, &sid));

  EXPECT_FALSE(name.IsEmpty());
  EXPECT_FALSE(domain.IsEmpty());
  if (user_info::IsRunningAsSystem()) {
    EXPECT_STREQ(kLocalSystemSid, sid);
  } else {
    EXPECT_STREQ(kNtNonUniqueIdPrefix, sid.Left(kNtNonUniqueIdPrefixLength));
  }
}

TEST(UserInfoTest, GetProcessUser_SidOnly) {
  CString name, domain, sid1;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetProcessUser(&name, &domain, &sid1));

  if (user_info::IsRunningAsSystem()) {
    EXPECT_STREQ(kLocalSystemSid, sid1);
  } else {
    EXPECT_STREQ(kNtNonUniqueIdPrefix, sid1.Left(kNtNonUniqueIdPrefixLength));
  }

  CString sid2;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &sid2));
  EXPECT_STREQ(sid1, sid2);
}


TEST(UserInfoTest, GetProcessUserSid) {
  CSid sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetProcessUserSid(&sid));

  const CString name = sid.AccountName();
  EXPECT_FALSE(name.IsEmpty());
  const CString sid_string = sid.Sid();
  if (user_info::IsRunningAsSystem()) {
    EXPECT_STREQ(kLocalSystemSid, sid_string);
  } else {
    EXPECT_STREQ(kNtNonUniqueIdPrefix,
                 sid_string.Left(kNtNonUniqueIdPrefixLength));
  }

  EXPECT_EQ(1, sid.GetPSID()->Revision);

  const SID_IDENTIFIER_AUTHORITY kNtAuthority = SECURITY_NT_AUTHORITY;
  const SID_IDENTIFIER_AUTHORITY* authority =
      sid.GetPSID_IDENTIFIER_AUTHORITY();
  for (int i = 0; i < arraysize(authority->Value); ++i) {
    EXPECT_EQ(kNtAuthority.Value[i], authority->Value[i]);
  }

  UCHAR expected_auth_count = user_info::IsRunningAsSystem() ?  1 : 5;
  EXPECT_EQ(expected_auth_count, sid.GetSubAuthorityCount());

  DWORD expected_authority = user_info::IsRunningAsSystem() ?
                                 SECURITY_LOCAL_SYSTEM_RID :
                                 SECURITY_NT_NON_UNIQUE;
  EXPECT_EQ(expected_authority, sid.GetSubAuthority(0));
}

// Expect the unit tests do not run impersonated.
TEST(UserInfoTest, GetThreadUserSid) {
  CString thread_sid;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_NO_TOKEN),
            user_info::GetThreadUserSid(&thread_sid));
}

// Expect the unit tests do not run impersonated.
// TODO(omaha3): Assuming we are running as admin, is there anything we can
// impersonate so that this important path gets tested?
TEST(UserInfoTest, GetEffectiveUserSid) {
  CString thread_sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetEffectiveUserSid(&thread_sid));
  CSid process_sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetProcessUserSid(&process_sid));
  EXPECT_STREQ(process_sid.Sid(), thread_sid);
}

TEST(UserInfoTest, IsLocalSystemUser) {
  bool is_system = false;
  CString sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::IsLocalSystemUser(&is_system, &sid));
}

TEST(UserInfoTest, IsThreadImpersonating) {
  EXPECT_FALSE(user_info::IsThreadImpersonating());

  scoped_handle process_token;
  EXPECT_NE(0, ::OpenProcessToken(GetCurrentProcess(),
                                  TOKEN_ALL_ACCESS,
                                  address(process_token)));

  scoped_handle restricted_token;
  EXPECT_NE(0, ::CreateRestrictedToken(get(process_token),
                                       DISABLE_MAX_PRIVILEGE,
                                       0, NULL,
                                       0, NULL,
                                       0, NULL,
                                       address(restricted_token)));

  scoped_impersonation impersonate_user(get(restricted_token));

  EXPECT_TRUE(user_info::IsThreadImpersonating());
}

// Tests that the GetUserAccountAndDomainNames function succeeds and at least
// it returns something.
TEST(UserInfoTest, GetUserAccountAndDomainNames) {
  CString account_name;
  CString domain_name;
  EXPECT_HRESULT_SUCCEEDED(
      user_info::GetUserAccountAndDomainNames(&account_name, &domain_name));
  EXPECT_FALSE(account_name.IsEmpty());
}

}  // namespace omaha
