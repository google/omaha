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

#ifndef OMAHA_COMMON_SECURITY_RSA_H__
#define OMAHA_COMMON_SECURITY_RSA_H__

#include <inttypes.h>

class RSA {
 public:
  typedef const uint32_t PublicKeyInstance[];
  typedef const uint32_t* PublicKey;

  // Public_key as montgomery precomputed array
  explicit RSA(PublicKey public_key) : pkey_(public_key) {}

  // Verifies a Google style RSA message recovery signature.
  //
  // sig[] signature to verify, big-endian byte array.
  // sig_len length of sig[] in bytes.
  // If verified successfully, output receives the recovered
  // message and the function returns the number of bytes.
  // If not successful, the function returns 0.
  // (empty message is not a useful message)
  int verify(const uint8_t* sig, int sig_len,
             void* output, int output_max) const;

  // Hybrid encrypt message.
  //
  // output_max should be at least encryptedSize(msg_len)
  // Returns 0 on failure, # output bytes on success.
  int encrypt(const uint8_t* msg, int msg_len,
              const void* seed, int seed_len,
              uint8_t* output, int output_max) const;

  int encryptedSize(int len) const {
    return len + 1 + 4 + size();
  }

  // Performs in-place public key exponentiation.
  //
  // Input_len should match size of modulus in bytes.
  // Returns 0 on failure, # of bytes written on success.
  int raw(uint8_t* input, int input_len) const;

  int version() const { return pkey_[0]; }
  int size() const { return pkey_[1] * 4; }

 private:
  const PublicKey pkey_;
  static const int kMaxWords = 64;
};

#endif  // OMAHA_COMMON_SECURITY_RSA_H__
