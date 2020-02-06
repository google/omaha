// Copyright 2017 Google Inc.
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
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omaha/internal/chrome_recovery_improved/command_line.h"

#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// To test Windows quoting behavior, we use a string that has some backslashes
// and quotes.
// Consider the command-line argument: q\"bs1\bs2\\bs3q\\\"
static const std::wstring kTrickyQuoted = L"q\\\"bs1\\bs2\\\\bs3q\\\\\\\"";
// It should be parsed by Windows as: q"bs1\bs2\\bs3q\"
// Here that is with C-style escapes.
static const std::wstring kTricky = L"q\"bs1\\bs2\\\\bs3q\\\"";

TEST(CommandLineTest, CommandLineConstructor) {
  const wchar_t* argv[] = {
      L"program",
      L"--foo=",
      L"-bAr",
      L"-spaetzel=pierogi",
      L"-baz",
      L"flim",
      L"--other-switches=--dog=canine --cat=feline",
      L"-spaetzle=Crepe",
      L"-=loosevalue",
      L"-",
      L"FLAN",
      L"a",
      L"--input-translation=45--output-rotation",
      L"--",
      L"--",
      L"--not-a-switch",
      L"\"in the time of submarines...\"",
      L"unquoted arg-with-space"};
  CommandLine cl(arraysize(argv), argv);

  EXPECT_FALSE(cl.GetCommandLineString().empty());
  EXPECT_FALSE(cl.HasSwitch(L"cruller"));
  EXPECT_FALSE(cl.HasSwitch(L"flim"));
  EXPECT_FALSE(cl.HasSwitch(L"program"));
  EXPECT_FALSE(cl.HasSwitch(L"dog"));
  EXPECT_FALSE(cl.HasSwitch(L"cat"));
  EXPECT_FALSE(cl.HasSwitch(L"output-rotation"));
  EXPECT_FALSE(cl.HasSwitch(L"not-a-switch"));
  EXPECT_FALSE(cl.HasSwitch(L"--"));

  EXPECT_EQ(L"program", cl.GetProgram());

  EXPECT_TRUE(cl.HasSwitch(L"foo"));
  EXPECT_TRUE(cl.HasSwitch(L"bar"));
  EXPECT_TRUE(cl.HasSwitch(L"baz"));
  EXPECT_TRUE(cl.HasSwitch(L"spaetzle"));
  EXPECT_TRUE(cl.HasSwitch(L"other-switches"));
  EXPECT_TRUE(cl.HasSwitch(L"input-translation"));

  EXPECT_EQ(L"Crepe", cl.GetSwitchValue(L"spaetzle"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"foo"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"bar"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"cruller"));
  EXPECT_EQ(L"--dog=canine --cat=feline", cl.GetSwitchValue(
      L"other-switches"));
  EXPECT_EQ(L"45--output-rotation", cl.GetSwitchValue(L"input-translation"));

  const CommandLine::StringVector& args = cl.GetArgs();
  ASSERT_EQ(8U, args.size());

  std::vector<std::wstring>::const_iterator iter = args.begin();
  EXPECT_EQ(L"flim", *iter);
  ++iter;
  EXPECT_EQ(L"-", *iter);
  ++iter;
  EXPECT_EQ(L"FLAN", *iter);
  ++iter;
  EXPECT_EQ(L"a", *iter);
  ++iter;
  EXPECT_EQ(L"--", *iter);
  ++iter;
  EXPECT_EQ(L"--not-a-switch", *iter);
  ++iter;
  EXPECT_EQ(L"\"in the time of submarines...\"", *iter);
  ++iter;
  EXPECT_EQ(L"unquoted arg-with-space", *iter);
  ++iter;
  EXPECT_TRUE(iter == args.end());
}

