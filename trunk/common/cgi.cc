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

#include "omaha/common/cgi.h"

#include <tchar.h>
#include "base/basictypes.h"
#include "omaha/common/debug.h"

namespace omaha {

static uint32 _needs_escape[8] = {
  0xffffffffL,
  0xf80008fdL,
  0x78000001L,
  0xb8000001L,
  0xffffffffL,
  0xffffffffL,
  0xffffffffL,
  0xffffffffL
};
#define needs_escape(c) (_needs_escape[(c)>>5]&(1<<((c)&31)))

// Here are a couple utility methods to change ints to hex chars & back.
inline int int_to_hex_digit(int i) {
  ASSERT((i >= 0) && (i <= 16), (_T("")));
  return ((i < 10) ? (i + '0') : ((i - 10) + 'A'));
}

inline int hex_digit_to_int(TCHAR c) {
  ASSERT(isxdigit(c), (_T("")));
  return ((c >= 'a') ? ((c - 'a') + 10) :
          (c >= 'A') ? ((c - 'A') + 10) :
          (c - '0'));
}

bool CGI::EscapeString(const TCHAR* src, int srcn, TCHAR* dst, int dstn) {
  ASSERT1(src != dst);  // In-place escaping will fail.
  ASSERT1(srcn >= 0);
  ASSERT1(dstn >= 1);
  dstn--;   // Number of characters we can write, not including null terminator.

  int i, j;
  for (i = 0, j = 0; i < srcn && j < dstn; i++) {
    TCHAR c = src[i];
    if (c == ' ') {
      dst[j++] = '+';
    } else if (!needs_escape(c)) {
      dst[j++] = c;
    } else if (j + 3 > dstn) {
      break;  // Escape sequence will not fit.
    } else {
      dst[j++] = '%';
      dst[j++] = static_cast<TCHAR>(int_to_hex_digit((c >> 4) & 0xf));
      dst[j++] = static_cast<TCHAR>(int_to_hex_digit(c & 0xf));
    }
  }
  dst[j] = '\0';
  return i == srcn;
}

bool CGI::UnescapeString(const TCHAR* src, int srcn, TCHAR* dst, int dstn) {
  ASSERT1(srcn >= 0);
  ASSERT1(dstn >= 1);
  dstn--;   // Number of characters we can write, not including null terminator.

  int i, j;
  for (i = 0, j = 0; i < srcn && j < dstn; ++j) {
    TCHAR c = src[i++];
    if (c == '+') {
      dst[j] = ' ';
    } else if (c != '%') {
      dst[j] = c;
    } else if (i + 2 > srcn) {
      break;  // Escape sequence is incomplete.
    } else if (!isxdigit(src[i]) || !isxdigit(src[i + 1])) {
      break;  // Escape sequence isn't hex.
    } else {
      int num = hex_digit_to_int(src[i++]) << 4;
      num += hex_digit_to_int(src[i++]);
      dst[j] = static_cast<TCHAR>(num);
    }
  }
  dst[j] = '\0';
  return i == srcn;
}

}  // namespace omaha

