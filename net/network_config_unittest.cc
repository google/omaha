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

#include <windows.h>
#include <atlconv.h>
#include <cstring>
#include "base/basictypes.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/net/cup_request.h"
#include "omaha/net/cup_utils.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_config.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class NetworkConfigTest : public testing::Test {
 protected:
  NetworkConfigTest() {}

  static void SetUpTestCase() {}

  static void TearDownTestCase() {}

  virtual void SetUp() {}

  virtual void TearDown() {}
};

TEST_F(NetworkConfigTest, GetAccessType) {
  EXPECT_EQ(NetworkConfig::GetAccessType(Config()),
            WINHTTP_ACCESS_TYPE_NO_PROXY);

  Config config;
  config.auto_detect = true;
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_AUTO_DETECT);

  config = Config();
  config.auto_config_url = _T("http://foo");
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_AUTO_DETECT);

  config = Config();
  config.auto_detect = true;
  config.proxy = _T("foo");
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_AUTO_DETECT);

  config = Config();
  config.proxy = _T("foo");
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_NAMED_PROXY);
}

TEST_F(NetworkConfigTest, CupCredentials) {
  NetworkConfig& network_config = NetworkConfig::Instance();
  EXPECT_HRESULT_SUCCEEDED(network_config.SetCupCredentials(NULL));

  CupCredentials cup_credentials1;
  EXPECT_HRESULT_FAILED(network_config.GetCupCredentials(&cup_credentials1));

  // Start with some random bytes. Persist them as sk and as B64-encoded c.
  // Read back and verify they match.
  uint8 data[20] = {0};
  EXPECT_TRUE(GenRandom(data, arraysize(data)));

  const uint8* first = data;
  const uint8* last  = data + arraysize(data);
  cup_credentials1.sk.insert(cup_credentials1.sk.begin(), first, last);
  cup_credentials1.c = cup_utils::B64Encode(data, arraysize(data));

  EXPECT_HRESULT_SUCCEEDED(network_config.SetCupCredentials(&cup_credentials1));

  CupCredentials cup_credentials2;
  EXPECT_HRESULT_SUCCEEDED(network_config.GetCupCredentials(&cup_credentials2));
  EXPECT_EQ(cup_credentials1.sk.size(), cup_credentials2.sk.size());
  EXPECT_TRUE(memcmp(&cup_credentials1.sk.front(),
                     &cup_credentials2.sk.front(),
                     cup_credentials1.sk.size()) == 0);
  EXPECT_STREQ(cup_credentials1.c, cup_credentials2.c);

  EXPECT_HRESULT_SUCCEEDED(network_config.SetCupCredentials(NULL));
  EXPECT_HRESULT_FAILED(network_config.GetCupCredentials(&cup_credentials1));
}

TEST_F(NetworkConfigTest, JoinStrings) {
  EXPECT_STREQ(NetworkConfig::JoinStrings(NULL, NULL, NULL), _T(""));

  CString result;
  EXPECT_STREQ(NetworkConfig::JoinStrings(NULL, NULL, _T("-")), _T("-"));
  EXPECT_STREQ(NetworkConfig::JoinStrings(_T("foo"), _T("bar"), _T("-")),
                                          _T("foo-bar"));
}

TEST_F(NetworkConfigTest, GetUserAgentTest) {
  CString version(GetVersionString());
  EXPECT_FALSE(version.IsEmpty());
  CString actual_user_agent(NetworkConfig::GetUserAgent());
  CString expected_user_agent;
  expected_user_agent.Format(_T("Google Update/%s"), version);
  EXPECT_STREQ(actual_user_agent, expected_user_agent);
}

// Hosts names used in the test are only used as string literals.
TEST_F(NetworkConfigTest, RemoveDuplicates) {
  // 'source' is not considered in the hash computation.
  std::vector<Config> configurations;
  Config cfg1;
  cfg1.source = "foo";
  Config cfg2;
  cfg2.source = "bar";
  configurations.push_back(cfg1);
  configurations.push_back(cfg2);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant direct connection configurations.
  Config direct_config;
  configurations.push_back(direct_config);
  configurations.push_back(direct_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant WPAD configurations.
  Config wpad_config;
  wpad_config.auto_detect = true;
  configurations.push_back(wpad_config);
  configurations.push_back(wpad_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant WPAD with auto config url configurations.
  Config wpad_url_config;
  wpad_url_config.auto_detect = true;
  wpad_url_config.auto_config_url = _T("http://www.google.com/wpad.dat");
  configurations.push_back(wpad_url_config);
  configurations.push_back(wpad_url_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant named proxy configurations.
  Config named_proxy_config;
  named_proxy_config.proxy = _T("www1.google.com:3128");
  configurations.push_back(named_proxy_config);
  configurations.push_back(named_proxy_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Does not remove distinct configurations.
  Config named_proxy_config_alt;
  named_proxy_config_alt.proxy = _T("www2.google.com:3128");
  configurations.push_back(named_proxy_config);
  configurations.push_back(named_proxy_config_alt);
  configurations.push_back(direct_config);
  configurations.push_back(wpad_config);
  configurations.push_back(wpad_url_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(5, configurations.size());
}

TEST_F(NetworkConfigTest, ParseNetConfig) {
  Config config = NetworkConfig::ParseNetConfig(_T(""));
  EXPECT_EQ(false, config.auto_detect);
  EXPECT_EQ(true,  config.auto_config_url.IsEmpty());
  EXPECT_EQ(true,  config.proxy.IsEmpty());

  config = NetworkConfig::ParseNetConfig(_T("wpad=false"));
  EXPECT_EQ(false, config.auto_detect);
  EXPECT_EQ(true,  config.auto_config_url.IsEmpty());
  EXPECT_EQ(true,  config.proxy.IsEmpty());

  config = NetworkConfig::ParseNetConfig(_T("wpad=true"));
  EXPECT_EQ(true,  config.auto_detect);
  EXPECT_EQ(true,  config.auto_config_url.IsEmpty());
  EXPECT_EQ(true,  config.proxy.IsEmpty());

  config = NetworkConfig::ParseNetConfig(_T("script=foo;proxy=bar"));
  EXPECT_EQ(false, config.auto_detect);
  EXPECT_STREQ(_T("foo"), config.auto_config_url);
  EXPECT_EQ(_T("bar"), config.proxy);

  config = NetworkConfig::ParseNetConfig(_T("proxy=foobar"));
  EXPECT_EQ(false, config.auto_detect);
  EXPECT_EQ(true, config.auto_config_url.IsEmpty());
  EXPECT_EQ(_T("foobar"), config.proxy);
}

TEST_F(NetworkConfigTest, ConfigurationOverride) {
  NetworkConfig& network_config = NetworkConfig::Instance();

  Config actual, expected;
  expected.auto_detect = true;
  network_config.SetConfigurationOverride(&expected);
  EXPECT_HRESULT_SUCCEEDED(network_config.GetConfigurationOverride(&actual));
  EXPECT_EQ(expected.auto_detect, actual.auto_detect);

  network_config.SetConfigurationOverride(NULL);
  EXPECT_EQ(E_FAIL, network_config.GetConfigurationOverride(&actual));
}

}  // namespace omaha

