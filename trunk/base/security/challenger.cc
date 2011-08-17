// Copyright 2007-2009 Google Inc.
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

#include "challenger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rsa.h"
#include "md5.h"
#include "aes.h"
#include "b64.h"

// Windows compilers do not have C99 support yet.
#if defined(WIN32) || defined(_WIN32)
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

Challenger::Challenger(RSA::PublicKey pkey,
                       const unsigned char* seed, int seed_size)
    : rsa_(pkey) {
  memset(count_, 0, sizeof(count_));
  // Use seed as key for AES. Compress seed first.
  MD5(seed, seed_size, seed_);
}

const char* Challenger::challenge() {
  uint8_t ctr[AES_BLOCK_SIZE];

  // Compute current challenge.
  AES_encrypt_block(seed_, count_, ctr);

  // Increment count for future fresh challenges.
  for (size_t i = 0; i < sizeof(count_) && !++count_[i]; ++i);

  // Prepend our version number.
  char* p = challenge_;
  p += snprintf(challenge_, sizeof(challenge_), "%d:", rsa_.version());

  // Append our current challenge.
  B64_encode(ctr, sizeof(ctr), p, sizeof(challenge_) - (p - challenge_));

  return challenge_;
}

bool Challenger::verify(const char* hash, const char* signature) const {
  char message[128];
  uint8_t sigbuf[128];

  // Expect exactly 128 bytes of decoded signature data.
  if (B64_decode(signature, sigbuf, sizeof(sigbuf)) != sizeof(sigbuf))
    return false;

  // Verify signature with baked-in public key and recover embedded message.
  int result = rsa_.verify(sigbuf, sizeof(sigbuf),
                           message, sizeof(message) - 1);

  if (result < 0 || result >= static_cast<int>(sizeof(message) - 1))
    return false;

  // Since we're expecting a textual message, 0-terminate it.
  message[result] = '\0';

  // Construct and compare expected against received signed message.
  char expected[128];
  snprintf(expected, sizeof(expected), "%s:%s", challenge_, hash);

  return !strcmp(expected, message);
}
