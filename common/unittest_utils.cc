// Copyright 2003-2009 Google Inc.
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
// unittest_utils.cpp

#include <windows.h>
#include "omaha/common/unittest_utils.h"
#include "omaha/common/string.h"
#include "omaha/common/tr_rand.h"

namespace omaha {

// ---------------------------------------------------
// Unittest utils
//

int32 UnittestUtils::GetRandomInt32() {
  int32 x = 0;

  // rand() returns a value between 0x0000 and 0x7fff
  // that's only 16 bits, so we shift by 16
  // and since it's 0x7fff, we randomly flip the last bit

  int32 a = tr_rand();
  if (tr_rand() % 2 == 0) {
    a = a | 0x8000;
  }

  int32 b = tr_rand();
  if (tr_rand() % 2 == 0) {
    b = b | 0x8000;
  }

  x = (x | a) << 16;
  x = x | b;

  return x;
}

int64 UnittestUtils::GetRandomInt64() {
  int64 x = 0;

  // rand() returns a value between 0x0000 and 0x7fff
  // that's only 16 bits, so we shift by 16
  // and since it's 0x7fff, we randomly flip the last bit

  int32 a = tr_rand();
  if (tr_rand() % 2 == 0) {
    a = a | 0x8000;
  }

  int32 b = tr_rand();
  if (tr_rand() % 2 == 0) {
    b = b | 0x8000;
  }

  int32 c = tr_rand();
  if (tr_rand() % 2 == 0) {
    c = c | 0x8000;
  }

  int32 d = tr_rand();
  if (tr_rand() % 2 == 0) {
    d = d | 0x8000;
  }

  x = (x | a) << 16;
  x = (x | b) << 16;
  x = (x | c) << 16;
  x = x | d;

  return x;
}

char UnittestUtils::RandomPrintableCharacter() {
  // +1 is to make it inclusive
  return kMinASCIIPrintable +
         (tr_rand() % (kMaxASCIIPrintable - kMinASCIIPrintable + 1));
}

CString UnittestUtils::CreateRandomString(uint32 length) {
  CString cstr;
  if (length > 0) {
    char* rand_chars = new char[length];
    if (rand_chars != NULL) {
      for (uint32 i = 0; i < length; ++i) {
        rand_chars[i] = RandomPrintableCharacter();
      }

      WCHAR* rand_wchars = ToWide(rand_chars, length);
      cstr = rand_wchars;

      delete[] rand_wchars;
      delete[] rand_chars;
      return cstr;
    }
  }
  return cstr;
}

bool UnittestUtils::CreateRandomByteArray(uint32 length, byte out_bytes[]) {
  if (length > 0) {
      for (uint32 i = 0; i < length; ++i) {
        out_bytes[i] = static_cast<byte>(tr_rand() % 256);
      }
      return true;
  }
  return false;
}

}  // namespace omaha

