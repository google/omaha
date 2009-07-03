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
// metrics report unit testing
#include "omaha/third_party/gtest/include/gtest/gtest.h"
#include "metrics.h"
#include <algorithm>
#include <new>

DECLARE_METRIC_count(count);
DEFINE_METRIC_count(count);

DECLARE_METRIC_timing(timing);
DEFINE_METRIC_timing(timing);

DECLARE_METRIC_integer(integer);
DEFINE_METRIC_integer(integer);

DECLARE_METRIC_bool(bool);
DEFINE_METRIC_bool(bool);

using namespace stats_report;

namespace {

class MetricsTest: public testing::Test {
protected:
  MetricCollection coll_;
};

class MetricsEnumTest: public MetricsTest {
public:
  virtual void SetUp() {
    coll_.Initialize();
  }

  virtual void TearDown() {
    coll_.Uninitialize();
  }

protected:
  MetricsEnumTest(): count_("count", &coll_), timing_("timing", &coll_),
       integer_("integer", &coll_), bool_("bool", &coll_){
  }

  CountMetric count_;
  TimingMetric timing_;
  IntegerMetric integer_;
  BoolMetric bool_;
};

} // namespace

// Validates that the above-declared metrics are available
// in the expected namespace
TEST_F(MetricsTest, Globals) {
  EXPECT_EQ(0, ::metric_count.Reset());
  TimingMetric::TimingData data = ::metric_timing.Reset();
  EXPECT_EQ(0, data.count);
  EXPECT_EQ(0, data.maximum);
  EXPECT_EQ(0, data.minimum);
  EXPECT_EQ(0, data.sum);

  EXPECT_EQ(0, ::metric_integer.value());
  EXPECT_EQ(BoolMetric::kBoolUnset, ::metric_bool.Reset());

  // Check for correct initialization
  EXPECT_STREQ("count", metric_count.name());
  EXPECT_STREQ("timing", metric_timing.name());
  EXPECT_STREQ("integer", metric_integer.name());
  EXPECT_STREQ("bool", metric_bool.name());
}


// make GTest happy
inline std::ostream &operator << (std::ostream &str, const MetricIterator &it) {
  str << std::hex << (void*)*it;
  return str;
}

TEST_F(MetricsTest, CollectionInitialization) {
  // The global MetricCollection is aliased to zero memory so as to ensure
  // no initialization order snafus. If an initialized MetricCollection
  // sets any of its storage to non-zero, there's a good chance that e.g. a
  // vtbl has snuck in there, which must not happen
  char buf1[sizeof(MetricCollection)] = { 0 };
  char buf2[sizeof(MetricCollection)] = { 0 };

  // Placement new a MetricCollection to one of the buffers
  new (buf1) MetricCollection();

  // and check they're still equivalent
  EXPECT_EQ(0, memcmp(buf1, buf2, sizeof(MetricCollection)));

  // MetricCollection must not extend MetricCollectionBase in size
  EXPECT_EQ(sizeof(MetricCollection), sizeof(MetricCollectionBase));
}

TEST_F(MetricsTest, Count) {
  CountMetric foo("foo", &coll_);

  EXPECT_EQ(0, foo.Reset());
  EXPECT_EQ(kCountType, foo.type());
  CountMetric &foo_ref = foo.AsCount();

  ++foo;
  EXPECT_EQ(1, foo.value());
  foo++;
  EXPECT_EQ(2, foo.value());

  foo += 100;
  EXPECT_EQ(102, foo.value());
}

TEST_F(MetricsTest, Timing) {
  TimingMetric foo("foo", &coll_);

  EXPECT_EQ(kTimingType, foo.type());
  TimingMetric &foo_ref = foo.AsTiming();

  foo.AddSample(100);
  foo.AddSample(50);

  EXPECT_EQ(2, foo.count());
  EXPECT_EQ(150, foo.sum());
  EXPECT_EQ(100, foo.maximum());
  EXPECT_EQ(50, foo.minimum());
  EXPECT_EQ(75, foo.average());

  TimingMetric::TimingData data = foo.Reset();
  EXPECT_EQ(2, data.count);
  EXPECT_EQ(150, data.sum);
  EXPECT_EQ(100, data.maximum);
  EXPECT_EQ(50, data.minimum);

  EXPECT_EQ(0, foo.count());
  EXPECT_EQ(0, foo.sum());
  EXPECT_EQ(0, foo.maximum());
  EXPECT_EQ(0, foo.minimum());
  EXPECT_EQ(0, foo.average());

  // Test counted samples
  foo.AddSamples(10, 1000);
  foo.AddSamples(10, 500);
  EXPECT_EQ(20, foo.count());
  EXPECT_EQ(1500, foo.sum());
  EXPECT_EQ(100, foo.maximum());
  EXPECT_EQ(50, foo.minimum());
  EXPECT_EQ(75, foo.average());
}

TEST_F(MetricsTest, TimingSample) {
  TimingMetric foo("foo", &coll_);

  // add a sample to foo
  {
    TimingSample sample(foo);

    ::Sleep(30);
  }

  TimingMetric::TimingData data = foo.Reset();

  // Should be precisely one sample in there
  EXPECT_EQ(1, data.count);

  // Let's hope the scheduler doesn't leave us hanging more than 10 ms.
  // Increased time from 40 to 100 because test failed intermittently on Pulse.
  EXPECT_GE(30 + 70, data.sum);
  // The sleep above seems to often terminate early on the build server,
  // I've observed captured times down to 18 ms, which is strange.
  // TODO(omaha): figure out whether the timer is broken or whether
  //    sleep is breaking its promise, or whether e.g. we're getting different
  //    walltimes on different CPUs due to BIOS bugs on the build server
  EXPECT_LE(14, data.sum);

  // again, this time with a non-unity count
  {
    TimingSample sample(foo, 2);

    EXPECT_EQ(2, sample.count());
    ::Sleep(30);
  }

  data = foo.Reset();

  // Should be precisely two samples in there
  EXPECT_EQ(2, data.count);

  // Let's hope the scheduler doesn't leave us hanging more than 10 ms.
  // Increased time from 40 to 100 because test failed intermittently on Pulse.
  EXPECT_GE(30 + 70, data.sum);
  EXPECT_LE(14, data.sum);

  // now with zero count
  {
    TimingSample sample(foo, 0);
  }

  data = foo.Reset();

  // Should be no samples in there
  EXPECT_EQ(0, data.count);
}

