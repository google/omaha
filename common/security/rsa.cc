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

#include "rsa.h"

#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "md5.h"
#include "aes.h"
#include "sha.h"
#include "rc4.h"

#define DINV mod[0]
#define RR(i) mod[1 + 2*(i)]
#define MOD(i) (mod[2 + 2*(i)] + mod[1 + 2*(i)])  // +mod[1+2*(i)] to deobscure

//
// a[] -= M
//
static void subM(uint32_t* a, const uint32_t* mod, int len) {
  int64_t A = 0;
  for (int i = 0; i < len; ++i) {
    A += (uint64_t)a[i] - MOD(i);
    a[i] = (uint32_t)A;
    A >>= 32;
  }
}

//
// return a[] >= M
//
static bool geM(const uint32_t* a, const uint32_t* mod, int len) {
  for (int i = len; i;) {
    --i;
    if (a[i] < MOD(i)) return false;
    if (a[i] > MOD(i)) return true;
  }
  return true;  // equal
}

//
// montgomery c[] += a * b[] / R mod M
//
static void montMulAdd(uint32_t* c,
                       uint32_t a,
                       const uint32_t* b,
                       const uint32_t* mod,
                       int len) {
  uint64_t A = (uint64_t)a * b[0] + c[0];
  uint32_t d0 = (uint32_t)A * DINV;
  uint64_t B = (uint64_t)d0 * MOD(0) + (uint32_t)A;

  int i = 1;
  for (; i < len; ++i) {
    A = (A >> 32) + (uint64_t)a * b[i] + c[i];
    B = (B >> 32) + (uint64_t)d0 * MOD(i) + (uint32_t)A;
    c[i - 1] = (uint32_t)B;
  }

  A = (A >> 32) + (B >> 32);

  c[i - 1] = (uint32_t)A;

  if ((A >> 32)) {  // proper probablistic padding could avoid this?
    subM(c, mod, len);  // or moduli without the highest bit set..
  }
}

//
// montgomery c[] = a[] * R^2 / R mod M (= a[] * R mod M)
//
static void montMulR(uint32_t* c,
                     const uint32_t* a,
                     const uint32_t* mod,
                     int len) {
  memset(c, 0, len * sizeof(uint32_t));

  for (int i = 0; i < len; ++i) {
    montMulAdd(c, RR(i), a, mod, len);
  }
}

//
// montgomery c[] = a[] * b[] / R mod M
//
static void montMul(uint32_t* c,
                    const uint32_t* a,
                    const uint32_t* b,
                    const uint32_t* mod,
                    int len) {
  memset(c, 0, len * sizeof(uint32_t));

  for (int i = 0; i < len; ++i) {
    montMulAdd(c, a[i], b, mod, len);
  }
}


//
// In-place public exponentiation.
// Input and output big-endian byte array.
// Returns 0 on failure or # uint8_t written in inout (always inout_len).
//
int RSA::raw(uint8_t* inout, int inout_len) const {
  const uint32_t* mod = &pkey_[1];
  int len = *mod++;

  if (len > kMaxWords)
    return 0;  // Only work with up to 2048 bit moduli.
  if ((len * 4) != inout_len)
    return 0;  // Input length should match modulus length.

  uint32_t a[kMaxWords];

  // Convert from big endian byte array to little endian word array.
  for (int i = 0; i < len; ++i) {
    uint32_t tmp =
      (inout[((len - 1 - i) * 4) + 0] << 24) |
      (inout[((len - 1 - i) * 4) + 1] << 16) |
      (inout[((len - 1 - i) * 4) + 2] << 8) |
      (inout[((len - 1 - i) * 4) + 3] << 0);
    a[i] = tmp;
  }

  uint32_t aR[kMaxWords];
  uint32_t aaR[kMaxWords];
  uint32_t aaa[kMaxWords];

  montMulR(aR, a, mod, len);       // aR = a * R mod M
  montMul(aaR, aR, aR, mod, len);  // aaR = a^2 * R mod M
  montMul(aaa, aaR, a, mod, len);  // aaa = a^3 mod M

  // Make sure aaa < mod; aaa is at most 1x mod too large.
  if (geM(aaa, mod, len)) {
    subM(aaa, mod, len);
  }

  // Convert to bigendian byte array
  int reslen = 0;

  for (int i = len - 1; i >= 0; --i) {
    uint32_t tmp = aaa[i];
    inout[reslen++] = tmp >> 24;
    inout[reslen++] = tmp >> 16;
    inout[reslen++] = tmp >> 8;
    inout[reslen++] = tmp >> 0;
  }

  return reslen;
}