TEST(CommandLineTest, CommandLineFromString) {
  CommandLine cl = CommandLine::FromString(
      L"program --foo= -bAr  /Spaetzel=pierogi /Baz flim "
      L"--other-switches=\"--dog=canine --cat=feline\" "
      L"-spaetzle=Crepe   -=loosevalue  FLAN "
      L"--input-translation=\"45\"--output-rotation "
      L"--quotes=" + kTrickyQuoted + L" "
      L"-- -- --not-a-switch "
      L"\"in the time of submarines...\"");

  EXPECT_FALSE(cl.GetCommandLineString().empty());
  EXPECT_FALSE(cl.HasSwitch(L"cruller"));
  EXPECT_FALSE(cl.HasSwitch(L"flim"));
  EXPECT_FALSE(cl.HasSwitch(L"program"));
  EXPECT_FALSE(cl.HasSwitch(L"dog"));
  EXPECT_FALSE(cl.HasSwitch(L"cat"));
  EXPECT_FALSE(cl.HasSwitch(L"output-rotation"));
  EXPECT_FALSE(cl.HasSwitch(L"not-a-switch"));
  EXPECT_FALSE(cl.HasSwitch(L"--"));

  EXPECT_EQ(L"program", cl.GetProgram());

  EXPECT_TRUE(cl.HasSwitch(L"foo"));
  EXPECT_TRUE(cl.HasSwitch(L"bar"));
  EXPECT_TRUE(cl.HasSwitch(L"baz"));
  EXPECT_TRUE(cl.HasSwitch(L"spaetzle"));
  EXPECT_TRUE(cl.HasSwitch(L"other-switches"));
  EXPECT_TRUE(cl.HasSwitch(L"input-translation"));
  EXPECT_TRUE(cl.HasSwitch(L"quotes"));

  EXPECT_EQ(L"Crepe", cl.GetSwitchValue(L"spaetzle"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"foo"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"bar"));
  EXPECT_EQ(L"", cl.GetSwitchValue(L"cruller"));
  EXPECT_EQ(L"--dog=canine --cat=feline", cl.GetSwitchValue(
      L"other-switches"));
  EXPECT_EQ(L"45--output-rotation", cl.GetSwitchValue(L"input-translation"));
  EXPECT_EQ(kTricky, cl.GetSwitchValue(L"quotes"));

  const CommandLine::StringVector& args = cl.GetArgs();
  ASSERT_EQ(5U, args.size());

  std::vector<std::wstring>::const_iterator iter = args.begin();
  EXPECT_EQ(L"flim", *iter);
  ++iter;
  EXPECT_EQ(L"FLAN", *iter);
  ++iter;
  EXPECT_EQ(L"--", *iter);
  ++iter;
  EXPECT_EQ(L"--not-a-switch", *iter);
  ++iter;
  EXPECT_EQ(L"in the time of submarines...", *iter);
  ++iter;
  EXPECT_TRUE(iter == args.end());

  // Check that a generated string produces an equivalent command line.
  CommandLine cl_duplicate = CommandLine::FromString(cl.GetCommandLineString());
  EXPECT_EQ(cl.GetCommandLineString(), cl_duplicate.GetCommandLineString());
}

// Tests behavior with an empty input string.
TEST(CommandLineTest, EmptyString) {
  CommandLine cl_from_string = CommandLine::FromString(L"");
  EXPECT_TRUE(cl_from_string.GetCommandLineString().empty());
  EXPECT_TRUE(cl_from_string.GetProgram().empty());
  EXPECT_EQ(1U, cl_from_string.argv().size());
  EXPECT_TRUE(cl_from_string.GetArgs().empty());
  CommandLine cl_from_argv(0, NULL);
  EXPECT_TRUE(cl_from_argv.GetCommandLineString().empty());
  EXPECT_TRUE(cl_from_argv.GetProgram().empty());
  EXPECT_EQ(1U, cl_from_argv.argv().size());
  EXPECT_TRUE(cl_from_argv.GetArgs().empty());
}