TEST_F(MetricsTest, Integer) {
  IntegerMetric foo("foo", &coll_);

  EXPECT_EQ(kIntegerType, foo.type());
  IntegerMetric &foo_ref = foo.AsInteger();

  EXPECT_EQ(0, foo.value());
  foo.Set(1005);
  EXPECT_EQ(1005, foo.value());
  foo = 1009UL;
  EXPECT_EQ(1009, foo.value());

  foo.Set(0);

  ++foo;
  EXPECT_EQ(1, foo.value());
  foo++;
  EXPECT_EQ(2, foo.value());

  foo += 100;
  EXPECT_EQ(102, foo.value());

  foo -= 100;
  EXPECT_EQ(2, foo.value());
  foo--;
  EXPECT_EQ(1, foo.value());
  --foo;
  EXPECT_EQ(0, foo.value());
}

TEST_F(MetricsTest, Bool) {
  BoolMetric foo("foo", &coll_);

  EXPECT_EQ(kBoolType, foo.type());
  BoolMetric &foo_ref = foo.AsBool();

  EXPECT_EQ(BoolMetric::kBoolUnset, foo.Reset());
  foo.Set(true);
  EXPECT_EQ(BoolMetric::kBoolTrue, foo.Reset());
  foo.Set(false);
  EXPECT_EQ(BoolMetric::kBoolFalse, foo.Reset());
  EXPECT_EQ(BoolMetric::kBoolUnset, foo.Reset());
}

TEST_F(MetricsEnumTest, Enumeration) {
  MetricBase *metrics[] = {
        &count_,
        &timing_,
        &integer_,
        &bool_,
  };

  for (int i = 0; i < sizeof(metrics) / sizeof(metrics[0]); ++i) {
    MetricBase *stat = metrics[i];
    MetricBase *curr = coll_.first();

    for (; NULL != curr; curr = curr->next()) {
      if (stat == curr)
        break;
    }

    // if NULL, we didn't find our counter
    EXPECT_TRUE(NULL != curr);
  }
}

TEST_F(MetricsEnumTest, Iterator) {
  typedef MetricBase *MetricBasePtr;
  MetricBasePtr metrics[] = { &count_, &timing_, &integer_, &bool_, };
  int num_stats = sizeof(metrics) / sizeof(metrics[0]);

  MetricIterator it(coll_), end;
  EXPECT_NE(it, end);

  // copy construction
  EXPECT_EQ(it, MetricIterator(it));
  EXPECT_EQ(end, MetricIterator(end));

  // # of iterations
  int i = 0;
  while (it++ != end)
    ++i;
  DCHECK_EQ(i, num_stats);
  DCHECK_EQ(it, end);

  // increment past end is idempotent
  ++it;
  DCHECK_EQ(it, end);

  // Check that we return no garbage or nonsense
  for (it = MetricIterator(coll_); it != end; ++it) {
    MetricBasePtr *stats_end = &metrics[num_stats];
    EXPECT_NE(stats_end, std::find(metrics, stats_end, *it));
  }

  // and that all metrics can be found
  for (int i = 0; i < sizeof(metrics) / sizeof(metrics[0]); ++i) {
    MetricBase *stat = metrics[i];

    EXPECT_EQ(stat, *std::find(MetricIterator(coll_), end, stat));
  }
}

TEST_F(MetricsTest, SimpleConstruction) {
  const CountMetric c("c", 100);

  EXPECT_EQ(100, c.value());
  EXPECT_EQ(kCountType, c.type());
  EXPECT_STREQ("c", c.name());
  EXPECT_TRUE(NULL == c.next());

  TimingMetric::TimingData data = { 10, 0, 1000, 10, 500 };
  const TimingMetric t("t", data);

  EXPECT_EQ(10, t.count());
  EXPECT_EQ(1000, t.sum());
  EXPECT_EQ(10, t.minimum());
  EXPECT_EQ(500, t.maximum());
  EXPECT_EQ(kTimingType, t.type());
  EXPECT_STREQ("t", t.name());
  EXPECT_TRUE(NULL == t.next());

  const IntegerMetric i("i", 200);

  EXPECT_EQ(200, i.value());
  EXPECT_EQ(kIntegerType, i.type());
  EXPECT_STREQ("i", i.name());
  EXPECT_TRUE(NULL == i.next());

  const BoolMetric bool_true("bool_true", BoolMetric::kBoolTrue);

  EXPECT_EQ(BoolMetric::kBoolTrue, bool_true.value());
  EXPECT_EQ(kBoolType, bool_true.type());
  EXPECT_STREQ("bool_true", bool_true.name());
  EXPECT_TRUE(NULL == bool_true.next());

  const BoolMetric bool_false("bool_false", BoolMetric::kBoolFalse);

  EXPECT_EQ(BoolMetric::kBoolFalse, bool_false.value());
  EXPECT_EQ(kBoolType, bool_false.type());
  EXPECT_STREQ("bool_false", bool_false.name());
  EXPECT_TRUE(NULL == bool_false.next());
}

