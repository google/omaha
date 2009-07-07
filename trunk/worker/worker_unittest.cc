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

#include "omaha/common/time.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/statsreport/aggregator.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/worker.h"
#include "omaha/worker/worker_metrics.h"
#include "omaha/worker/worker-internal.h"

namespace omaha {

namespace {

const TCHAR* const kDailyUsageStatsKeyPath =
    _T("HKCU\\Software\\Google\\Update\\UsageStats\\Daily");

// The alphabetical order of these is important for
// RecordUpdateAvailableUsageStatsTest.
const TCHAR* const kApp1 = _T("{0C480772-AC73-418f-9603-66303DA4C7AA}");
const TCHAR* const kApp2 = _T("{89906BCD-4D12-4c9b-B5BA-8286051CB8D9}");
const TCHAR* const kApp3 = _T("{F5A1FE97-CF5A-47b8-8B28-2A72F9A57A45}");

const uint64 kApp1GuidUpper = 0x0C480772AC73418f;
const uint64 kApp2GuidUpper = 0x89906BCD4D124c9b;

const TCHAR* const kApp1ClientsKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{0C480772-AC73-418f-9603-66303DA4C7AA}");
const TCHAR* const kApp2ClientsKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{89906BCD-4D12-4c9b-B5BA-8286051CB8D9}");
const TCHAR* const kApp3ClientsKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{F5A1FE97-CF5A-47b8-8B28-2A72F9A57A45}");

const TCHAR* const kApp1ClientStateKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{0C480772-AC73-418f-9603-66303DA4C7AA}");
const TCHAR* const kApp2ClientStateKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{89906BCD-4D12-4c9b-B5BA-8286051CB8D9}");
const TCHAR* const kApp3ClientStateKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{F5A1FE97-CF5A-47b8-8B28-2A72F9A57A45}");

}  // namespace

class WorkerTest : public testing::Test {
 protected:
  Worker worker_;
};

// TODO(omaha): Test all methods of Worker

class RecordUpdateAvailableUsageStatsTest : public testing::Test {
 protected:
  RecordUpdateAvailableUsageStatsTest() : is_machine_(false) {}

  static void SetUpTestCase() {
    // Initialize the global metrics collection.
    stats_report::g_global_metrics.Initialize();
  }

  static void TearDownTestCase() {
    // The global metrics collection must be uninitialized before the metrics
    // destructors are called.
    stats_report::g_global_metrics.Uninitialize();
  }

  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
    OverrideRegistryHives(kRegistryHiveOverrideRoot);

    metric_worker_self_update_responses.Set(0);
    metric_worker_self_update_response_time_since_first_ms.Set(0);
    metric_worker_app_max_update_responses_app_high.Set(0);
    metric_worker_app_max_update_responses.Set(0);
    metric_worker_app_max_update_responses_ms_since_first.Set(0);

    ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                      kRegValueProductVersion,
                                      _T("0.1.0.0")));
    ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientsKeyPathUser,
                                      kRegValueProductVersion,
                                      _T("0.1")));

    ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENT_STATE_GOOPDATE));
    ASSERT_SUCCEEDED(RegKey::CreateKey(kApp1ClientStateKeyPathUser));
  }

  int GetNumProducts() {
    AppManager app_manager(is_machine_);
    ProductDataVector products;
    VERIFY1(SUCCEEDED(app_manager.GetRegisteredProducts(&products)));
    return products.size();
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }

  bool is_machine_;
  scoped_ptr<AppManager> app_manager_;
};

TEST_F(RecordUpdateAvailableUsageStatsTest, NoData) {
  ASSERT_EQ(2, GetNumProducts());

  internal::RecordUpdateAvailableUsageStats(is_machine_);

  EXPECT_EQ(0, metric_worker_self_update_responses.value());
  EXPECT_EQ(0, metric_worker_self_update_response_time_since_first_ms.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses_ms_since_first.value());
}

TEST_F(RecordUpdateAvailableUsageStatsTest, OmahaDataOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(10)));

  ASSERT_EQ(2, GetNumProducts());

  const time64 current_time_100ns(GetCurrent100NSTime());
  const uint64 expected_ms_since_first_update =
      (current_time_100ns - 10) / kMillisecsTo100ns;

  internal::RecordUpdateAvailableUsageStats(is_machine_);

  EXPECT_EQ(123456, metric_worker_self_update_responses.value());

  EXPECT_LE(expected_ms_since_first_update,
            metric_worker_self_update_response_time_since_first_ms.value());
  EXPECT_GT(expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_self_update_response_time_since_first_ms.value());

  EXPECT_EQ(0, metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses_ms_since_first.value());
}

