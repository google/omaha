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
// unittest_utils.h

#ifndef OMAHA_COMMON_UNITTEST_UTILS_H__
#define OMAHA_COMMON_UNITTEST_UTILS_H__

#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

#define kMinASCIIPrintable  32
#define kMaxASCIIPrintable  126

// Overriden class that does work in unittests.
class UnittestUtils {
 public:
  // returns a random signed 32-bit integer
  // not particularly fast, done by using rand() and shifting
  static int32 GetRandomInt32();

  // returns a random signed 64-bit integer
  // not particularly fast, done by using rand() and shifting
  static int64 GetRandomInt64();

  // returns a random printable ASCII character
  // characters range from kMinASCIIPrintable (32 = spacebar) to
  // kMaxASCIIPrintable (126 = ~)
  // see: http://www.asciitable.com
  static char RandomPrintableCharacter();

  // creates a string of random characters using RandomPrintableCharacter()
  // characters are then converted to wide strings
  // returns L"" if length = 0
  static CString CreateRandomString(uint32 length);

  // assumes out_bytes[] has length length
  // fills out_bytes[] with random bytes
  // returns NULL if length = 0
  static bool CreateRandomByteArray(uint32 length, byte out_bytes[]);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UnittestUtils);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_UNITTEST_UTILS_H__
