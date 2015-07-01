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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "p256.h"
#include "p256_prng.h"
#include "omaha/testing/unit_test.h"

static int count_bits(const p256_int* a) {
  int i, n = 0;
  for (i = 0; i < 256; ++i) {
    n += p256_get_bit(a, i);
  }
  return n;
}

TEST(P256, TestShifts) {
  p256_int a = {{1}};
  p256_int b;
  int i;

  // First shift bit up one step at a time.
  for (i = 0; i < 255; ++i) {
    EXPECT_TRUE(p256_get_bit(&a, i) == 1);
    EXPECT_FALSE(p256_is_zero(&a));
    EXPECT_TRUE(p256_shl(&a, 1, &a) == 0);
    EXPECT_TRUE(p256_get_bit(&a, i) == 0);
    EXPECT_TRUE(count_bits(&a) == 1);
  }
  EXPECT_TRUE(p256_get_bit(&a, i) == 1);
  EXPECT_FALSE(p256_is_zero(&a));

  // Shift bit out top.
  EXPECT_TRUE(p256_shl(&a, 1, &b) == 1);
  EXPECT_TRUE(p256_get_bit(&b, i) == 0);
  EXPECT_TRUE(p256_is_zero(&b));

  // Shift bit back down.
  for (; i > 0; --i) {
    EXPECT_TRUE(p256_get_bit(&a, i) == 1);
    EXPECT_FALSE(p256_is_zero(&a));
    p256_shr(&a, 1, &a);
    EXPECT_TRUE(p256_get_bit(&a, i) == 0);
    EXPECT_TRUE(count_bits(&a) == 1);
  }

  EXPECT_TRUE(p256_get_bit(&a, i) == 1);
  EXPECT_FALSE(p256_is_zero(&a));

  // Shift bit out bottom.
  p256_shr(&a, 1, &a);
  EXPECT_TRUE(p256_is_zero(&a));
}

TEST(P256, AddSubCmp) {
  p256_int a = {{1}};
  p256_int b;
  p256_int one = {{1}};
  int i;

  for (i = 0; i < 255; ++i) {
    EXPECT_TRUE(count_bits(&a) == 1);
    EXPECT_TRUE(p256_sub(&a, &one, &b) == 0);
    EXPECT_TRUE(p256_cmp(&a, &b) == 1);
    EXPECT_TRUE(p256_cmp(&b, &a) == -1);
    EXPECT_TRUE(count_bits(&b) == i);
    EXPECT_TRUE(p256_add(&b, &one, &b) == 0);
    EXPECT_TRUE(count_bits(&b) == 1);
    EXPECT_TRUE(p256_cmp(&b, &a) == 0);

    EXPECT_TRUE(p256_shl(&a, 1, &a) == 0);
  }

  EXPECT_TRUE(p256_add(&a, &a, &b) == 1);  // expect carry
  EXPECT_TRUE(p256_is_zero(&b));
  EXPECT_TRUE(p256_cmp(&b, &a) == -1);
  EXPECT_TRUE(p256_sub(&b, &one, &b) == -1);  // expect borrow
  EXPECT_TRUE(p256_cmp(&b, &a) == 1);
}

TEST(P256, TestMulInv) {
  p256_int a = {{1}};
  p256_int one = {{1}};
  p256_int b, c;
  int i;

  for (i = 0; i < 255; ++i) {
    p256_modinv(&SECP256r1_n, &a, &b);  // b = 1/a
    p256_modmul(&SECP256r1_n, &a, 0, &b, &c);  // c = b * a = 1/a * a = 1
    EXPECT_TRUE(p256_cmp(&c, &one) == 0);

    p256_modinv_vartime(&SECP256r1_n, &b, &c);  // c = 1/b = 1/1/a = a
    EXPECT_TRUE(p256_cmp(&a, &c) == 0);

    EXPECT_TRUE(p256_shl(&a, 1, &a) == 0);
  }
}
