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

#ifndef OMAHA_COMMON_CGI_H__
#define OMAHA_COMMON_CGI_H__

#include <tchar.h>

namespace omaha {

class CGI {
 public:
  // Maximum factor by which EscapeString() can increase the string length
  static const int kEscapeFactor = 3;

  // EscapeString() converts funky characters found in "src[0,srcn-1]" into
  // escape sequences.  The escaped string is placed back in "dst".  At most
  // "dstn" characters are written into "dst", including the
  // null-termination byte.  Returns true if
  // successful, false if the escaped string will not fit entirely in "dstn"
  // characters.  Since escaping can increase the length, "dst" should not
  // be the same as "src".

  // These functions always return a null-terminated string, even if they
  // fail (e.g. if a bad escape sequence is found).  As a consequence, you
  // must pass in a dstn > 0.
  // If you want to guarantee that the result will fit, you need
  //      dstn >= kEscapeFactor * srcn + 1
  // for EscapeString and
  //      dstn >= srcn + 1
  // for UnescapeString.

  static bool EscapeString(const TCHAR* src, int srcn, TCHAR* dst, int dstn);
  static bool UnescapeString(const TCHAR* src, int srcn, TCHAR* dst, int dstn);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_CGI_H__
