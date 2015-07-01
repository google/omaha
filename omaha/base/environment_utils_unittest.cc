// Copyright 2007-2013 Google Inc.
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

#include "omaha/base/environment_utils.h"
#include "omaha/base/error.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(EnvironmentUtilTest, BlockLength) {
  EXPECT_EQ(GetEnvironmentBlockLengthInTchar(_T("123456=8\0")), 10);

  EXPECT_EQ(GetEnvironmentBlockLengthInTchar(
                _T("a=3\0TEST=Hello, world!\0")), 24);

  EXPECT_EQ(GetEnvironmentBlockLengthInTchar(_T("")), 1);
}

TEST(EnvironmentUtilTest, ParseName) {
  EXPECT_EQ(ParseNameFromEnvironmentString(_T("name_314=value")),
            _T("name_314"));

  EXPECT_EQ(ParseNameFromEnvironmentString(_T("")), _T(""));

  EXPECT_EQ(ParseNameFromEnvironmentString(_T("=")), _T(""));

  EXPECT_EQ(ParseNameFromEnvironmentString(_T("no_sep")), _T(""));

  EXPECT_EQ(ParseNameFromEnvironmentString(_T("=no_name")), _T(""));

  // The environment should not have this, but the parser needs not know.
  EXPECT_EQ(ParseNameFromEnvironmentString(_T("name_only=")), _T("name_only"));
}

TEST(EnvironmentUtilTest, CompareBlock) {
  EXPECT_TRUE(CompareEnvironmentBlock(_T(""), _T("")));

  EXPECT_TRUE(CompareEnvironmentBlock(_T("name=value\0"), _T("name=value\0")));

  EXPECT_TRUE(CompareEnvironmentBlock(_T("a=3\0b=4\0"), _T("a=3\0b=4\0")));

  EXPECT_FALSE(CompareEnvironmentBlock(_T("a=3\0b=4\0"), _T("a=3\0b=5\0")));

  EXPECT_FALSE(CompareEnvironmentBlock(_T("a=3\0"), _T("a=3\0b=4\0")));

  EXPECT_FALSE(CompareEnvironmentBlock(_T("some=string\0"), _T("a=3\0")));

  EXPECT_FALSE(CompareEnvironmentBlock(_T("case=a\0"), _T("CASE=a\0")));
}

}  // namespace omaha
