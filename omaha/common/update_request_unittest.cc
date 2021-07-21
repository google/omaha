// Copyright 2008-2009 Google Inc.
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

#include "omaha/common/update_request.h"

#include <memory>

#include "omaha/base/reg_key.h"
#include "omaha/base/system_info.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace xml {

class UpdateRequestTest : public ::testing::TestWithParam<bool> {
 protected:
  bool IsDomain() {
    return GetParam();
  }

  void SetUp() override {
    ClearGroupPolicies();
  }

  void TearDown() override {
    ClearGroupPolicies();
  }

};

INSTANTIATE_TEST_CASE_P(IsDomain, UpdateRequestTest, ::testing::Bool());

TEST_F(UpdateRequestTest, Create_Machine) {
  std::unique_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(true, _T("unittest"), _T("unittest"), CString()));
  ASSERT_TRUE(update_request.get());
  EXPECT_TRUE(update_request->IsEmpty());
}

TEST_F(UpdateRequestTest, Create_User) {
  std::unique_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(false, _T("unittest"), _T("unittest"), CString()));
  ASSERT_TRUE(update_request.get());
  EXPECT_TRUE(update_request->IsEmpty());
}


TEST_F(UpdateRequestTest, HardwarePlatformAttributes) {
  std::unique_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(false, _T("unittest"), _T("unittest"), CString()));
  ASSERT_TRUE(update_request.get());
  EXPECT_TRUE(update_request->IsEmpty());

  const request::Request request = update_request->request();

  EXPECT_EQ(!!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE),
            request.hw.has_sse);

  if (SystemInfo::IsRunningOnXPOrLater()) {
    EXPECT_EQ(!!IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE),
            request.hw.has_sse2);
  }

  if (SystemInfo::IsRunningOnVistaOrLater()) {
    EXPECT_EQ(!!IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE),
            request.hw.has_sse3);
  }

  // Assume the test machines have at least 512 MB of physical memory.
  EXPECT_LE(1U, request.hw.physmemory);
}

TEST_P(UpdateRequestTest, DlPref) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    IsDomain() ? 1UL : 0UL));

  std::unique_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(false, _T("unittest"), _T("unittest"), CString()));
  EXPECT_STREQ(_T(""), update_request->request().dlpref);

  EXPECT_SUCCEEDED(SetPolicyString(kRegValueDownloadPreference, _T("unknown")));
  update_request.reset(
      UpdateRequest::Create(false, _T("unittest"), _T("unittest"), CString()));
  EXPECT_STREQ(_T(""), update_request->request().dlpref);

  EXPECT_SUCCEEDED(SetPolicyString(kRegValueDownloadPreference,
                                   kDownloadPreferenceCacheable));
  update_request.reset(
      UpdateRequest::Create(false, _T("unittest"), _T("unittest"), CString()));
  EXPECT_STREQ(IsDomain() ? kDownloadPreferenceCacheable : _T(""),
               update_request->request().dlpref);

  RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV, kRegValueIsEnrolledToDomain);
}

}  // namespace xml

}  // namespace omaha
