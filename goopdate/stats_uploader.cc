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


#include "omaha/goopdate/stats_uploader.h"
#include <atlbase.h>
#include <atlconv.h>
#include <atlstr.h>
#include <ctime>
#include "omaha/common/const_addresses.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/net/browser_request.h"
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

  // Do not access the network during an OEM install.
  if (!ConfigManager::Instance()->CanUseNetwork(is_machine)) {
    CORE_LOG(L1, (_T("[Stats not uploaded because network use prohibited]")));
    return GOOPDATE_E_CANNOT_USE_NETWORK;
  }

  const TCHAR* version = GetVersionString();
  CString machine_id(goopdate_utils::GetPersistentMachineId());
  CString user_id(goopdate_utils::GetPersistentUserId(is_machine ?
                                                      MACHINE_KEY_NAME :
                                                      USER_KEY_NAME));
  CString test_source(ConfigManager::Instance()->GetTestSource());

  CString url(kUrlUsageStatsReport);
  url.AppendFormat(_T("?%s=%s&%s=%s&%s=%s&%s=%s&%s=%s&%s=%s&%s"),
      kMetricsServerParamSourceId,  kMetricsProductName,
      kMetricsServerParamVersion,   version,
      kMetricsServerParamIsMachine, is_machine ? _T("1") : _T("0"),
      kMetricsServerTestSource,     test_source,
      kMetricsServerMachineId,      machine_id,
      kMetricsServerUserId,         user_id,
      extra_url_data);

  CORE_LOG(L3, (_T("[upload usage stats][%s]"), content));

  const NetworkConfig::Session& session(NetworkConfig::Instance().session());
  NetworkRequest network_request(session);
  network_request.set_num_retries(1);
  network_request.AddHttpRequest(new SimpleRequest);
  network_request.AddHttpRequest(new BrowserRequest);

  // PostRequest falls back to https.
  CString response;
  return PostRequest(&network_request, true, url, content, &response);
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
  key_name.Format(kStatsKeyFormatString, kMetricsProductName);
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
  key_name.Format(kStatsKeyFormatString, kMetricsProductName);
  HKEY parent_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  HRESULT hr = key.Create(parent_key, key_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Unable to create metrics key][0x%08x]"), hr));
    return hr;
  }

  DWORD now = static_cast<DWORD>(time(NULL));

  DWORD last_transmission_time(0);
  hr = key.GetValue(kLastTransmissionTimeValueName, &last_transmission_time);

  // Reset and start over if last transmission time is missing or hinky.
  if (FAILED(hr) || last_transmission_time > now) {
    CORE_LOG(LW, (_T("[hinky or missing last transmission time]")));
    ResetPersistentMetrics(&key);
    return S_OK;
  }

  hr = DoAggregateMetrics(is_machine);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DoAggregateMetrics failed][0x%08x]"), hr));
    return hr;
  }

  DWORD time_since_last_transmission = now - last_transmission_time;
  if (force_report ||
      time_since_last_transmission >= kMetricsUploadIntervalSec) {
    // Report the metrics, reset the metrics, and update 'LastTransmission'.
    HRESULT hr = ReportMetrics(is_machine, NULL, time_since_last_transmission);
    if (SUCCEEDED(hr)) {
      VERIFY1(SUCCEEDED(ResetPersistentMetrics(&key)));
      DWORD now_sec = static_cast<DWORD>(time(NULL));
      CORE_LOG(L3, (_T("[Stats upload successful]")));
      return S_OK;
    } else {
      CORE_LOG(LE, (_T("[Stats upload failed][0x%08x]"), hr));
      return hr;
    }
  }

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

