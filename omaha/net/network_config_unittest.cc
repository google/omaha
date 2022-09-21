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
#include <algorithm>
#include <cstring>
#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/goopdate/dm_messages.h"
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
  EXPECT_EQ(NetworkConfig::GetAccessType(ProxyConfig()),
            WINHTTP_ACCESS_TYPE_NO_PROXY);

  ProxyConfig config;
  config.auto_detect = true;
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_AUTO_DETECT);

  config = ProxyConfig();
  config.auto_config_url = _T("http://foo");
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_AUTO_DETECT);

  config = ProxyConfig();
  config.auto_detect = true;
  config.proxy = _T("foo");
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_AUTO_DETECT);

  config = ProxyConfig();
  config.proxy = _T("foo");
  EXPECT_EQ(NetworkConfig::GetAccessType(config),
            WINHTTP_ACCESS_TYPE_NAMED_PROXY);
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
  std::vector<ProxyConfig> configurations;
  ProxyConfig cfg1;
  cfg1.source = "foo";
  ProxyConfig cfg2;
  cfg2.source = "bar";
  configurations.push_back(cfg1);
  configurations.push_back(cfg2);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant direct connection configurations.
  ProxyConfig direct_config;
  configurations.push_back(direct_config);
  configurations.push_back(direct_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant WPAD configurations.
  ProxyConfig wpad_config;
  wpad_config.auto_detect = true;
  configurations.push_back(wpad_config);
  configurations.push_back(wpad_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant WPAD with auto config url configurations.
  ProxyConfig wpad_url_config;
  wpad_url_config.auto_detect = true;
  wpad_url_config.auto_config_url = _T("http://www.google.com/wpad.dat");
  configurations.push_back(wpad_url_config);
  configurations.push_back(wpad_url_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Remove redundant named proxy configurations.
  ProxyConfig named_proxy_config;
  named_proxy_config.proxy = _T("www1.google.com:3128");
  configurations.push_back(named_proxy_config);
  configurations.push_back(named_proxy_config);
  NetworkConfig::RemoveDuplicates(&configurations);
  EXPECT_EQ(1, configurations.size());
  configurations.clear();

  // Does not remove distinct configurations.
  ProxyConfig named_proxy_config_alt;
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
  ProxyConfig config = NetworkConfig::ParseNetConfig(_T(""));
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
  NetworkConfig* network_config = NULL;
  EXPECT_HRESULT_SUCCEEDED(
      NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));

  ProxyConfig actual, expected;
  expected.auto_detect = true;
  network_config->SetConfigurationOverride(&expected);
  EXPECT_HRESULT_SUCCEEDED(network_config->GetConfigurationOverride(&actual));
  EXPECT_EQ(expected.auto_detect, actual.auto_detect);

  network_config->SetConfigurationOverride(NULL);
  EXPECT_EQ(E_FAIL, network_config->GetConfigurationOverride(&actual));
}

// This test fails on and after Win 11 because ::InternetGetProxyInfo() is
// no longer supported.
TEST_F(NetworkConfigTest, DISABLED_GetProxyForUrlLocal) {
  CString pac_file_path = app_util::GetModuleDirectory(NULL);
  ASSERT_FALSE(pac_file_path.IsEmpty());
  pac_file_path.Append(_T("\\unittest_support\\localproxytest.pac"));
  ASSERT_TRUE(::PathFileExists(pac_file_path));

  HttpClient::ProxyInfo proxy_info = {};

  // The PAC file should emit a preset response for any URL with a hostname
  // matching *.omahaproxytest.com and DIRECT otherwise.

  EXPECT_HRESULT_SUCCEEDED(NetworkConfig::GetProxyForUrlLocal(
    _T("http://regex.matches.domain.omahaproxytest.com/test_url/index.html"),
    pac_file_path, &proxy_info));
  EXPECT_EQ(WINHTTP_ACCESS_TYPE_NAMED_PROXY, proxy_info.access_type);
  EXPECT_STREQ(_T("omaha_unittest1;omaha_unittest2:8080"),
               CString(proxy_info.proxy));
  EXPECT_EQ(NULL, proxy_info.proxy_bypass);

  if (proxy_info.proxy) {
    ::GlobalFree(const_cast<TCHAR*>(proxy_info.proxy));
  }
  if (proxy_info.proxy_bypass) {
    ::GlobalFree(const_cast<TCHAR*>(proxy_info.proxy_bypass));
  }

  EXPECT_HRESULT_SUCCEEDED(NetworkConfig::GetProxyForUrlLocal(
    _T("http://should.not.match.domain.example.com/test_url/index.html"),
    pac_file_path, &proxy_info));
  EXPECT_EQ(WINHTTP_ACCESS_TYPE_NO_PROXY, proxy_info.access_type);
  EXPECT_EQ(NULL, proxy_info.proxy);
  EXPECT_EQ(NULL, proxy_info.proxy_bypass);
}

TEST_F(NetworkConfigTest, ToString) {
  const CString string4096(_T('a'), 4096);

  ProxyConfig config;
  config.proxy = string4096;

  const CString expected_tostring(_T("priority=0, source=, named proxy=") +
                                  string4096 + _T(", bypass="));
  EXPECT_STREQ(expected_tostring, NetworkConfig::ToString(config));
}

// This class is parameterized for Domain, Device Management, and
// CloudPolicyOverridesPlatformPolicy using
// ::testing::TestWithParam<std::tuple<bool, bool, bool>>. The first parameter
// is the bool for Domain, the second the bool for DM (Device Management), and
// the third the bool for CloudPolicyOverridesPlatformPolicy.
class NetworkConfigPolicyTest :
    public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {
 protected:
  virtual void SetUp() {
    EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueIsEnrolledToDomain,
                                      IsDomain() ? 1UL : 0UL));
    if (IsDomain()) {
      SetPolicyString(kRegValueProxyMode, kProxyModeAutoDetect);
    }

    if (IsCloudPolicyOverridesPlatformPolicy()) {
      SetPolicy(kRegValueCloudPolicyOverridesPlatformPolicy, 1UL);
    }

    // Delete the ConfigManager instance so it is recreated, since the registry
    // entries above need to be accounted for within the ConfigManager
    // constructor.
    ConfigManager::DeleteInstance();

    if (IsDM()) {
      CachedOmahaPolicy info;
      info.is_managed = true;
      info.is_initialized = true;
      info.proxy_mode = kProxyModePacScript;
      info.proxy_pac_url = _T("https://PS/");
      ConfigManager::Instance()->SetOmahaDMPolicies(info);
    }
  }

  virtual void TearDown() {
    ConfigManager::Instance()->SetOmahaDMPolicies(CachedOmahaPolicy());
    RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
    EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                         kRegValueIsEnrolledToDomain));
    NetworkConfigManager::DeleteInstance();
  }

  bool IsDomain() {
    return std::get<0>(GetParam());
  }

  bool IsDM() {
    return std::get<1>(GetParam());
  }

  bool IsCloudPolicyOverridesPlatformPolicy() {
    return IsDomain() && std::get<2>(GetParam());
  }

  bool IsDomainPredominant() {
    return IsDomain() && (!IsCloudPolicyOverridesPlatformPolicy() || !IsDM());
  }
};

INSTANTIATE_TEST_CASE_P(IsDomainIsDMIsCloudPolicyOverridesPlatformPolicy,
                        NetworkConfigPolicyTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool(),
                                           ::testing::Bool()));

TEST_P(NetworkConfigPolicyTest, ProxyConfig) {
  NetworkConfig* network_config = NULL;
  ASSERT_HRESULT_SUCCEEDED(
      NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));

  std::vector<ProxyConfig> proxy_configurations;

  // Detect the configurations.
  ASSERT_HRESULT_SUCCEEDED(network_config->Detect());
  network_config->GetConfigurations().swap(proxy_configurations);
  if (IsDomainPredominant()) {
    EXPECT_TRUE(proxy_configurations[0].auto_detect);
  } else if (IsDM()) {
    EXPECT_STREQ(_T("https://PS/"), proxy_configurations[0].auto_config_url);
  }
}

}  // namespace omaha
