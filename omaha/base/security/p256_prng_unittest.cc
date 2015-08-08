// Copyright 2013 Google Inc.
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

#include <stdio.h>
#include <string.h>
#include "p256_prng.h"
#include "omaha/testing/unit_test.h"

static const uint8_t expected_buffer[P256_PRNG_SIZE] = {
  0x3c,0x0a,0x3f,0xdc,0x53,0x13,0xde,0x17,
  0x5f,0xad,0xc5,0x53,0xd7,0x6b,0xc6,0x28,
  0x40,0x37,0x23,0x3c,0x26,0x80,0xe0,0xcd,
  0x4c,0xf6,0xfd,0x11,0x28,0x18,0xfe,0x31
};

TEST(P256_PRNG, P256_PRNG_Test) {
  P256_PRNG_CTX ctx;
  uint8_t rnd[P256_PRNG_SIZE];

  p256_prng_init(&ctx, "p256_prng_test", 14, 0);

  EXPECT_EQ(0, ctx.instance_count);
  EXPECT_EQ(0, ctx.call_count);

  p256_prng_draw(&ctx, rnd);

  EXPECT_EQ(0, ctx.instance_count);
  EXPECT_EQ(1, ctx.call_count);

  p256_prng_draw(&ctx, rnd);

  EXPECT_EQ(0, ctx.instance_count);
  EXPECT_EQ(2, ctx.call_count);

  // Compare prng output against known good output.
  // This catches endianess issues and algorigthm changes.
  EXPECT_EQ(0, memcmp(rnd, expected_buffer, P256_PRNG_SIZE));
}

