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
// Helper class to aggregate the collected in-memory stats to persistent
// storage.
#ifndef OMAHA_STATSREPORT_AGGREGATOR_H__
#define OMAHA_STATSREPORT_AGGREGATOR_H__

#include "metrics.h"

namespace stats_report {
// TODO(omaha): Refactor to avoid cross platform code duplication.

/// Wrapper class and interface for metrics aggregation. This is a platform
/// independent class and needs to be subclassed for various platforms and/or
/// metrics persistence methods
class MetricsAggregator {
public:
  /// Aggregate all metrics in the associated collection
  /// @returns true iff aggregation started successfully, false otherwise.
  bool AggregateMetrics();

protected:
  MetricsAggregator();
  MetricsAggregator(const MetricCollection &coll);
  virtual ~MetricsAggregator();

  /// Start aggregation. Override this to grab locks, open files, whatever
  /// needs to happen or can expedite the individual aggregate steps.
  /// @return true on success, false on failure.
  /// @note aggregation will not progress if this function returns false
  virtual bool StartAggregation();
  virtual void EndAggregation();

  virtual void Aggregate(CountMetric &metric) = 0;
  virtual void Aggregate(TimingMetric &metric) = 0;
  virtual void Aggregate(IntegerMetric &metric) = 0;
  virtual void Aggregate(BoolMetric &metric) = 0;

private:
  DISALLOW_EVIL_CONSTRUCTORS(MetricsAggregator);

  const MetricCollection &coll_;
};

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_AGGREGATOR_H__
