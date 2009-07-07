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
// Implementation of helper classes to aggregate the collected in-memory 
// stats to persistent storage.
#include "aggregator.h"

namespace stats_report {

bool MetricsAggregator::AggregateMetrics() {
  if (!StartAggregation())
    return false;
  
  MetricIterator it(coll_), end;
  for (; it != end; ++it) {
    MetricBase *metric = *it;
    DCHECK(NULL != metric);
    
    switch (metric->type()) {
     case kCountType:
      Aggregate(metric->AsCount());
      break;
     case kTimingType:
      Aggregate(metric->AsTiming());
      break;
     case kIntegerType:
      Aggregate(metric->AsInteger());
      break;
     case kBoolType:
      Aggregate(metric->AsBool());
      break;
     default:
      DCHECK(false && "Impossible metric type");
      break;
    }
  }
  
  // done, close up
  EndAggregation();
  
  return true;
}

MetricsAggregator::MetricsAggregator() : coll_(g_global_metrics) {
  DCHECK(coll_.initialized());
}

MetricsAggregator::MetricsAggregator(const MetricCollection &coll) 
    : coll_(coll) {
  DCHECK(coll_.initialized());
}

MetricsAggregator::~MetricsAggregator() {
}

bool MetricsAggregator::StartAggregation() {
  // nothing
  return true;
}

void MetricsAggregator::EndAggregation() {
  // nothing
}

} // namespace stats_report