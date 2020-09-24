// Copyright 2008-2010 Google Inc.
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


#include "omaha/common/stats_uploader.h"
#include <atlbase.h>
#include <atlconv.h>
#include <atlstr.h>
#include <ctime>
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/statsreport/aggregator-win32.h"
#include "omaha/statsreport/const-win32.h"
#include "omaha/statsreport/formatter.h"
#include "omaha/statsreport/metrics.h"
#include "omaha/statsreport/persistent_iterator-win32.h"

using stats_report::g_global_metrics;

using stats_report::kCountsKeyName;
using stats_report::kTimingsKeyName;
using stats_report::kIntegersKeyName;
using stats_report::kBooleansKeyName;
using stats_report::kStatsKeyFormatString;
using stats_report::kLastTransmissionTimeValueName;

using stats_report::Formatter;
using stats_report::MetricsAggregatorWin32;
using stats_report::PersistentMetricsIteratorWin32;

namespace omaha {

namespace {

HRESULT ResetPersistentMetrics(RegKey* key) {
  ASSERT1(key);
  HRESULT result = S_OK;
  DWORD now_sec = static_cast<DWORD>(time(NULL));
  HRESULT hr = key->SetValue(kLastTransmissionTimeValueName, now_sec);
  if (FAILED(hr)) {
    result = hr;
  }
  hr = key->DeleteSubKey(kCountsKeyName);
  if (FAILED(hr)) {
    result = hr;
  }
  hr = key->DeleteSubKey(kTimingsKeyName);
  if (FAILED(hr)) {
    result = hr;
  }
  hr = key->DeleteSubKey(kIntegersKeyName);
  if (FAILED(hr)) {
    result = hr;
  }
  hr = key->DeleteSubKey(kBooleansKeyName);
  if (FAILED(hr)) {
    result = hr;
  }
  return result;
}

// Returns S_OK without uploading in OEM mode.
HRESULT UploadMetrics(bool is_machine,
                      const TCHAR* extra_url_data,
                      const TCHAR* content) {
  ASSERT1(content);

  CString uid = goopdate_utils::GetUserIdLazyInit(is_machine);

  // Impersonate the user if the caller is machine, running as local system,
  // and a user is logged on to the system.
  scoped_handle impersonation_token(
      goopdate_utils::GetImpersonationTokenForMachineProcess(is_machine));
  scoped_impersonation impersonate_user(get(impersonation_token));

  // Do not access the network during an OEM install.
  if (!ConfigManager::Instance()->CanUseNetwork(is_machine)) {
    CORE_LOG(L1, (_T("[Stats not uploaded because network use prohibited]")));
    return GOOPDATE_E_CANNOT_USE_NETWORK;
  }

#if defined(GOOGLE_UPDATE_BUILD)
  // The usagestats collection, aggregation, and reporting features are
  // deprecated and may be removed from the code base in the future.
  // Omaha uses the completion pings for telemetry and quality of service.
  // There is no need to collect and report a different kind of usagestats.
  UNREFERENCED_PARAMETER(is_machine);
  UNREFERENCED_PARAMETER(extra_url_data);
  UNREFERENCED_PARAMETER(content);
  OPT_LOG(L3, (_T("[Stats not uploaded because the feature is deprecated.]")));
  return S_FALSE;
#else
  const TCHAR* version = GetVersionString();
  CString test_source(ConfigManager::Instance()->GetTestSource());

  CString url;
  HRESULT hr = ConfigManager::Instance()->GetUsageStatsReportUrl(&url);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetUsageStatsReportUrl failed][0x%08x]"), hr));
    return hr;
  }
  SafeCStringAppendFormat(&url, _T("?%s=%s&%s=%s&%s=%s&%s=%s&%s=%s&%s"),
      kMetricsServerParamSourceId,  kMetricsProductName,
      kMetricsServerParamVersion,   version,
      kMetricsServerParamIsMachine, is_machine ? _T("1") : _T("0"),
      kMetricsServerTestSource,     test_source,
      kMetricsServerUserId,         uid,
      extra_url_data);

  CORE_LOG(L3, (_T("[upload usage stats][%s]"), content));

  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  hr = network_manager.GetUserNetworkConfig(&network_config);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetUserNetworkConfig failed][0x%08x]"), hr));
    return hr;
  }
  NetworkRequest network_request(network_config->session());

  network_request.set_num_retries(1);
  network_request.AddHttpRequest(new SimpleRequest);

  std::vector<uint8> response_buffer;
  return network_request.PostString(url, content, &response_buffer);
#endif  // GOOGLE_UPDATE_BUILD
}

