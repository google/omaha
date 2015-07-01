// Copyright 2011 Google Inc.
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
#include "omaha/common/protocol_definition.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(XmlRequestPingTest, ConstructorDefaultValue) {
  xml::request::Ping ping;

  const bool was_active = ping.active == ACTIVE_RUN;
  const bool need_active = ping.active != ACTIVE_UNKNOWN;
  const bool has_sent_a_today = ping.days_since_last_active_ping == 0;
  const bool need_a = was_active && !has_sent_a_today;
  const bool need_r = ping.days_since_last_roll_call != 0;
  const bool need_ad = was_active && ping.day_of_last_activity != 0;
  const bool need_rd = ping.day_of_last_roll_call != 0;

  EXPECT_FALSE(need_active);
  EXPECT_FALSE(need_a);
  EXPECT_FALSE(need_r);
  EXPECT_FALSE(need_ad);
  EXPECT_FALSE(need_rd);
}

}  // namespace omaha