TEST(CommandLineTest, GetArgumentsString) {
  static const wchar_t kPath1[] =
      L"C:\\Some File\\With Spaces.ggg";
  static const wchar_t kPath2[] =
      L"C:\\no\\spaces.ggg";

  static const wchar_t kFirstArgName[] = L"first-arg";
  static const wchar_t kSecondArgName[] = L"arg2";
  static const wchar_t kThirdArgName[] = L"arg with space";
  static const wchar_t kFourthArgName[] = L"nospace";
  static const wchar_t kFifthArgName[] = L"%1";

  CommandLine cl(CommandLine::NO_PROGRAM);
  cl.AppendSwitch(kFirstArgName, kPath1);
  cl.AppendSwitch(kSecondArgName, kPath2);
  cl.AppendArg(kThirdArgName);
  cl.AppendArg(kFourthArgName);
  cl.AppendArg(kFifthArgName);

  std::wstring expected_first_arg(kFirstArgName);
  std::wstring expected_second_arg(kSecondArgName);
  std::wstring expected_third_arg(kThirdArgName);
  std::wstring expected_fourth_arg(kFourthArgName);
  std::wstring expected_fifth_arg(kFifthArgName);

#define QUOTE_ON_WIN L"\""

  std::wstring expected_str;
  expected_str.append(L"--")
              .append(expected_first_arg)
              .append(L"=")
              .append(QUOTE_ON_WIN)
              .append(kPath1)
              .append(QUOTE_ON_WIN)
              .append(L" ")
              .append(L"--")
              .append(expected_second_arg)
              .append(L"=")
              .append(QUOTE_ON_WIN)
              .append(kPath2)
              .append(QUOTE_ON_WIN)
              .append(L" ")
              .append(QUOTE_ON_WIN)
              .append(expected_third_arg)
              .append(QUOTE_ON_WIN)
              .append(L" ")
              .append(expected_fourth_arg)
              .append(L" ");

  std::wstring expected_str_no_quote_placeholders(expected_str);
  expected_str_no_quote_placeholders.append(expected_fifth_arg);
  EXPECT_EQ(expected_str_no_quote_placeholders, cl.GetArgumentsString());

  std::wstring expected_str_quote_placeholders(expected_str);
  expected_str_quote_placeholders.append(QUOTE_ON_WIN)
                                 .append(expected_fifth_arg)
                                 .append(QUOTE_ON_WIN);
  EXPECT_EQ(expected_str_quote_placeholders,
            cl.GetArgumentsStringWithPlaceholders());
}

// Test methods for appending switches to a command line.
TEST(CommandLineTest, AppendSwitches) {
  std::wstring switch1 = L"switch1";
  std::wstring switch2 = L"switch2";
  std::wstring value2 = L"value";
  std::wstring switch3 = L"switch3";
  std::wstring value3 = L"a value with spaces";
  std::wstring switch4 = L"switch4";
  std::wstring value4 = L"\"a value with quotes\"";
  std::wstring switch5 = L"quotes";
  std::wstring value5 = kTricky;

  CommandLine cl(L"Program");

  cl.AppendSwitch(switch1);
  cl.AppendSwitch(switch2, value2);
  cl.AppendSwitch(switch3, value3);
  cl.AppendSwitch(switch4, value4);
  cl.AppendSwitch(switch5, value4);
  cl.AppendSwitch(switch5, value5);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value2, cl.GetSwitchValue(switch2));
  EXPECT_TRUE(cl.HasSwitch(switch3));
  EXPECT_EQ(value3, cl.GetSwitchValue(switch3));
  EXPECT_TRUE(cl.HasSwitch(switch4));
  EXPECT_EQ(value4, cl.GetSwitchValue(switch4));
  EXPECT_TRUE(cl.HasSwitch(switch5));
  EXPECT_EQ(value5, cl.GetSwitchValue(switch5));

  EXPECT_EQ(L"Program "
            L"--switch1 "
            L"--switch2=value "
            L"--switch3=\"a value with spaces\" "
            L"--switch4=\"\\\"a value with quotes\\\"\" "
            // Even though the switches are unique, appending can add repeat
            // switches to argv.
            L"--quotes=\"\\\"a value with quotes\\\"\" "
            L"--quotes=\"" + kTrickyQuoted + L"\"",
            cl.GetCommandLineString());
}

