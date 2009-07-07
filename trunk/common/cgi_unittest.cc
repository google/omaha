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
//
// Unit test for the CGI escape/unescape string..

#include "base/scoped_ptr.h"
#include "omaha/common/cgi.h"
#include "omaha/common/string.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

void TestEscapeUnescape(const TCHAR* origin, const TCHAR* escaped) {
  int origin_len = lstrlen(origin);
  int buffer_len = origin_len * CGI::kEscapeFactor + 1;
  scoped_array<TCHAR> escaped_buffer(new TCHAR[buffer_len]);
  ASSERT_TRUE(CGI::EscapeString(origin, origin_len,
                                escaped_buffer.get(), buffer_len));
  ASSERT_STREQ(escaped_buffer.get(), escaped);

  scoped_array<TCHAR> origin_buffer(new TCHAR[buffer_len]);
  ASSERT_TRUE(CGI::UnescapeString(escaped_buffer.get(),
                                  lstrlen(escaped_buffer.get()),
                                  origin_buffer.get(), buffer_len));
  ASSERT_STREQ(origin_buffer.get(), origin);
}

TEST(CGITEST, EscapeUnescape) {
  // Regular chars.
  TCHAR origin1[] = _T("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  TestEscapeUnescape(origin1, origin1);

  String_ToLower(origin1);
  TestEscapeUnescape(origin1, origin1);

  // Special chars.
  TCHAR origin2[] =  _T("^&`{}|][\"<>\\");    // NOLINT
  TCHAR escaped2[] = _T("%5E%26%60%7B%7D%7C%5D%5B%22%3C%3E%5C");
  TestEscapeUnescape(origin2, escaped2);

  // Real case.
  TCHAR origin3[] = _T("http://foo2.bar.google.com:80/pagead/conversion/1067912086/?ai=123&gclid=456&label=installation&value=0.0");                    // NOLINT
  TCHAR escaped3[] = _T("http://foo2.bar.google.com:80/pagead/conversion/1067912086/%3Fai%3D123%26gclid%3D456%26label%3Dinstallation%26value%3D0.0");   // NOLINT
  TestEscapeUnescape(origin3, escaped3);
}

}  // namespace omaha

