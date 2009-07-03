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
// sha_unittest.cpp
//
// Unit test functions for SHA

#include <cstring>

#include "omaha/common/sha.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// From the Original Eric Fredricksen implementation
TEST(ShaTest, Digest) {

  const char * const abc_digest =
      "\xA9\x99\x3E\x36"
      "\x47\x06\x81\x6A"
      "\xBA\x3E\x25\x71"
      "\x78\x50\xC2\x6C"
      "\x9C\xD0\xD8\x9D";

  { // FIPS 180-1 Appendix A example
    SecureHashAlgorithm sha1;
    sha1.AddBytes("abc", 3);
    sha1.Finished();
    ASSERT_TRUE(!memcmp(sha1.Digest(), abc_digest, 20));

    // do it again to make sure Init works
    sha1.Init();
    sha1.AddBytes("abc", 3);
    sha1.Finished();
    ASSERT_TRUE(!memcmp(sha1.Digest(), abc_digest, 20));
  }

  const char * const multiblock_digest =
      "\x84\x98\x3E\x44"
      "\x1C\x3B\xD2\x6E"
      "\xBA\xAE\x4A\xA1"
      "\xF9\x51\x29\xE5"
      "\xE5\x46\x70\xF1";

  { // FIPS 180-1 Appendix A example
    SecureHashAlgorithm sha1;
    char * to_hash = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha1.AddBytes(to_hash, strlen(to_hash));
    sha1.Finished();
    ASSERT_TRUE(!memcmp(sha1.Digest(), multiblock_digest, 20));
  }
}

}  // namespace omaha