//
// Verify a Google style padded message recovery signature and return the
// message.
//
int RSA::verify(const uint8_t* data, int data_len,
                void* output, int output_len) const {
  uint8_t res[kMaxWords * 4];

  if (data_len < 0 || data_len > (kMaxWords * 4))
    return 0;  // Input too big, 2048 bit max.

  memcpy(res, data, data_len);

  int reslen = this->raw(res, data_len);

  if (!reslen) return 0;

  uint8_t md5[16];

  MD5(res, reslen - 16, md5);

  for (int i = 0; i < 16; ++i) {
    res[reslen - 16 + i] ^= md5[i];
  }

  // Unmask low part using high part as ofb key.
  uint8_t iv[16] = {0};

  for (int i = 0; i < reslen - 16; i++) {
    if (!(i & 15))
      AES_encrypt_block(res + reslen - 16, iv, iv);
    res[i] ^= iv[i & 15];
  }

  res[0] &= 127;
  res[0] %= reslen - 16 - 16;

  bool result = true;

  // Verify high part is hash of random in low part.
  MD5(res + 1, res[0] + 16, md5);
  for (int i = 0; i < 16; ++i) {
    result = result && (res[reslen - 16 + i] == md5[i]);
  }

  if (!result) {
    return 0;  // verification failure
  }

  // Copy message into output[]
  if (res[0] > output_len) {
    return 0;  // output too small, return failure
  }

  memcpy(output, res + 1, res[0]);

  return res[0];
}

//
// Hybrid encrypt message.
// Make up RC4 key using seed and hash of msg.
// Wrap key with RSA, encrypt msg with RC4.
//
int RSA::encrypt(const uint8_t* msg, int msg_len,
                 const void* seed, int seed_len,
                 uint8_t* output, int output_max) const {
  int output_len = this->encryptedSize(msg_len);
  if (output_max < 0 || output_max < output_len)
    return 0;

  int header_size = output_len - msg_len;  // Our added overhead.

  // Hash of message. Least significant SHA_DIGEST_SIZE bytes of RSA number.
  uint8_t* hash = &output[header_size - SHA_DIGEST_SIZE];
  SHA(msg, msg_len, hash);

  // Hash(Hash(message) | seed).
  SHA_CTX sha;
  SHA_init(&sha);
  SHA_update(&sha, hash, SHA_DIGEST_SIZE);
  SHA_update(&sha, seed, seed_len);

  // Use this Hash(Hash(message) | seed) as RC4 key for prng.
  RC4_CTX rc4;
  RC4_setKey(&rc4, SHA_final(&sha), SHA_DIGEST_SIZE);
  RC4_discard(&rc4, 1536);  // Drop some to warm up RC4.

  uint8_t* key = &output[1 + 4];

  // Prng conjure some bytes.
  RC4_stream(&rc4, key, this->size() - SHA_DIGEST_SIZE);
  key[0] &= 127;  // Drop top bit to be less than modulus.

  // Mask plaintext hash with hash of prng part.
  SHA_init(&sha);
  SHA_update(&sha, key, this->size() - SHA_DIGEST_SIZE);
  const uint8_t* mask = SHA_final(&sha);
  for (int i = 0; i < SHA_DIGEST_SIZE; ++i)
    hash[i] ^= mask[i];

  // Use entire RSA number as content encryption key.
  RC4_setKey(&rc4, key, this->size());
  RC4_discard(&rc4, 1536);  // Warm up RC4.

  // Output wire-format version, single 0 byte.
  output[0] = 0;

  // Output version, msb first.
  uint32_t version = this->version();
  output[1] = version >> 24;
  output[2] = version >> 16;
  output[3] = version >> 8;
  output[4] = version >> 0;

  // Wrap key data with public RSA key.
  if (!this->raw(key, this->size()))
    return 0;

  // Append encrypted message.
  RC4_crypt(&rc4, msg, &output[header_size], msg_len);

  return output_len;
}
