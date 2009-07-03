// Copyright 2006-2009 Google Inc.
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
//
// Implementation of Win32 metrics aggregator.
#include "aggregator-win32.h"
#include "const-win32.h"
#include "util-win32.h"

namespace stats_report {

MetricsAggregatorWin32::MetricsAggregatorWin32(MetricCollection &coll,
                                               const wchar_t *key_name)
    : MetricsAggregator(coll),
      is_machine_(false) {
  DCHECK(NULL != key_name);

  key_name_.Format(kStatsKeyFormatString, key_name);
}

MetricsAggregatorWin32::MetricsAggregatorWin32(MetricCollection &coll,
                                               const wchar_t *key_name,
                                               bool is_machine)
    : MetricsAggregator(coll),
      is_machine_(is_machine) {
  DCHECK(NULL != key_name);

  key_name_.Format(kStatsKeyFormatString, key_name);
}

MetricsAggregatorWin32::~MetricsAggregatorWin32() {
}

bool MetricsAggregatorWin32::StartAggregation() {
  DCHECK(NULL == key_.m_hKey);

  HKEY parent_key = is_machine_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  LONG err = key_.Create(parent_key, key_name_);
  if (err != ERROR_SUCCESS)
    return false;

  return true;
}

void MetricsAggregatorWin32::EndAggregation() {
  count_key_.Close();
  timing_key_.Close();
  integer_key_.Close();
  bool_key_.Close();

  key_.Close();
}

bool MetricsAggregatorWin32::EnsureKey(const wchar_t *name, CRegKey *key) {
  if (NULL != key->m_hKey)
    return true;

  LONG err = key->Create(key_, name);
  if (ERROR_SUCCESS != err) {
    DCHECK(NULL == key->m_hKey);
    // TODO(omaha): log?
    return false;
  }

  return true;
}

void MetricsAggregatorWin32::Aggregate(CountMetric &metric) {
  // do as little as possible if no value
  uint64 value = metric.Reset();
  if (0 == value)
    return;

  if (!EnsureKey(kCountsKeyName, &count_key_))
    return;

  CString name(metric.name());
  uint64 reg_value = 0;
  if (!GetData(count_key_, name, &reg_value)) {
    // TODO(omaha): clean up??
  }
  reg_value += value;

  DWORD err = count_key_.SetBinaryValue(name, &reg_value, sizeof(reg_value));
}

void MetricsAggregatorWin32::Aggregate(TimingMetric &metric) {
  // do as little as possible if no value
  TimingMetric::TimingData value = metric.Reset();
  if (0 == value.count)
    return;

  if (!EnsureKey(kTimingsKeyName, &timing_key_))
    return;

  CString name(metric.name());
  TimingMetric::TimingData reg_value;
  if (!GetData(timing_key_, name, &reg_value)) {
    memcpy(&reg_value, &value, sizeof(value));
  } else {
    reg_value.count += value.count;
    reg_value.sum += value.sum;
    reg_value.minimum = std::min(reg_value.minimum, value.minimum);
    reg_value.maximum = std::max(reg_value.maximum, value.maximum);
  }

  DWORD err = timing_key_.SetBinaryValue(name, &reg_value, sizeof(reg_value));
}

void MetricsAggregatorWin32::Aggregate(IntegerMetric &metric) {
  // do as little as possible if no value
  uint64 value = metric.value();
  if (0 == value)
    return;

  if (!EnsureKey(kIntegersKeyName, &integer_key_))
    return;

  DWORD err = integer_key_.SetBinaryValue(CString(metric.name()),
                                          &value, sizeof(value));
}

void MetricsAggregatorWin32::Aggregate(BoolMetric &metric) {
  // do as little as possible if no value
  int32 value = metric.Reset();
  if (BoolMetric::kBoolUnset == value)
    return;

  if (!EnsureKey(kBooleansKeyName, &bool_key_))
    return;

  DWORD err = bool_key_.SetBinaryValue(CString(metric.name()),
                                       &value, sizeof(value));
}

} // namespace stats_report