TEST(CommandLineTest, AppendSwitchesDashDash) {
 const wchar_t* raw_argv[] = { L"prog",
                               L"--",
                               L"--arg1" };
  CommandLine cl(arraysize(raw_argv), raw_argv);

  cl.AppendSwitch(L"switch1");
  cl.AppendSwitch(L"switch2", L"foo");

  cl.AppendArg(L"--arg2");

  EXPECT_EQ(L"prog --switch1 --switch2=foo -- --arg1 --arg2",
            cl.GetCommandLineString());
  CommandLine::StringVector cl_argv = cl.argv();
  EXPECT_EQ(L"prog", cl_argv[0]);
  EXPECT_EQ(L"--switch1", cl_argv[1]);
  EXPECT_EQ(L"--switch2=foo", cl_argv[2]);
  EXPECT_EQ(L"--", cl_argv[3]);
  EXPECT_EQ(L"--arg1", cl_argv[4]);
  EXPECT_EQ(L"--arg2", cl_argv[5]);
}

// Tests that when AppendArguments is called that the program is set correctly
// on the target CommandLine object and the switches from the source
// CommandLine are added to the target.
TEST(CommandLineTest, AppendArguments) {
  CommandLine cl1(L"Program");
  cl1.AppendSwitch(L"switch1");
  cl1.AppendSwitch(L"switch2", L"foo");

  CommandLine cl2(CommandLine::NO_PROGRAM);
  cl2.AppendArguments(cl1, true);
  EXPECT_EQ(cl1.GetProgram(), cl2.GetProgram());
  EXPECT_EQ(cl1.GetCommandLineString(), cl2.GetCommandLineString());

  CommandLine c1(L"Program1");
  c1.AppendSwitch(L"switch1");
  CommandLine c2(L"Program2");
  c2.AppendSwitch(L"switch2");

  c1.AppendArguments(c2, true);
  EXPECT_EQ(c1.GetProgram(), c2.GetProgram());
  EXPECT_TRUE(c1.HasSwitch(L"switch1"));
  EXPECT_TRUE(c1.HasSwitch(L"switch2"));
}

// Make sure that the command line string program paths are quoted as necessary.
// This only makes sense on Windows and the test is basically here to guard
// against regressions.
TEST(CommandLineTest, ProgramQuotes) {
  // Check that quotes are not added for paths without spaces.
  const std::wstring kProgram(L"Program");
  CommandLine cl_program(kProgram);
  EXPECT_EQ(kProgram, cl_program.GetProgram());
  EXPECT_EQ(kProgram, cl_program.GetCommandLineString());

  const std::wstring kProgramPath(L"Program Path");

  // Check that quotes are not returned from GetProgram().
  CommandLine cl_program_path(kProgramPath);
  EXPECT_EQ(kProgramPath, cl_program_path.GetProgram());

  // Check that quotes are added to command line string paths containing spaces.
  std::wstring cmd_string(cl_program_path.GetCommandLineString());
  EXPECT_EQ(L"\"Program Path\"", cmd_string);

  // Check the optional quoting of placeholders in programs.
  CommandLine cl_quote_placeholder(L"%1");
  EXPECT_EQ(L"%1", cl_quote_placeholder.GetCommandLineString());
  EXPECT_EQ(L"\"%1\"",
            cl_quote_placeholder.GetCommandLineStringWithPlaceholders());
}

// Calling Init multiple times should not modify the previous CommandLine.
TEST(CommandLineTest, Init) {
  // Call Init without checking output once so we know it's been called
  // whether or not the test runner does so.
  CommandLine::Init(0, NULL);
  CommandLine* initial = CommandLine::ForCurrentProcess();
  EXPECT_FALSE(CommandLine::Init(0, NULL));
  CommandLine* current = CommandLine::ForCurrentProcess();
  EXPECT_EQ(initial, current);
}

} // namespace omaha
