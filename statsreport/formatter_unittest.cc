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
#include "formatter.h"

using stats_report::Formatter;

TEST(Formatter, Format) {
  Formatter formatter("test_application", 86400);
  
  formatter.AddCount("count1", 10);
  formatter.AddTiming("timing1", 2, 150, 50, 200);
  formatter.AddInteger("integer1", 3000);
  formatter.AddBoolean("boolean1", true);
  formatter.AddBoolean("boolean2", false);
  
  EXPECT_STREQ("test_application&86400"
               "&count1:c=10"
               "&timing1:t=2;150;50;200"
               "&integer1:i=3000"
               "&boolean1:b=t"
               "&boolean2:b=f",
               formatter.output());
}