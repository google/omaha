// Copyright 2005-2009 Google Inc.
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

#include "omaha/common/atl_regexp.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(AtlRETest, AtlRE) {
  AtlRE newline_test_re(_T("ab{\\n}cd"));
  const TCHAR* newline_strings[] = { _T("\n"), _T("\r"), _T("\r\n") };
  for (size_t i = 0; i < arraysize(newline_strings); ++i) {
    CString content(_T("ab"));
    content.Append(newline_strings[i]);
    content.Append(_T("cd"));
    const TCHAR* content_ptr = content.GetString();
    CString newline;
    EXPECT_TRUE(RE::FindAndConsume(&content_ptr, newline_test_re, &newline));
    EXPECT_STREQ(newline, newline_strings[i]);
  }

  // Check that AtlRE works with Unicode characters.
  AtlRE one_two_three_four(_T("\x1234"));
  EXPECT_TRUE(RE::PartialMatch(_T("\x4321\x1234\x4321"), one_two_three_four));
}

}  // namespace omaha
