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

// All tests are user only.

#include <windows.h>
#include <limits.h>
#include <ctime>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/constants.h"
#include "omaha/common/error.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/statsreport/metrics.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

DEFINE_METRIC_bool(test_bool);

}  // namespace

class StatsUploaderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);

    // Overriding HKLM prevents the Windows DNS resolver from working.
    // Only override HKCU and run the tests as user.
    OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);
    stats_report::g_global_metrics.Initialize();

    // These tests assume that metric collection is enabled.
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueForceUsageStats,
                                      static_cast<DWORD>(1)));
  }

  virtual void TearDown() {
    stats_report::g_global_metrics.Uninitialize();
    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }

  HRESULT GetMetricValue(const TCHAR* value_name, bool* value) {
    CString key_name = key_name_ + CString(_T("Booleans"));

    scoped_array<byte> buffer;
    DWORD byte_count(0);
    HRESULT hr = RegKey::GetValue(key_name,
                                  value_name,
                                  address(buffer),
                                  &byte_count);
    if (FAILED(hr)) {
      return hr;
    }
    if (byte_count != sizeof(uint32)) {                         // NOLINT
      return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    *value = *reinterpret_cast<uint32*>(buffer.get()) != 0;
    return S_OK;
  }

  HRESULT GetLastTrasmission(DWORD* last_transmission) {
    const TCHAR value_name[] = _T("LastTransmission");
    return RegKey::GetValue(key_name_, value_name, last_transmission);
  }

  HRESULT SetLastTransmission(DWORD last_transmission) {
    const TCHAR value_name[] = _T("LastTransmission");
    return RegKey::SetValue(key_name_, value_name, last_transmission);
  }

  bool AreMetricsEmpty() {
    RegKey reg_key;
    HRESULT hr = reg_key.Open(key_name_, KEY_READ);
    if (FAILED(hr)) {
      return true;
    }
    return reg_key.GetSubkeyCount() == 0;
  }

  static const TCHAR key_name_[];
  static const TCHAR metric_name_[];
};

const TCHAR StatsUploaderTest::key_name_[] =
    _T("HKCU\\Software\\Google\\Update\\UsageStats\\Daily\\");
const TCHAR StatsUploaderTest::metric_name_[] =  _T("test_bool");


TEST_F(StatsUploaderTest, AggregateMetrics) {
  bool value = false;
  EXPECT_HRESULT_FAILED(GetMetricValue(metric_name_, &value));

  metric_test_bool = true;
  EXPECT_HRESULT_SUCCEEDED(AggregateMetrics(false));    // User.

  EXPECT_HRESULT_SUCCEEDED(GetMetricValue(metric_name_, &value));
  EXPECT_EQ(true, value);

  metric_test_bool = false;
  EXPECT_HRESULT_SUCCEEDED(AggregateMetrics(false));    // User.

  EXPECT_HRESULT_SUCCEEDED(GetMetricValue(metric_name_, &value));
  EXPECT_EQ(false, value);
}

