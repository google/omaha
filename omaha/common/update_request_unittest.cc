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

// TODO(omaha): write tests.
// TODO(omaha): nice to mock the machine/user ids.
#include "base/scoped_ptr.h"
#include "omaha/common/update_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace xml {

class UpdateRequestTest : public testing::Test {
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(UpdateRequestTest, Create_Machine) {
  scoped_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(true, _T("unittest"), _T("unittest"), CString()));
  ASSERT_TRUE(update_request.get());
  EXPECT_TRUE(update_request->IsEmpty());
}

TEST_F(UpdateRequestTest, Create_User) {
  scoped_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(false, _T("unittest"), _T("unittest"), CString()));

  ASSERT_TRUE(update_request.get());
  EXPECT_TRUE(update_request->IsEmpty());
}

}  // namespace xml

}  // namespace omaha

