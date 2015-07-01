// Copyright 2009 Google Inc.
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

#include "omaha/plugins/update/site_lock.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class SiteLockTest : public testing::Test {
 protected:
  SiteLock site_lock_;
};

TEST_F(SiteLockTest, InApprovedDomain_GoogleDotCom) {
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://www.google.com/"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://www.google.com/pack/"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://www.google.co.uk"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://www.google.co.uk/pack/"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://pack.google.com/"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://pack.google.com/pack/"));
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://fake.google.com/"));
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://fake.google.com/pack/"));
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://google.com/"));
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://google.com/pack/"));
}

TEST_F(SiteLockTest, InApprovedDomain_ChromeDotCom) {
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://www.chrome.com/"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://www.chrome.com/dl/"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://chrome.com"));
  EXPECT_TRUE(site_lock_.InApprovedDomain(L"http://chrome.com/dl/"));
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://fake.chrome.com/"));
}

TEST_F(SiteLockTest, InApprovedDomain_EvilHackerDotCom) {
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://www.evilhacker.com/"));
  EXPECT_FALSE(site_lock_.InApprovedDomain(L"http://www.evilhacker.com/dl/"));
}

}  // namespace omaha
