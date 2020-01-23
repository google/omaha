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

#include "omaha/net/detector.h"

#include <cstdio>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/goopdate/dm_messages.h"
#include "omaha/net/network_config.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// This class is parameterized for Managed and whether the tests use the Device
// Management (DM) proxy detector. The first parameter is a bool that indicates
// "Managed", which when true means that the tests run with Policy turned on.
// The second parameter indicates whether the tests are using the DM proxy
// detector class or the GPO proxy detector class.
class GroupPolicyProxyDetectorTest
    : public testing::TestWithParam<std::tuple<bool, bool>>  {
 protected:
  GroupPolicyProxyDetectorTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);

    EXPECT_SUCCEEDED(RegKey::SetValue(
        MACHINE_REG_UPDATE_DEV,
        kRegValueIsEnrolledToDomain,
        IsManaged() && !IsUsingDMProxyDetector() ? 1UL : 0UL));

    if (IsManaged() && !IsUsingDMProxyDetector()) {
      RegKey::CreateKey(kRegKeyGoopdateGroupPolicy);
    } else {
      RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
    }

    detector_.reset(IsUsingDMProxyDetector() ? new DMProxyDetector :
                                               new GroupPolicyProxyDetector);
  }

  virtual void TearDown() {
    ResetCachedOmahaPolicy();
    RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
    EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                         kRegValueIsEnrolledToDomain));

    RestoreRegistryHives();
    EXPECT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }

  bool IsManaged() {
    return std::get<0>(GetParam());
  }

  bool IsUsingDMProxyDetector() {
    return std::get<1>(GetParam());
  }

  CString GetSourceString() {
    const TCHAR kGpoSourceString[] = _T("GroupPolicy");
    const TCHAR kDMSourceString[] = _T("DeviceManagement");

    if (!IsManaged()) {
      return CString();
    }

    return IsUsingDMProxyDetector() ? kDMSourceString : kGpoSourceString;
  }

  void SetValidGroupPolicyValue(const CString& entry, const CString& val) {
    if (val.IsEmpty()) {
      return;
    }

    EXPECT_SUCCEEDED(RegKey::SetValue(kRegKeyGoopdateGroupPolicy, entry, val));
  }

  void SetPolicy(const CString& proxy_mode,
                 const CString& proxy_pac_url,
                 const CString& proxy_server) {
    if (IsManaged() && IsUsingDMProxyDetector()) {
      CachedOmahaPolicy info;
      info.is_initialized = true;
      info.proxy_mode = proxy_mode;
      info.proxy_pac_url = proxy_pac_url;
      info.proxy_server = proxy_server;
      ConfigManager::Instance()->SetOmahaDMPolicies(info);
    } else {
      SetValidGroupPolicyValue(kRegValueProxyMode, proxy_mode);
      SetValidGroupPolicyValue(kRegValueProxyPacUrl, proxy_pac_url);
      SetValidGroupPolicyValue(kRegValueProxyServer, proxy_server);
    }
  }

  void ResetCachedOmahaPolicy() {
    ConfigManager::Instance()->SetOmahaDMPolicies(CachedOmahaPolicy());
  }

  CString hive_override_key_name_;
  std::unique_ptr<GroupPolicyProxyDetector> detector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GroupPolicyProxyDetectorTest);
};

INSTANTIATE_TEST_CASE_P(IsManagedIsUsingDMProxyDetector,
                        GroupPolicyProxyDetectorTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool()));

TEST_P(GroupPolicyProxyDetectorTest, NoPolicy) {
  ProxyConfig config;
  EXPECT_FAILED(detector_->Detect(&config));
}

TEST_P(GroupPolicyProxyDetectorTest, InvalidPolicyString) {
  SetPolicy(_T("this_is_never_a_real_policy"), CString(), CString());

  ProxyConfig config;
  EXPECT_FAILED(detector_->Detect(&config));
}

TEST_P(GroupPolicyProxyDetectorTest, PolicyDirect) {
  SetPolicy(kProxyModeDirect, CString(), CString());

  ProxyConfig config;
  EXPECT_EQ(IsManaged() ? S_OK : E_FAIL, detector_->Detect(&config));
  EXPECT_STREQ(GetSourceString(), config.source);
  EXPECT_FALSE(config.auto_detect);
  EXPECT_TRUE(config.auto_config_url.IsEmpty());
  EXPECT_TRUE(config.proxy.IsEmpty());
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
}

TEST_P(GroupPolicyProxyDetectorTest, PolicyAutoDetect) {
  SetPolicy(kProxyModeAutoDetect, CString(), CString());

  ProxyConfig config;
  EXPECT_EQ(IsManaged() ? S_OK : E_FAIL, detector_->Detect(&config));
  EXPECT_STREQ(GetSourceString(), config.source);
  EXPECT_EQ(IsManaged(), config.auto_detect);
  EXPECT_TRUE(config.auto_config_url.IsEmpty());
  EXPECT_TRUE(config.proxy.IsEmpty());
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
}

TEST_P(GroupPolicyProxyDetectorTest, PolicyPacUrlNoData) {
  SetPolicy(kProxyModePacScript, CString(), CString());

  ProxyConfig config;
  EXPECT_FAILED(detector_->Detect(&config));
}

TEST_P(GroupPolicyProxyDetectorTest, PolicyPacUrlHasData) {
  const TCHAR* const kUnitTestPacUrl = _T("http://unittest/testurl.pac");

  SetPolicy(kProxyModePacScript, kUnitTestPacUrl, CString());

  ProxyConfig config;
  EXPECT_EQ(IsManaged() ? S_OK : E_FAIL, detector_->Detect(&config));
  EXPECT_STREQ(GetSourceString(), config.source);
  EXPECT_FALSE(config.auto_detect);
  EXPECT_STREQ(IsManaged() ? kUnitTestPacUrl : _T(""), config.auto_config_url);
  EXPECT_TRUE(config.proxy.IsEmpty());
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
}

TEST_P(GroupPolicyProxyDetectorTest, PolicyFixedNoData) {
  SetPolicy(kProxyModeFixedServers, CString(), CString());

  ProxyConfig config;
  EXPECT_FAILED(detector_->Detect(&config));
}

TEST_P(GroupPolicyProxyDetectorTest, PolicyFixedHasData) {
  const TCHAR* const kUnitTestFixedServer = _T("unitTEST_Pixedserver:8080");

  SetPolicy(kProxyModeFixedServers, CString(), kUnitTestFixedServer);

  ProxyConfig config;
  EXPECT_EQ(IsManaged() ? S_OK : E_FAIL, detector_->Detect(&config));
  EXPECT_STREQ(GetSourceString(), config.source);
  EXPECT_FALSE(config.auto_detect);
  EXPECT_TRUE(config.auto_config_url.IsEmpty());
  EXPECT_STREQ(IsManaged() ? kUnitTestFixedServer : _T(""), config.proxy);
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
}

TEST_P(GroupPolicyProxyDetectorTest, PolicySystem) {
  SetPolicy(kProxyModeSystem, CString(), CString());

  ProxyConfig config;
  EXPECT_FAILED(detector_->Detect(&config));
}

}  // namespace omaha

