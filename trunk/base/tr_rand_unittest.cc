// Copyright 2004-2009 Google Inc.
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

#include <cstring>
#include "omaha/base/tr_rand.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(TRRandTest, TRRand) {
  int min_period = +99999;
  int max_period = -99999;

  int min_period_at = -99999;
  int max_period_at = -99999;

  byte hits[65536] = {0};
  memset(hits, 0, sizeof(hits));

  // Compute minimum and maximum period by considering all possible seed values.
  for (int seed = 0; seed < 65536; ++seed) {
    // See if value is part of some known sequence we've traversed.
    // If multiple values map to same next-val, this check could cause us to
    // report a min_period that's too short. But a long min_period still
    // indicates success.
    if (hits[seed]) { continue; }

    // Compute length of period starting at this seed.
    tr_srand(seed);
    int i = seed;
    int period = 0;
    do {
      ++hits[i];
      ++period;
      i = tr_rand();
      ASSERT_GE(i, 0);
    } while (hits[i] == 0);

    // Update stats.
    if (period < min_period) {
      min_period = period;
      min_period_at = seed;
    }
    if (period > max_period) {
      max_period = period;
      max_period_at = seed;
    }
  }
  ASSERT_GE(min_period, (0xFFFF / 2));
}

}  // namespace omaha

