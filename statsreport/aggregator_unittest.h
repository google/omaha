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
#ifndef OMAHA_STATSREPORT_AGGREGATOR_UNITTEST_H__
#define OMAHA_STATSREPORT_AGGREGATOR_UNITTEST_H__

#include "metrics.h"
#include "omaha/third_party/gtest/include/gtest/gtest.h"

/// Test fixture shared among aggregator unit tests
class MetricsAggregatorTest: public testing::Test {
public:
#define INIT_METRIC(type, name) name##_(#name, &coll_)
#define DECL_METRIC(type, name) stats_report::type##Metric name##_

  MetricsAggregatorTest() :
    INIT_METRIC(Count, c1),
    INIT_METRIC(Count, c2),
    INIT_METRIC(Timing, t1),
    INIT_METRIC(Timing, t2),
    INIT_METRIC(Integer, i1),
    INIT_METRIC(Integer, i2),
    INIT_METRIC(Bool, b1),
    INIT_METRIC(Bool, b2) {
  }

  enum {
    kNumCounts = 2,
    kNumTimings = 2,
    kNumIntegers = 2,
    kNumBools = 2
  };

  stats_report::MetricCollection coll_;
  DECL_METRIC(Count, c1);
  DECL_METRIC(Count, c2);
  DECL_METRIC(Timing, t1);
  DECL_METRIC(Timing, t2);
  DECL_METRIC(Integer, i1);
  DECL_METRIC(Integer, i2);
  DECL_METRIC(Bool, b1);
  DECL_METRIC(Bool, b2);

#undef INIT_METRIC
#undef DECL_METRIC

  virtual void SetUp() {
    coll_.Initialize();
  }

  virtual void TearDown() {
    coll_.Uninitialize();
  }
};

#endif  // OMAHA_STATSREPORT_AGGREGATOR_UNITTEST_H__