TEST_F(RecordUpdateAvailableUsageStatsTest, OneAppOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(10)));

  ASSERT_EQ(2, GetNumProducts());

  const time64 current_time_100ns(GetCurrent100NSTime());
  const uint64 expected_ms_since_first_update =
      (current_time_100ns - 10) / kMillisecsTo100ns;

  internal::RecordUpdateAvailableUsageStats(is_machine_);

  EXPECT_EQ(0, metric_worker_self_update_responses.value());
  EXPECT_EQ(0, metric_worker_self_update_response_time_since_first_ms.value());

  EXPECT_EQ(kApp1GuidUpper,
            metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(123456, metric_worker_app_max_update_responses.value());
  EXPECT_LE(expected_ms_since_first_update,
            metric_worker_app_max_update_responses_ms_since_first.value());
  EXPECT_GT(expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_app_max_update_responses_ms_since_first.value());
}

// It is important that Omaha's count is the largest.
// All app data should be from app 2, which has the greatest count, a middle
// time, and an alphabetically middle GUID
TEST_F(RecordUpdateAvailableUsageStatsTest, OmahaAndSeveralApps) {
  const DWORD64 kApp2SinceTime = 1000 * kSecsTo100ns;

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp2ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("1.2")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp3ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("2.3")));

  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(0x99887766)));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(1)));

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(1)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(1)));

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp2ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(9876543)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp2ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    kApp2SinceTime));

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp3ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(234)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp3ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(128580000000000000)));

  ASSERT_EQ(4, GetNumProducts());

  const time64 current_time_100ns(GetCurrent100NSTime());
  const uint64 goopdate_expected_ms_since_first_update =
      (current_time_100ns - 1) / kMillisecsTo100ns;

  const uint64 app_expected_ms_since_first_update =
      (current_time_100ns - kApp2SinceTime) / kMillisecsTo100ns;

  internal::RecordUpdateAvailableUsageStats(is_machine_);

  EXPECT_EQ(0x99887766, metric_worker_self_update_responses.value());
  EXPECT_LE(goopdate_expected_ms_since_first_update,
            metric_worker_self_update_response_time_since_first_ms.value());
  EXPECT_GT(goopdate_expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_self_update_response_time_since_first_ms.value());

  EXPECT_EQ(kApp2GuidUpper,
            metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(9876543, metric_worker_app_max_update_responses.value());
  EXPECT_LE(app_expected_ms_since_first_update,
            metric_worker_app_max_update_responses_ms_since_first.value());
  EXPECT_GT(app_expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_app_max_update_responses_ms_since_first.value());
}

class SendSelfUpdateFailurePingTest : public testing::Test {
 protected:
  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
    // Overriding HKLM prevents the Windows DNS resolver from working.
    // Only override HKCU and run the tests as user.
    OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }
};

TEST_F(SendSelfUpdateFailurePingTest, UserKeyDoesNotExist) {
  ExpectAsserts expect_asserts;
  internal::SendSelfUpdateFailurePing(false);
}

TEST_F(SendSelfUpdateFailurePingTest, MachineKeyDoesNotExist) {
  OverrideRegistryHives(kRegistryHiveOverrideRoot);
  ExpectAsserts expect_asserts;
  internal::SendSelfUpdateFailurePing(true);
}

TEST_F(SendSelfUpdateFailurePingTest, UserUpdateErrorCodeDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  internal::SendSelfUpdateFailurePing(false);
}

TEST_F(SendSelfUpdateFailurePingTest, MachineUpdateErrorCodeDoesNotExist) {
  OverrideRegistryHives(kRegistryHiveOverrideRoot);
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  internal::SendSelfUpdateFailurePing(true);
}

TEST_F(SendSelfUpdateFailurePingTest, UserAllValuesPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));
  internal::SendSelfUpdateFailurePing(false);
}

TEST_F(SendSelfUpdateFailurePingTest, UserValuesPresentInMachineOnly) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  ExpectAsserts expect_asserts;
  internal::SendSelfUpdateFailurePing(false);

  // Clean up HKLM, which isn't overridden.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                       kRegValueSelfUpdateErrorCode));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                       kRegValueSelfUpdateExtraCode1));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                       kRegValueSelfUpdateVersion));
}

}  // namespace omaha
