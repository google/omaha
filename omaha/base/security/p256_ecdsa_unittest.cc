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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>


#include "p256.h"
#include "p256_ecdsa.h"
#include "p256_prng.h"
#include "omaha/testing/unit_test.h"




// Create and verify some random signatures.
// time(NULL) is used as prng seed so repeat runs test different values.
TEST(P256_ECDSA, RandomSigsTest) {
  int n;
  P256_PRNG_CTX prng;
  uint8_t tmp[P256_PRNG_SIZE];
  uint32_t boot_count = static_cast<uint32_t>(time(NULL));

  // Setup deterministic prng.
  p256_prng_init(&prng, "random_sigs_test", 16, boot_count);

  for (n = 0; n < 100; ++n) {
    p256_int a, b, Gx, Gy;
    p256_int r, s;

    // Make up private key
    do {
      // Pick well distributed random number 0 < a < n.
      p256_int p1, p2;
      p256_prng_draw(&prng, tmp);
      p256_from_bin(tmp, &p1);
      p256_prng_draw(&prng, tmp);
      p256_from_bin(tmp, &p2);
      p256_modmul(&SECP256r1_n, &p1, 0, &p2, &a);
    } while (p256_is_zero(&a));

    // Compute public key; a is our secret key.
    p256_base_point_mul(&a, &Gx, &Gy);

    // Pick random message to sign.
    p256_prng_draw(&prng, tmp);
    p256_from_bin(tmp, &b);

    // Compute signature on b.
    p256_ecdsa_sign(&a, &b, &r, &s);

    EXPECT_TRUE(p256_ecdsa_verify(&Gx, &Gy, &b, &r, &s));
  }
}

// Test signature parameter validation.
TEST(P256_ECDSA, InvalidSigsTest) {
  P256_PRNG_CTX prng;
  uint8_t tmp[P256_PRNG_SIZE];
  uint32_t boot_count = static_cast<uint32_t>(time(NULL));

  p256_prng_init(&prng, "invalid_sigs_test", 17, boot_count);

  {
    p256_int a, b, Gx, Gy;
    p256_int r, s;
    p256_int one = P256_ONE;
    p256_int zero = P256_ZERO;

    (void)one;
    (void)zero;

    // Make up private key.
    do {
      // Pick well distributed random number 0 < a < n.
      p256_int p1, p2;
      p256_prng_draw(&prng, tmp);
      p256_from_bin(tmp, &p1);
      p256_prng_draw(&prng, tmp);
      p256_from_bin(tmp, &p2);
      p256_modmul(&SECP256r1_n, &p1, 0, &p2, &a);
    } while (p256_is_zero(&a));

    // Compute public key; a is our secret key.
    p256_base_point_mul(&a, &Gx, &Gy);

    // Pick random message to sign.
    p256_prng_draw(&prng, tmp);
    p256_from_bin(tmp, &b);

    // Compute signature on b.
    p256_ecdsa_sign(&a, &b, &r, &s);

    EXPECT_TRUE(p256_ecdsa_verify(&Gx, &Gy, &b, &r, &s));
  }
}

