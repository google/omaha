// Copyright 2009 Google Inc.
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
// REMINDER: Identifiers are in accordance with the FIPS180-1 spec

#include "sha.h"
#include "common/debug.h"

namespace omaha {

typedef SecureHashAlgorithm::uint uint;

//const int SecureHashAlgorithm::kDigestSize;

static inline uint f(uint t, uint B, uint C, uint D) {
  if (t < 20)
    return (B & C) | ((~B) & D);
  else if (t < 40)
    return B ^ C ^ D;
  else if (t < 60)
    return (B & C) | (B & D) | (C & D);
  else
    return B ^ C ^ D;
}

static inline uint S(uint n, uint X) {
  return (X << n) | (X >> (32-n));
}

static inline uint K(uint t) {
  if (t < 20)
    return 0x5a827999;
  else if (t < 40)
    return 0x6ed9eba1;
  else if (t < 60)
    return 0x8f1bbcdc;
  else
    return 0xca62c1d6;
}

static inline void swapends(uint& t) {
  t = ((t & 0xff000000) >> 24) |
      ((t & 0xff0000) >> 8) |
      ((t & 0xff00) << 8) |
      ((t & 0xff) << 24);
}

//---------------------------------------------------------------------------

void SecureHashAlgorithm::Init() {
  cursor = 0;
  l = 0;
  H[0] = 0x67452301;
  H[1] = 0xefcdab89;
  H[2] = 0x98badcfe;
  H[3] = 0x10325476;
  H[4] = 0xc3d2e1f0;
}

void SecureHashAlgorithm::Finished() {
  Pad();
  Process();

  for(int t = 0; t < 5; ++t)
    swapends(H[t]);
}

void SecureHashAlgorithm::AddBytes(const void * data, int nbytes) {
  ASSERT(data, (L""));

  const byte * d = reinterpret_cast<const byte *>(data);
  while (nbytes--) {
    M[cursor++] = *d++;
    if (cursor >= 64) this->Process();
    l += 8;
  }
}

void SecureHashAlgorithm::Pad() {
  M[cursor++] = 0x80;
  if (cursor > 64-8) {
    // pad out to next block
    while (cursor < 64)
      M[cursor++] = 0;
    this->Process();
  }
  while (cursor < 64-4)
    M[cursor++] = 0;
  M[64-4] = static_cast<byte>((l & 0xff000000) >> 24);
  M[64-3] = static_cast<byte>((l & 0xff0000) >> 16);
  M[64-2] = static_cast<byte>((l & 0xff00) >> 8);
  M[64-1] = static_cast<byte>((l & 0xff));
}

void SecureHashAlgorithm::Process() {
  uint t;

  // a.
  // CopyMemory(W, M, sizeof(M));
  for (t = 0; t < 16; ++t)
    swapends(W[t]);

  // b.
  for (t = 16; t < 80; ++t)
    W[t] = S(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

  // c.
  A = H[0];
  B = H[1];
  C = H[2];
  D = H[3];
  E = H[4];

  // d.
  for (t = 0; t < 80; ++t) {
    uint TEMP = S(5,A) + f(t,B,C,D) + E + W[t] + K(t);
    E = D;
    D = C;
    C = S(30,B);
    B = A;
    A = TEMP;
  }

  // e.
  H[0] += A;
  H[1] += B;
  H[2] += C;
  H[3] += D;
  H[4] += E;

  cursor = 0;
}

}  // namespace omaha

