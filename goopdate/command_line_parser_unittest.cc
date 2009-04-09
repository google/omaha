// Copyright 2008-2009 Google Inc.
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

#include "omaha/goopdate/command_line_parser.h"

#include "omaha/testing/unit_test.h"

namespace omaha {

// This will succeed since the CommandLineToArgvW function returns the
// path to the current executable file if it's passed the empty string.
TEST(CommandLineParserTest, ParseNullString) {
  CommandLineParser parser;
  EXPECT_SUCCEEDED(parser.ParseFromString(NULL));
  EXPECT_EQ(0, parser.GetSwitchCount());
}

// This will succeed since the CommandLineToArgvW function returns the
// path to the current executable file if it's passed the empty string.
TEST(CommandLineParserTest, ParseEmptyString) {
  CommandLineParser parser;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("")));
  EXPECT_EQ(0, parser.GetSwitchCount());
}

// This will succeed since the CommandLineToArgvW function returns the
// path to the current executable file if it's passed the empty string.
TEST(CommandLineParserTest, ParseSpacesOnlyString) {
  CommandLineParser parser;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("    ")));
}

TEST(CommandLineParserTest, ParseNullArgv) {
  CommandLineParser parser;
  EXPECT_FAILED(parser.ParseFromArgv(0, NULL));
}

TEST(CommandLineParserTest, CallFunctionsBeforeParse) {
  CommandLineParser parser;
  int arg_count = 0;
  CString arg_value;
  EXPECT_FALSE(parser.HasSwitch(_T("foo")));
  EXPECT_EQ(0, parser.GetSwitchCount());
  EXPECT_FAILED(parser.GetSwitchArgumentCount(_T("foo"), &arg_count));
  EXPECT_FAILED(parser.GetSwitchArgumentValue(_T("foo"), 0, &arg_value));
}

TEST(CommandLineParserTest, ParseProgramNameOnly) {
  CommandLineParser parser;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe")));
  EXPECT_EQ(0, parser.GetSwitchCount());
}

TEST(CommandLineParserTest, ValidateSwitchMixedCase) {
  CommandLineParser parser;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe /FooP")));
  EXPECT_EQ(1, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("foop")));
  EXPECT_TRUE(parser.HasSwitch(_T("FooP")));
  EXPECT_TRUE(parser.HasSwitch(_T("fOOp")));
  EXPECT_TRUE(parser.HasSwitch(_T("FOOP")));
  EXPECT_FALSE(parser.HasSwitch(_T("blAH")));
}

TEST(CommandLineParserTest, ParseOneSwitchNoArgs) {
  CommandLineParser parser;
  int arg_count = 0;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe /foo")));
  EXPECT_EQ(1, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("foo")));
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("foo"), &arg_count));
  EXPECT_EQ(0, arg_count);
}

TEST(CommandLineParserTest, ParseOneSwitchOneArg) {
  CommandLineParser parser;
  int arg_count = 0;
  CString arg_value;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe /foo bar")));
  EXPECT_EQ(1, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("foo")));
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("foo"), &arg_count));
  EXPECT_EQ(1, arg_count);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentValue(_T("foo"), 0, &arg_value));
  EXPECT_STREQ(_T("bar"), arg_value);
}

TEST(CommandLineParserTest, ParseOneSwitchTwoArgs) {
  CommandLineParser parser;
  int arg_count = 0;
  CString arg_value;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe /foo bar baz")));
  EXPECT_EQ(1, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("foo")));
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("foo"), &arg_count));
  EXPECT_EQ(2, arg_count);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentValue(_T("foo"), 0, &arg_value));
  EXPECT_STREQ(_T("bar"), arg_value);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentValue(_T("foo"), 1, &arg_value));
  EXPECT_STREQ(_T("baz"), arg_value);
}

TEST(CommandLineParserTest, ParseTwoSwitchesNoArgs) {
  CommandLineParser parser;
  int arg_count = 0;
  CString arg_value;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe /foo /bar")));
  EXPECT_EQ(2, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("foo")));
  EXPECT_TRUE(parser.HasSwitch(_T("bar")));
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("foo"), &arg_count));
  EXPECT_EQ(0, arg_count);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("bar"), &arg_count));
  EXPECT_EQ(0, arg_count);
}

TEST(CommandLineParserTest, ParseTwoSwitchesOneArgNoArg) {
  CommandLineParser parser;
  int arg_count = 0;
  CString arg_value;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("myprog.exe /foo blech /bar")));
  EXPECT_EQ(2, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("foo")));
  EXPECT_TRUE(parser.HasSwitch(_T("bar")));
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("foo"), &arg_count));
  EXPECT_EQ(1, arg_count);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentValue(_T("foo"), 0, &arg_value));
  EXPECT_STREQ(_T("blech"), arg_value);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("bar"), &arg_count));
  EXPECT_EQ(0, arg_count);
}

TEST(CommandLineParserTest, ParseArgInQuotesWithLeadingSlash) {
  CommandLineParser parser;
  int arg_count = 0;
  CString arg_value;
  EXPECT_SUCCEEDED(parser.ParseFromString(_T("f.exe /pi \"arg\" \"/sw x\"")));
  EXPECT_EQ(1, parser.GetSwitchCount());
  EXPECT_TRUE(parser.HasSwitch(_T("pi")));
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentCount(_T("pi"), &arg_count));
  EXPECT_EQ(2, arg_count);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentValue(_T("pi"), 0, &arg_value));
  EXPECT_STREQ(_T("arg"), arg_value);
  EXPECT_SUCCEEDED(parser.GetSwitchArgumentValue(_T("pi"), 1, &arg_value));
  EXPECT_STREQ(_T("/sw x"), arg_value);
}

}  // namespace omaha

