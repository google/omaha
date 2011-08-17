// Copyright 2010 Google Inc.
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
// SafeCStringFormat unit tests.

#include "omaha/base/safe_format.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(SafeFormatTest, BrokenCStringFormatTruncates) {
  // TODO(omaha): See http://b/1016121 for details.  As of Sept 2010,
  // CString::Format() is implemented using ::wvsprintf(), which has
  // an internal 1024-byte buffer limit.  A bug has been filed with
  // Microsoft.  If this test breaks, it means that we have gotten a
  // new version of ATL/MFC with a fixed CString and can use that
  // instead of SafeCStrFormat/AppendFormat.

  TCHAR largestr[4000] = { 0 };

  for (int i = 0; i < ARRAYSIZE(largestr); ++i) {
    largestr[i] = _T('a') + static_cast<TCHAR>(i % 26);
  }
  largestr[ARRAYSIZE(largestr) - 1] = _T('\0');

  CString test_string;
  test_string.Format(_T("%s"), largestr);
  EXPECT_EQ(1024, test_string.GetLength());
  test_string.AppendFormat(_T("%s"), largestr);
  EXPECT_EQ(2048, test_string.GetLength());
}

TEST(SafeFormatTest, SafeFormatDoesNotTruncate) {
  TCHAR largestr[4000] = { 0 };

  for (int i = 0; i < ARRAYSIZE(largestr); ++i) {
    largestr[i] = _T('a') + static_cast<TCHAR>(i % 26);
  }
  largestr[ARRAYSIZE(largestr) - 1] = _T('\0');

  CString test_string;
  SafeCStringFormat(&test_string, _T("%s"), largestr);
  EXPECT_EQ(ARRAYSIZE(largestr) - 1, test_string.GetLength());

  SafeCStringAppendFormat(&test_string, _T("%s"), largestr);
  EXPECT_EQ(2 * (ARRAYSIZE(largestr) - 1), test_string.GetLength());
}

TEST(SafeFormatTest, FormatBasicFieldTypes) {
  CString test_string;

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("%%"));
  EXPECT_STREQ(_T("%"), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("%c"), _T('h'));
  EXPECT_STREQ(_T("h"), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("%d"), -42);
  EXPECT_STREQ(_T("-42"), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("%6u"), 1337);
  EXPECT_STREQ(_T("  1337"), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("%010X"), 3545084735U);
  EXPECT_STREQ(_T("00D34DB33F"), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("%0.3f"), 123.456);
  EXPECT_STREQ(_T("123.456"), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("\"%s\""), _T("ut_str"));
  EXPECT_STREQ(_T("\"ut_str\""), test_string);

  test_string.Empty();
  SafeCStringFormat(&test_string, _T("\"%s\""), NULL);
  EXPECT_STREQ(_T("\"(null)\""), test_string);
}

TEST(SafeFormatTest, AppendFormatBasicFieldTypes) {
  const CString prefix = _T("ut_prefix");
  CString test_string;

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("%%"));
  EXPECT_STREQ(prefix + _T("%"), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("%c"), _T('h'));
  EXPECT_STREQ(prefix + _T("h"), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("%d"), -42);
  EXPECT_STREQ(prefix + _T("-42"), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("%6u"), 1337);
  EXPECT_STREQ(prefix + _T("  1337"), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("%010X"), 3545084735U);
  EXPECT_STREQ(prefix + _T("00D34DB33F"), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("%0.3f"), 123.456);
  EXPECT_STREQ(prefix + _T("123.456"), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("\"%s\""), _T("ut_str"));
  EXPECT_STREQ(prefix + _T("\"ut_str\""), test_string);

  test_string = prefix;
  SafeCStringAppendFormat(&test_string, _T("\"%s\""), NULL);
  EXPECT_STREQ(prefix + _T("\"(null)\""), test_string);
}

TEST(SafeFormatTest, FormatComplex) {
  CString test_string = _T("prefix: ");
  SafeCStringAppendFormat(&test_string, _T("Test: %cx%08X '%s' %4d %0.2f"),
                          _T('0'), 12648430, _T("utstr"), 42, 123.456);
  EXPECT_STREQ(_T("prefix: Test: 0x00C0FFEE 'utstr'   42 123.46"), test_string);
}

}  // namespace omaha
