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
//
// Simple pseudo-random number generator.
// Not intended for anything but random-to-humans numbers.
// Very fast, and has a period of 32768 for all seed values (including zero).
// Returns values in the range 0..0xFFFF (inclusive).
//

#include "omaha/base/tr_rand.h"

namespace omaha {

static int rand_val = 0;

void tr_srand(unsigned int seed) {
  rand_val = seed & 0xFFFF;
}

int tr_rand() {
  rand_val = ((rand_val * 75) + 1) & 0xFFFF;
  return rand_val;
}

}  // namespace omaha

