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

#ifndef OMAHA_COMMON_SHA_H__
#define OMAHA_COMMON_SHA_H__

#include <wtypes.h>
#include "base/basictypes.h"

namespace omaha {

// Implementation of SHA-1. Only handles data in byte-sized blocks,
// which simplifies the code a fair bit.
//
// NOTE: All identifier names follow the notation in FIPS PUB 180-1 for
// easy debugging. http://www.google.com/search?q=lucky:+FIPS+PUB+180-1
//
// Usage example:
//
// SecureHashAlgorithm sha;
// while(there is data to hash)
//   sha.AddBytes(moredata, size of data);
// sha.Finished();
// CopyMemory(somewhere, sha.Digest(), 20);
//
// To reuse the instance of sha, call sha.Init();
//
// This class is NOT threadsafe!


class SecureHashAlgorithm {

 public:
  SecureHashAlgorithm() { Init(); }

  typedef unsigned int uint;         // to keep with notation in the spec

  static const int kDigestSize = 20;   // 20 bytes of output

  void Init();                                   // resets internal registers
  void AddBytes(const void * data, int nbytes);  // processes bytes
  void Finished();                               // computes final hash

  // extracts the digest (20 bytes)
  const unsigned char * Digest() const {
    return reinterpret_cast<const unsigned char *>(H);
  }

 private:
  void Pad();           // if length of input is less than blocksize
  void Process();       // does some rounds

  uint A,B,C,D,E;       // internal registers; see spec

  uint H[5];            // 20 bytes of message digest

  union {
    uint W[80];         // see spec
    byte M[64];
  };

  uint cursor;          // placekeeping variables; see spec
  uint l;

  DISALLOW_EVIL_CONSTRUCTORS(SecureHashAlgorithm);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SHA_H__
