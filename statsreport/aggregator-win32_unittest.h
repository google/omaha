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
#ifndef OMAHA_STATSREPORT_AGGREGATOR_WIN32_UNITTEST_H__
#define OMAHA_STATSREPORT_AGGREGATOR_WIN32_UNITTEST_H__

#include "aggregator_unittest.h"
#include "aggregator-win32.h"

/// Shared test fixture for win32 unit tests
class MetricsAggregatorWin32Test: public MetricsAggregatorTest {
public:
  virtual void SetUp() {
    // clean the registry
    SHDeleteKey(HKEY_CURRENT_USER, kRootKeyName);
    MetricsAggregatorTest::SetUp();
  }
  virtual void TearDown() {
    MetricsAggregatorTest::TearDown();
    SHDeleteKey(HKEY_CURRENT_USER, kRootKeyName);
  }

  void AddStats() {
    ++c1_;
    ++c2_;
    ++c2_;

    t1_.AddSample(1000);
    t1_.AddSample(500);

    t2_.AddSample(2000);
    t2_.AddSample(30);

    i1_ = 1;
    i2_ = 2;

    b1_ = true;
    b2_ = false;
  }

  static const wchar_t kAppName[];
  static const wchar_t kRootKeyName[];
  static const wchar_t kCountsKeyName[];
  static const wchar_t kTimingsKeyName[];
  static const wchar_t kIntegersKeyName[];
  static const wchar_t kBoolsKeyName[];
};

#endif  // OMAHA_STATSREPORT_AGGREGATOR_WIN32_UNITTEST_H__