HRESULT ReportMetrics(bool is_machine,
                      const TCHAR* extra_url_data,
                      DWORD interval) {
  PersistentMetricsIteratorWin32 it(kMetricsProductName, is_machine), end;
  Formatter formatter(CT2A(kMetricsProductName), interval);

  for (; it != end; ++it) {
    formatter.AddMetric(*it);
  }

  return UploadMetrics(is_machine, extra_url_data, CA2T(formatter.output()));
}

HRESULT DoResetMetrics(bool is_machine) {
  CString key_name;
  SafeCStringFormat(&key_name, kStatsKeyFormatString, kMetricsProductName);
  HKEY parent_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  HRESULT hr = key.Create(parent_key, key_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Unable to create metrics key][0x%08x]"), hr));
    return hr;
  }
  return ResetPersistentMetrics(&key);
}

HRESULT DoAggregateMetrics(bool is_machine) {
  MetricsAggregatorWin32 aggregator(g_global_metrics,
                                    kMetricsProductName,
                                    is_machine);
  if (!aggregator.AggregateMetrics()) {
    CORE_LOG(LW, (_T("[Metrics aggregation failed for unknown reasons]")));
    return GOOPDATE_E_METRICS_AGGREGATE_FAILED;
  }
  return S_OK;
}

HRESULT DoAggregateAndReportMetrics(bool is_machine, bool force_report) {
  CString key_name;
  SafeCStringFormat(&key_name, kStatsKeyFormatString, kMetricsProductName);
  HKEY parent_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  HRESULT hr = key.Create(parent_key, key_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Unable to create metrics key][0x%08x]"), hr));
    return hr;
  }

  DWORD now_sec = static_cast<DWORD>(time(NULL));

  DWORD last_transmission_sec(0);
  hr = key.GetValue(kLastTransmissionTimeValueName, &last_transmission_sec);

  // Reset and start over if last transmission time is missing or hinky.
  if (FAILED(hr) || last_transmission_sec > now_sec) {
    CORE_LOG(LW, (_T("[hinky or missing last transmission time][%u][now: %u]"),
                  last_transmission_sec, now_sec));
    ResetPersistentMetrics(&key);
    return S_OK;
  }

  hr = DoAggregateMetrics(is_machine);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DoAggregateMetrics failed][0x%08x]"), hr));
    return hr;
  }

  DWORD time_since_last_transmission = now_sec - last_transmission_sec;
  if (!force_report &&
      time_since_last_transmission < kMetricsUploadIntervalSec) {
    CORE_LOG(L1, (_T("[Stats upload not needed][last: %u][now: %u]"),
                  last_transmission_sec, now_sec));
    return S_OK;
  }

  // Report the metrics, reset the metrics, and update 'LastTransmission'.
  hr = ReportMetrics(is_machine, NULL, time_since_last_transmission);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Stats upload failed][0x%08x]"), hr));
    return hr;
  }

  VERIFY_SUCCEEDED(ResetPersistentMetrics(&key));
  CORE_LOG(L3, (_T("[Stats upload successful]")));
  return S_OK;
}

bool InitializeLock(GLock* lock, bool is_machine) {
  ASSERT1(lock);
  NamedObjectAttributes attributes;
  GetNamedObjectAttributes(kMetricsSerializer, is_machine, &attributes);
  return lock->InitializeWithSecAttr(attributes.name, &attributes.sa);
}

}  // namespace


HRESULT ResetMetrics(bool is_machine) {
  CORE_LOG(L2, (_T("[ResetMetrics]")));
  GLock lock;
  if (!InitializeLock(&lock, is_machine)) {
    return GOOPDATE_E_METRICS_LOCK_INIT_FAILED;
  }
  __mutexScope(lock);
  return DoResetMetrics(is_machine);
}

HRESULT AggregateMetrics(bool is_machine) {
  CORE_LOG(L2, (_T("[AggregateMetrics]")));

  if (!ConfigManager::Instance()->CanCollectStats(is_machine)) {
    return S_OK;
  }

  GLock lock;
  if (!InitializeLock(&lock, is_machine)) {
    return GOOPDATE_E_METRICS_LOCK_INIT_FAILED;
  }
  __mutexScope(lock);
  return DoAggregateMetrics(is_machine);
}

HRESULT AggregateAndReportMetrics(bool is_machine, bool force_report) {
  CORE_LOG(L2, (_T("[AggregateAndReportMetrics]")));

  if (!ConfigManager::Instance()->CanCollectStats(is_machine)) {
    return S_OK;
  }

  GLock lock;
  if (!InitializeLock(&lock, is_machine)) {
    return GOOPDATE_E_METRICS_LOCK_INIT_FAILED;
  }
  __mutexScope(lock);
  return DoAggregateAndReportMetrics(is_machine, force_report);
}

}  // namespace omaha

