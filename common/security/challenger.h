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

#ifndef OMAHA_COMMON_SECURITY_CHALLENGER_H__
#define OMAHA_COMMON_SECURITY_CHALLENGER_H__

#include <inttypes.h>

#include "md5.h"
#include "aes.h"
#include "rsa.h"

class Challenger {
 public:
  // Instantiate internal PRNG with seed. Use a proper method on your
  // target platform to collect some entropy. For windows for instance,
  // use CryptoAPI; on unix, read some from /dev/urandom.
  // 128 bits of entropy is plenty.
  explicit Challenger(RSA::PublicKey public_key,
                      const uint8_t* seed, int seed_size);

  // Not a const method! Every call updates internal state and never
  // are identical challenges returned.
  // Returns WebSafe base64 encoded string.
  const char* challenge();

  // Verifies whether signature contains current challenge and hash.
  // Arguments are expected to be WebSafe base64 encoded strings.
  bool verify(const char* hash, const char* signature) const;

 private:
  char challenge_[64];
  uint8_t count_[AES_BLOCK_SIZE];
  uint8_t seed_[MD5_DIGEST_SIZE];
  RSA rsa_;
};

#endif  // OMAHA_COMMON_SECURITY_CHALLENGER_H__
