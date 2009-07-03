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
#include "aggregator_unittest.h"

using namespace stats_report;

class TestMetricsAggregator: public MetricsAggregator {
public:
  TestMetricsAggregator(MetricCollection &coll) : MetricsAggregator(coll)
      , aggregating_(false), counts_(0), timings_(0), integers_(0), bools_(0) {
  }

  ~TestMetricsAggregator() {
  }

  bool aggregating() const { return aggregating_; }
  int counts() const { return counts_; }
  int timings() const { return timings_; }
  int integers() const { return integers_; }
  int bools() const { return bools_; }

protected:
  virtual bool StartAggregation() {
    aggregating_ = true;
    counts_ = 0;
    timings_ = 0;
    integers_ = 0;
    bools_ = 0;

    return true;
  }

  virtual void EndAggregation() {
    aggregating_ = false;
  }

  virtual void Aggregate(CountMetric &metric) {
    EXPECT_TRUE(aggregating());
    metric.Reset();
    ++counts_;
  }

  virtual void Aggregate(TimingMetric &metric) {
    UNREFERENCED_PARAMETER(metric);
    EXPECT_TRUE(aggregating());
    metric.Reset();
    ++timings_;
  }
  virtual void Aggregate(IntegerMetric &metric) {
    UNREFERENCED_PARAMETER(metric);
    EXPECT_TRUE(aggregating());
    // Integer metrics don't get reset on aggregation
    ++integers_;
  }
  virtual void Aggregate(BoolMetric &metric) {
    EXPECT_TRUE(aggregating());
    metric.Reset();
    ++bools_;
  }

private:
  bool aggregating_;
  int counts_;
  int timings_;
  int integers_;
  int bools_;
};

TEST_F(MetricsAggregatorTest, Aggregate) {
  TestMetricsAggregator agg(coll_);

  EXPECT_FALSE(agg.aggregating());
  EXPECT_EQ(0, agg.counts());
  EXPECT_EQ(0, agg.timings());
  EXPECT_EQ(0, agg.integers());
  EXPECT_EQ(0, agg.bools());
  EXPECT_TRUE(agg.AggregateMetrics());
  EXPECT_FALSE(agg.aggregating());

  // check that we saw all counters.
  EXPECT_TRUE(kNumCounts == agg.counts());
  EXPECT_TRUE(kNumTimings == agg.timings());
  EXPECT_TRUE(kNumIntegers == agg.integers());
  EXPECT_TRUE(kNumBools == agg.bools());
}

class FailureTestMetricsAggregator: public TestMetricsAggregator {
public:
  FailureTestMetricsAggregator(MetricCollection &coll) :
      TestMetricsAggregator(coll) {
  }

protected:
  virtual bool StartAggregation() {
    return false;
  }
};

TEST_F(MetricsAggregatorTest, AggregateFailure) {
  FailureTestMetricsAggregator agg(coll_);

  EXPECT_FALSE(agg.AggregateMetrics());
}