TEST_F(StatsUploaderTest, AggregateAndReportMetrics) {
  metric_test_bool = true;

  // Metrics are not in the registry until they are aggregated.
  bool value = false;
  EXPECT_HRESULT_FAILED(GetMetricValue(metric_name_, &value));

  // AggregateAndReportMetrics resets metrics and updates 'LastTransmission' to
  // the current time since there was no 'LastTransmission'.
  EXPECT_HRESULT_SUCCEEDED(AggregateAndReportMetrics(false, false));
  EXPECT_TRUE(AreMetricsEmpty());
  DWORD last_transmission(0);
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_NE(0, last_transmission);

  // AggregateAndReportMetrics aggregates but it does not report since
  // 'LastTransmission is current.
  EXPECT_HRESULT_SUCCEEDED(AggregateAndReportMetrics(false, false));
  EXPECT_FALSE(AreMetricsEmpty());
  EXPECT_HRESULT_SUCCEEDED(GetMetricValue(metric_name_, &value));
  EXPECT_EQ(true, value);
  DWORD previous_last_transmission = last_transmission;
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_EQ(previous_last_transmission, last_transmission);

  // Roll back 'Last Trasmission' by 26 hours. AggregateAndReportMetrics
  // aggregates, reports metrics, and updates 'LastTransmission'.
  metric_test_bool = true;
  last_transmission -= 26 * 60 * 60;
  EXPECT_HRESULT_SUCCEEDED(SetLastTransmission(last_transmission));
  EXPECT_HRESULT_SUCCEEDED(AggregateAndReportMetrics(false, false));
  EXPECT_TRUE(AreMetricsEmpty());
  previous_last_transmission = last_transmission;
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_NE(previous_last_transmission, last_transmission);

  // Roll forward the 'LastTransmission' by 60 seconds.
  // AggregateAndReportMetrics resets metrics and updates 'LastTransmission' to
  // the current time since there 'LastTransmission' was in the future.
  metric_test_bool = true;
  last_transmission = static_cast<DWORD>(time(NULL)) + 60;
  EXPECT_HRESULT_SUCCEEDED(SetLastTransmission(last_transmission));
  EXPECT_HRESULT_SUCCEEDED(AggregateAndReportMetrics(false, false));
  EXPECT_TRUE(AreMetricsEmpty());
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_NE(0, last_transmission);

  // Force reporting the metrics.
  metric_test_bool = true;
  EXPECT_HRESULT_SUCCEEDED(AggregateAndReportMetrics(false, true));
  EXPECT_TRUE(AreMetricsEmpty());
}

TEST_F(StatsUploaderTest, ResetPersistentMetricsTest) {
  const TCHAR* keys[] = {
    _T("HKCU\\Software\\Google\\Update\\UsageStats\\Daily\\Timings"),
    _T("HKCU\\Software\\Google\\Update\\UsageStats\\Daily\\Counts"),
    _T("HKCU\\Software\\Google\\Update\\UsageStats\\Daily\\Integers"),
    _T("HKCU\\Software\\Google\\Update\\UsageStats\\Daily\\Booleans"),
  };
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKeys(keys, arraysize(keys)));
  EXPECT_HRESULT_SUCCEEDED(ResetMetrics(false));    // User.

  for (size_t i = 0; i != arraysize(keys); ++i) {
    EXPECT_FALSE(RegKey::HasKey(keys[i]));
  }
  EXPECT_TRUE(AreMetricsEmpty());

  DWORD last_transmission(ULONG_MAX);
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_NE(0, last_transmission);
}

// AggregateAndReportMetrics aggregates, but is unable to report metrics and
// does not update 'LastTransmission'.
TEST_F(StatsUploaderTest,
       AggregateAndReportMetrics_GoogleUpdateEulaNotAccepted_DoNotForce) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  metric_test_bool = true;
  DWORD last_transmission = 12345678;
  EXPECT_HRESULT_SUCCEEDED(SetLastTransmission(last_transmission));
  EXPECT_EQ(GOOPDATE_E_CANNOT_USE_NETWORK,
            AggregateAndReportMetrics(false, false));
  EXPECT_FALSE(AreMetricsEmpty());
  DWORD previous_last_transmission = last_transmission;
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_EQ(12345678, last_transmission);
}

// AggregateAndReportMetrics aggregates, but is unable to report metrics and
// does not update 'LastTransmission'.
TEST_F(StatsUploaderTest,
       AggregateAndReportMetrics_GoogleUpdateEulaNotAccepted_Force) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  metric_test_bool = true;
  DWORD last_transmission = 12345678;
  EXPECT_HRESULT_SUCCEEDED(SetLastTransmission(last_transmission));
  EXPECT_EQ(GOOPDATE_E_CANNOT_USE_NETWORK,
            AggregateAndReportMetrics(false, true));
  EXPECT_FALSE(AreMetricsEmpty());
  DWORD previous_last_transmission = last_transmission;
  EXPECT_HRESULT_SUCCEEDED(GetLastTrasmission(&last_transmission));
  EXPECT_EQ(12345678, last_transmission);
}

}  // namespace omaha
