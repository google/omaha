// Copyright 2007-2009 Google Inc.
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

TEST(UserInfoTest, GetCurrentUser) {
  CString name, domain, sid;
  ASSERT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(&name, &domain, &sid));
}

TEST(UserInfoTest, IsLocalSystemUser) {
  bool is_system = false;
  CString sid;
  ASSERT_HRESULT_SUCCEEDED(user_info::IsLocalSystemUser(&is_system, &sid));
}

TEST(UserInfoTest, GetCurrentUserSid) {
  CString name, domain, sid1;
  ASSERT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(&name, &domain, &sid1));
  CString sid2;
  ASSERT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &sid2));
  ASSERT_STREQ(sid1, sid2);
}

// Expect the unit tests do not run impersonated.
TEST(UserInfoTest, GetCurrentThreadUser) {
  CString thread_sid;
  ASSERT_EQ(HRESULT_FROM_WIN32(ERROR_NO_TOKEN),
            user_info::GetCurrentThreadUser(&thread_sid));
}

}  // namespace omaha

