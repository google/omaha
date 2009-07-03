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
#include "aggregator-win32_unittest.h"
#include "aggregator_unittest.h"
#include "omaha/third_party/gtest/include/gtest/gtest.h"

using namespace stats_report;

#define APP_NAME_STRING L"aggregator-win32_unittest"
#define PREFIX_KEY_STRING L"Software\\Google\\" 
#define SUFFIX_KEY_STRING L"\\UsageStats\\Daily"
#define ROOT_KEY_STRING PREFIX_KEY_STRING APP_NAME_STRING 
#define KEY_STRING ROOT_KEY_STRING SUFFIX_KEY_STRING

const wchar_t MetricsAggregatorWin32Test::kAppName[] = APP_NAME_STRING;
const wchar_t MetricsAggregatorWin32Test::kRootKeyName[] = ROOT_KEY_STRING;
const wchar_t MetricsAggregatorWin32Test::kCountsKeyName[] = 
                                                      KEY_STRING L"\\Counts";
const wchar_t MetricsAggregatorWin32Test::kTimingsKeyName[] = 
                                                      KEY_STRING L"\\Timings";
const wchar_t MetricsAggregatorWin32Test::kIntegersKeyName[] = 
                                                      KEY_STRING L"\\Integers";
const wchar_t MetricsAggregatorWin32Test::kBoolsKeyName[] = 
                                                      KEY_STRING L"\\Booleans";


#define EXPECT_REGVAL_EQ(value, key_name, value_name) do { \
  char buf[sizeof(value)]; \
  ULONG len = sizeof(buf); \
  CRegKey key; \
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name)); \
  EXPECT_EQ(ERROR_SUCCESS, key.QueryBinaryValue(value_name, buf, &len)); \
  EXPECT_EQ(sizeof(buf), len); \
  EXPECT_EQ(0, memcmp(&value, buf, sizeof(buf))); \
} while(0)

TEST_F(MetricsAggregatorWin32Test, AggregateWin32) {
  MetricsAggregatorWin32 agg(coll_, kAppName);

  EXPECT_TRUE(agg.AggregateMetrics());
  AddStats();  
  EXPECT_TRUE(agg.AggregateMetrics());  

  {
    int64 one = 1, two = 2;
    EXPECT_REGVAL_EQ(one, kCountsKeyName, L"c1");
    EXPECT_REGVAL_EQ(two, kCountsKeyName, L"c2");

    TimingMetric::TimingData data1 = { 2, 0, 1500, 500, 1000 };  
    TimingMetric::TimingData data2 = { 2, 0, 2030, 30, 2000 };  
    EXPECT_REGVAL_EQ(data1, kTimingsKeyName, L"t1");
    EXPECT_REGVAL_EQ(data2, kTimingsKeyName, L"t2");

    EXPECT_REGVAL_EQ(one, kIntegersKeyName, L"i1");
    EXPECT_REGVAL_EQ(two, kIntegersKeyName, L"i2");

    int32 bool_true = 1, bool_false = 0;
    EXPECT_REGVAL_EQ(bool_true, kBoolsKeyName, L"b1");
    EXPECT_REGVAL_EQ(bool_false, kBoolsKeyName, L"b2");
  }
  
  AddStats();  
  EXPECT_TRUE(agg.AggregateMetrics());  

  {
    int64 two = 2, four = 4;
    EXPECT_REGVAL_EQ(two, kCountsKeyName, L"c1");
    EXPECT_REGVAL_EQ(four, kCountsKeyName, L"c2");

    TimingMetric::TimingData data1 = { 4, 0, 3000, 500, 1000 };  
    TimingMetric::TimingData data2 = { 4, 0, 4060, 30, 2000 };  
    EXPECT_REGVAL_EQ(data1, kTimingsKeyName, L"t1");
    EXPECT_REGVAL_EQ(data2, kTimingsKeyName, L"t2");

    int64 one = 1;
    EXPECT_REGVAL_EQ(one, kIntegersKeyName, L"i1");
    EXPECT_REGVAL_EQ(two, kIntegersKeyName, L"i2");

    int32 bool_true = 1, bool_false = 0;
    EXPECT_REGVAL_EQ(bool_true, kBoolsKeyName, L"b1");
    EXPECT_REGVAL_EQ(bool_false, kBoolsKeyName, L"b2");
  }
}
