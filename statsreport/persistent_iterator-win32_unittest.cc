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
#include "omaha/third_party/gtest/include/gtest/gtest.h"
#include "const-win32.h"
#include "aggregator-win32_unittest.h"
#include "persistent_iterator-win32.h"
#include <atlbase.h>
#include <atlcom.h>
#include <iterator>
#include <map>

using namespace stats_report;

namespace {

class PersistentMetricsIteratorWin32Test: public MetricsAggregatorWin32Test {
public:
  bool WriteStats() {
    // put some persistent metrics into the registry
    MetricsAggregatorWin32 agg(coll_, kAppName);
    AddStats();
    bool ret = agg.AggregateMetrics();

    // Reset the stats, we should now have the same stats
    // in our collection as in registry.
    AddStats();

    return ret;
  }

  typedef std::map<std::string, MetricBase*> MetricsMap;
  void IndexMetrics(MetricsMap *metrics) {
    // build a map over the metrics in our collection
    MetricIterator it(coll_), end;

    for (; it != end; ++it) {
      metrics->insert(std::make_pair(std::string(it->name()), *it));
    }
  }
};

// compare two metrics instances for equality
bool equals(MetricBase *a, MetricBase *b) {
  if (!a || !b)
    return false;

  if (a->type() != b->type() || 0 != strcmp(a->name(), b->name()))
    return false;

  switch (a->type()) {
   case kCountType:
    return a->AsCount().value() == b->AsCount().value();
    break;
   case kTimingType: {
      TimingMetric &at = a->AsTiming();
      TimingMetric &bt = b->AsTiming();

      return at.count() == bt.count() &&
             at.sum() == bt.sum() &&
             at.minimum() == bt.minimum() &&
             at.maximum() == bt.maximum();
    }
    break;
   case kIntegerType:
    return a->AsInteger().value() == b->AsInteger().value();
    break;
   case kBoolType:
    return a->AsBool().value() == b->AsBool().value();
    break;

   case kInvalidType:
   default:
    LOG(FATAL) << "Impossible metric type";
  }

  return false;
}

} // namespace

TEST_F(PersistentMetricsIteratorWin32Test, Basic) {
  EXPECT_TRUE(WriteStats());
  PersistentMetricsIteratorWin32 a, b, c(kAppName);

  EXPECT_TRUE(a == b);
  EXPECT_TRUE(b == a);

  EXPECT_FALSE(a == c);
  EXPECT_FALSE(b == c);
  EXPECT_FALSE(c == a);
  EXPECT_FALSE(c == b);

  ++a;
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(b == a);
}

// Test to see whether we can reliably roundtrip metrics through
// the registry without molestation
TEST_F(PersistentMetricsIteratorWin32Test, UnmolestedValues) {
  EXPECT_TRUE(WriteStats());

  MetricsMap metrics;
  IndexMetrics(&metrics);

  PersistentMetricsIteratorWin32 it(kAppName), end;
  int count = 0;
  for (; it != end; ++it) {
    MetricsMap::iterator found = metrics.find(it->name());

    // make sure we found it, and that it's unmolested in value
    EXPECT_TRUE(found != metrics.end() && equals(found->second, *it));
    count++;
  }

  // Did we visit all metrics?
  EXPECT_EQ(count, metrics.size());
}
