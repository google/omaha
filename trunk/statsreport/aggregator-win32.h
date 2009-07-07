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
// Win32 aggregator, which aggregates counters to registry under a named
// Mutex lock.
#ifndef OMAHA_STATSREPORT_AGGREGATOR_WIN32_H__
#define OMAHA_STATSREPORT_AGGREGATOR_WIN32_H__

#include "aggregator.h"
#include <atlbase.h>
#include <atlstr.h>

namespace stats_report {

class MetricsAggregatorWin32: public MetricsAggregator {
public:
  /// @param coll the metrics collection to aggregate, most usually this
  ///           is g_global_metrics.
  /// @param app_name name of the subkey under HKCU\Software\Google we
  ///           aggregate to.
  MetricsAggregatorWin32(MetricCollection &coll,
                         const wchar_t *app_name);

  /// @param is_machine specifies the registry hive where the stats are
  ///           aggregated to.
  MetricsAggregatorWin32(MetricCollection &coll,
                         const wchar_t *app_name,
                         bool is_machine);
  virtual ~MetricsAggregatorWin32();

protected:
  virtual bool StartAggregation();
  virtual void EndAggregation();

  virtual void Aggregate(CountMetric &metric);
  virtual void Aggregate(TimingMetric &metric);
  virtual void Aggregate(IntegerMetric &metric);
  virtual void Aggregate(BoolMetric &metric);
private:
  enum {
    /// Max length of time we wait for the mutex on StartAggregation.
    kMaxMutexWaitMs = 1000, // 1 second for now
  };

  /// Ensures that *key is open, opening it if it's NULL
  /// @return true on success, false on failure to open key
  bool EnsureKey(const wchar_t *name, CRegKey *key);

  /// Mutex name for locking access to key
  CString mutex_name_;

  /// Subkey name, as per constructor docs
  CString key_name_;

  /// Handle to our subkey under HKCU\Software\Google
  CRegKey key_;

  /// Subkeys under the above
  /// @{
  CRegKey count_key_;
  CRegKey timing_key_;
  CRegKey integer_key_;
  CRegKey bool_key_;
  /// @}

  /// Specifies HKLM or HKCU, respectively.
  bool is_machine_;

  DISALLOW_EVIL_CONSTRUCTORS(MetricsAggregatorWin32);
};

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_AGGREGATOR_WIN32_H__
