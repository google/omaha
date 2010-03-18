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

#include "omaha/common/user_info.h"
#include "omaha/testing/unit_test.h"

// We can't make any assumption about the context the unit test runs, however
// we expect the calls to succeed.
namespace omaha {

namespace {

const TCHAR kNtNonUniqueIdPrefix[] = _T("S-1-5-21-");
const int kNtNonUniqueIdPrefixLength = arraysize(kNtNonUniqueIdPrefix) - 1;

}  // namespace

TEST(UserInfoTest, GetCurrentUser) {
  CString name, domain, sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(&name, &domain, &sid));

  EXPECT_FALSE(name.IsEmpty());
  EXPECT_FALSE(domain.IsEmpty());
  EXPECT_EQ(0, sid.Find(_T("S-1-5-21-")));
}

TEST(UserInfoTest, GetCurrentUser_SidOnly) {
  CString name, domain, sid1;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(&name, &domain, &sid1));

  EXPECT_STREQ(kNtNonUniqueIdPrefix, sid1.Left(kNtNonUniqueIdPrefixLength));

  CString sid2;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &sid2));
  EXPECT_STREQ(sid1, sid2);
}


TEST(UserInfoTest, GetCurrentUserSid) {
  CSid sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetCurrentUserSid(&sid));

  const CString name = sid.AccountName();
  EXPECT_FALSE(name.IsEmpty());
  const CString sid_string = sid.Sid();
  EXPECT_STREQ(kNtNonUniqueIdPrefix,
               sid_string.Left(kNtNonUniqueIdPrefixLength));

  EXPECT_EQ(1, sid.GetPSID()->Revision);

  const SID_IDENTIFIER_AUTHORITY kNtAuthority = SECURITY_NT_AUTHORITY;
  const SID_IDENTIFIER_AUTHORITY* authority =
      sid.GetPSID_IDENTIFIER_AUTHORITY();
  for (int i = 0; i < arraysize(authority->Value); ++i) {
    EXPECT_EQ(kNtAuthority.Value[i], authority->Value[i]);
  }

  EXPECT_EQ(5, sid.GetSubAuthorityCount());

  EXPECT_EQ(SECURITY_NT_NON_UNIQUE, sid.GetSubAuthority(0));
  EXPECT_LT(static_cast<DWORD>(DOMAIN_USER_RID_MAX), sid.GetSubAuthority(4));
}

// Expect the unit tests do not run impersonated.
TEST(UserInfoTest, GetCurrentThreadUser) {
  CString thread_sid;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_NO_TOKEN),
            user_info::GetCurrentThreadUser(&thread_sid));
}

TEST(UserInfoTest, IsLocalSystemUser) {
  bool is_system = false;
  CString sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::IsLocalSystemUser(&is_system, &sid));
}

}  // namespace omaha
