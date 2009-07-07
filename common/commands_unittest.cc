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
// Unit tests of command line options parsing

#include <cstdio>
#include "omaha/common/commands.h"
#include "omaha/common/file.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

#define kDash         _T("-")
#define kBoolOption   _T("bool")
#define kThreeOption  _T("three")
#define kIntOption    _T("int")
#define kUintOption   _T("uint")
#define kStrOption    _T("str")

#define kIEBrowserExe \
  _T("C:\\PROGRAM FILES\\Internet Explorer\\iexplore.exe")

#define kIEBrowserQuotedExe \
  _T("\"") kIEBrowserExe _T("\"")

#define kIEBrowserQuotedArgs          _T("-h \"%1\"")
#define kIEBrowserQuotedCommandLine \
    kIEBrowserQuotedExe _T(" ") kIEBrowserQuotedArgs

#define kIEBrowserQuotedExeResult \
    _T("C:\\PROGRAM FILES\\Internet Explorer\\iexplore.exe")
#define kIEBrowserQuotedArgsResult    _T("-h %1")

#define kIEBrowserUnquotedCommandLine \
    _T("C:\\Program Files\\Internet Explorer\\iexplore.exe -nohome")
#define kIEBrowserUnquotedExe \
    _T("C:\\Program Files\\Internet Explorer\\iexplore.exe")
#define kIEBrowserUnquotedArgs        _T("-nohome")

#define kGEUninstallCommandLine       _T("RunDll32 C:\\PROGRA~1\\COMMON~1\\INSTAL~1\\PROFES~1\\RunTime\\10\\01\\Intel32\\Ctor.dll,LaunchSetup \"C:\\Program Files\\InstallShield Installation Information\\{3DE5E7D4-7B88-403C-A3FD-2017A8240C5B}\\setup.exe\" -l0x9  -removeonly")  // NOLINT
#define kGEUninstallExe               _T("RunDll32")
#define kGEUninstallArgs              _T("C:\\PROGRA~1\\COMMON~1\\INSTAL~1\\PROFES~1\\RunTime\\10\\01\\Intel32\\Ctor.dll,LaunchSetup \"C:\\Program Files\\InstallShield Installation Information\\{3DE5E7D4-7B88-403C-A3FD-2017A8240C5B}\\setup.exe\" -l0x9 -removeonly")            // NOLINT


struct TestData {
  TestData() {
    Clear();
  }

  void Clear() {
    bool_value = false;
    three_value = VALUE_NOT_SET;
    int_value = 0;
    uint_value = 0;
    str_value.Empty();
  }

  bool bool_value;
  ThreeValue three_value;
  int int_value;
  uint32 uint_value;
  CString str_value;
};

void FillTestData(TestData* data) {
  data->bool_value = true;
  data->three_value = FALSE_VALUE;
  data->int_value = -128;
  data->uint_value = 256;
  data->str_value = _T("Foo");
}

void CheckTestData(const TestData& d1, const TestData& d2) {
  EXPECT_EQ(d1.bool_value, d2.bool_value);
  EXPECT_EQ(d1.three_value, d2.three_value);
  EXPECT_EQ(d1.int_value, d2.int_value);
  EXPECT_EQ(d1.uint_value, d2.uint_value);
  EXPECT_STREQ(d1.str_value, d2.str_value);
}

TEST(CommandsTest, TraditionalCommandLineOptionsParsingTest) {
  TestData data;
  FillTestData(&data);
  CString cmd_line;
  cmd_line.Format(_T("%s%s %s ")
                  _T("%s%s %d ")
                  _T("%s%s %u ")
                  _T("%s%s %s "),
                  kDash, kThreeOption, (data.three_value == TRUE_VALUE) ? _T("on") : _T("off"),   // NOLINT
                  kDash, kIntOption, data.int_value,
                  kDash, kUintOption, data.uint_value,
                  kDash, kStrOption, data.str_value);
  if (data.bool_value) {
    cmd_line.AppendFormat(_T("%s%s"), kDash, kBoolOption);
  }

  TestData option_data;
  CommandOption cmd_options[] = {
    { kDash kBoolOption,  COMMAND_OPTION_BOOL,   &option_data.bool_value,  -1 },
    { kDash kThreeOption, COMMAND_OPTION_THREE,  &option_data.three_value, -1 },
    { kDash kIntOption,   COMMAND_OPTION_INT,    &option_data.int_value,   -1 },
    { kDash kUintOption,  COMMAND_OPTION_UINT,   &option_data.uint_value,  -1 },
    { kDash kStrOption,   COMMAND_OPTION_STRING, &option_data.str_value,   -1 }
  };

  option_data.Clear();
  CommandParsing cmd_parsing(cmd_options, arraysize(cmd_options));
  ASSERT_SUCCEEDED(cmd_parsing.Parse(cmd_line, false));

  CheckTestData(option_data, data);
}

TEST(CommandsTest, TraditionalIgnoreUnknownArgsParsingTest) {
  TestData data;
  FillTestData(&data);
  CString cmd_line;
  cmd_line.Format(_T("%s%s %s ")
                  _T("%s%s %d ")
                  _T("%s%s %u ")
                  _T("%s%s %s "),
                  kDash, kThreeOption, (data.three_value == TRUE_VALUE) ? _T("on") : _T("off"),   // NOLINT
                  kDash, kIntOption, data.int_value,
                  kDash, kUintOption, data.uint_value,
                  kDash, kStrOption, data.str_value);
  if (data.bool_value) {
    cmd_line.AppendFormat(_T("%s%s"), kDash, kBoolOption);
  }

  TestData option_data;
  CommandOption cmd_options[] = {
    { kDash kThreeOption, COMMAND_OPTION_THREE,  &option_data.three_value, -1 },
    { kDash kIntOption,   COMMAND_OPTION_INT,    &option_data.int_value,   -1 },
    { kDash kUintOption,  COMMAND_OPTION_UINT,   &option_data.uint_value,  -1 },
    { kDash kStrOption,   COMMAND_OPTION_STRING, &option_data.str_value,   -1 }
  };

  option_data.Clear();
  CommandParsing cmd_parsing(cmd_options, arraysize(cmd_options));
  ASSERT_FAILED(cmd_parsing.Parse(cmd_line, false));
  ASSERT_SUCCEEDED(cmd_parsing.Parse(cmd_line, true));

  option_data.bool_value = data.bool_value;
  CheckTestData(option_data, data);
}

TEST(CommandsTest, NameValuePairCommandLineOptionsParsingTest) {
  TestData data;
  FillTestData(&data);
  CString cmd_line;
  cmd_line.Format(
      _T("%s=%s&")
      _T("%s=%d&")
      _T("%s=%u&")
      _T("%s=%s"),
      kThreeOption, (data.three_value == TRUE_VALUE) ? _T("on") : _T("off"),
      kIntOption, data.int_value,
      kUintOption, data.uint_value,
      kStrOption, data.str_value);
  if (data.bool_value) {
    cmd_line.AppendFormat(_T("&%s="), kBoolOption);
  }

  TestData option_data;
  CommandOption cmd_options[] = {
    { kBoolOption,  COMMAND_OPTION_BOOL,   &option_data.bool_value,    -1 },
    { kThreeOption, COMMAND_OPTION_THREE,  &option_data.three_value,   -1 },
    { kIntOption,   COMMAND_OPTION_INT,    &option_data.int_value,     -1 },
    { kUintOption,  COMMAND_OPTION_UINT,   &option_data.uint_value,    -1 },
    { kStrOption,   COMMAND_OPTION_STRING, &option_data.str_value,     -1 }
  };

  option_data.Clear();
  CommandParsing cmd_parsing(cmd_options,
                             arraysize(cmd_options),
                             _T('&'),
                             true);
  ASSERT_SUCCEEDED(cmd_parsing.Parse(cmd_line, false));

  CheckTestData(option_data, data);
}

TEST(CommandsTest, NameValuePairIgnoreUnknownArgsParsingTest) {
  TestData data;
  FillTestData(&data);
  CString cmd_line;
  cmd_line.Format(
      _T("%s=%s&")
      _T("%s=%d&")
      _T("%s=%u&")
      _T("%s=%s"),
      kThreeOption, (data.three_value == TRUE_VALUE) ? _T("on") : _T("off"),
      kIntOption, data.int_value,
      kUintOption, data.uint_value,
      kStrOption, data.str_value);
  if (data.bool_value) {
    cmd_line.AppendFormat(_T("&%s="), kBoolOption);
  }

  TestData option_data;
  CommandOption cmd_options[] = {
    { kBoolOption,  COMMAND_OPTION_BOOL,   &option_data.bool_value,    -1 },
    { kThreeOption, COMMAND_OPTION_THREE,  &option_data.three_value,   -1 },
    { kUintOption,  COMMAND_OPTION_UINT,   &option_data.uint_value,    -1 },
    { kStrOption,   COMMAND_OPTION_STRING, &option_data.str_value,     -1 }
  };

  option_data.Clear();
  CommandParsing cmd_parsing(cmd_options,
                             arraysize(cmd_options),
                             _T('&'),
                             true);
  ASSERT_FAILED(cmd_parsing.Parse(cmd_line, false));
  ASSERT_SUCCEEDED(cmd_parsing.Parse(cmd_line, true));

  option_data.int_value = data.int_value;
  CheckTestData(option_data, data);
}

TEST(CommandsTest, CommandParsingSimpleSplitTest) {
  CString exe;
  CString args;

  // Test to make sure SplitExeAndArgs correctly splits
  // a properly constructed command line
  ASSERT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgs(kIEBrowserQuotedCommandLine,
                                          &exe,
                                          &args));

  EXPECT_STREQ(kIEBrowserQuotedExeResult, exe);
  EXPECT_STREQ(kIEBrowserQuotedArgsResult, args);
}

TEST(CommandsTest, CommandParsingGuessSplitTest) {
  CString exe;
  CString args;

  // Test to make sure SplitExeAndArgsGuess correctly splits
  // a properly constructed command line
  ASSERT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserQuotedCommandLine,
                                               &exe,
                                               &args));

  EXPECT_STREQ(kIEBrowserQuotedExeResult, exe);
  EXPECT_STREQ(kIEBrowserQuotedArgsResult, args);

  // Test to make sure SplitExeAndArgsGuess correctly splits
  // an improperly constructed "Uninstall" command line
  ASSERT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserUnquotedCommandLine,
                                               &exe,
                                               &args));

  EXPECT_STREQ(kIEBrowserUnquotedExe, exe);
  EXPECT_STREQ(kIEBrowserUnquotedArgs, args);

  // Test to make sure SplitExeAndArgsGuess correctly splits
  // a properly constructed "Uninstall" command line, where
  // the executable does not have a ".EXE" extension, and
  // where there happens to be an argument which happens to
  // be an executable
  ASSERT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kGEUninstallCommandLine,
                                               &exe,
                                               &args));

  EXPECT_STREQ(kGEUninstallExe, exe);
  EXPECT_STREQ(kGEUninstallArgs, args);
}

TEST(CommandsTest, CommandParsingGuessSplit_ExtraWhiteSpace) {
  CString exe;
  CString args;

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
      kIEBrowserUnquotedCommandLine _T(" "),
      &exe,
      &args));
  EXPECT_STREQ(kIEBrowserUnquotedExe, exe);
  EXPECT_STREQ(kIEBrowserUnquotedArgs, args);

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
      kIEBrowserUnquotedCommandLine _T("\t"),
      &exe,
      &args));
  EXPECT_STREQ(kIEBrowserUnquotedExe, exe);
  EXPECT_STREQ(kIEBrowserUnquotedArgs, args);

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
      _T(" ") kIEBrowserUnquotedCommandLine,
      &exe,
      &args));
  EXPECT_STREQ(kIEBrowserUnquotedExe, exe);
  EXPECT_STREQ(kIEBrowserUnquotedArgs, args);

  // The following cases have unexpected results.
  // Quoting a command line with args is not handled correctly.
  // The entire thing is interpreted as an EXE.

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
      _T("\" ") kIEBrowserUnquotedCommandLine _T("\""),
      &exe,
      &args));
  EXPECT_STREQ(kIEBrowserUnquotedCommandLine, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
      _T("\"") kIEBrowserUnquotedCommandLine _T(" \""),
      &exe,
      &args));
  EXPECT_STREQ(kIEBrowserUnquotedCommandLine, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
      _T("\"") kIEBrowserUnquotedCommandLine _T("\t\""),
      &exe,
      &args));
  EXPECT_STREQ(kIEBrowserUnquotedCommandLine, exe);
  EXPECT_TRUE(args.IsEmpty());
}

TEST(CommandsTest, CommandParsingGuessSplit_CommandLineIsExistingFile) {
  CString exe;
  CString args;

  EXPECT_TRUE(File::Exists(kIEBrowserExe));
  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserExe, &exe, &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  // File::Exists does not handle enclosed paths.
  EXPECT_FALSE(File::Exists(kIEBrowserQuotedExe));
  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserQuotedExe,
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());
}

TEST(CommandsTest,
     CommandParsingGuessSplit_CommandLineIsExistingFileWithExtraWhiteSpace) {
  CString exe;
  CString args;

  EXPECT_TRUE(File::Exists(kIEBrowserExe));
  // File::Exists does not handle enclosed paths.
  EXPECT_FALSE(File::Exists(kIEBrowserQuotedExe));

  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserExe _T(" "),
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserQuotedExe _T(" "),
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserExe _T("\t"),
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(kIEBrowserQuotedExe _T("\t"),
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  // SplitExeAndArgs does not treat tab like whitespace and args aren't trimmed.
  EXPECT_STREQ(_T("\t"), args);

  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(_T(" ") kIEBrowserExe,
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(
    CommandParsingSimple::SplitExeAndArgsGuess(_T(" ") kIEBrowserQuotedExe,
                                               &exe,
                                               &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
                       _T("\" ") kIEBrowserExe _T("\""),
                       &exe,
                       &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
                       _T("\"") kIEBrowserExe _T(" \""),
                       &exe,
                       &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());

  EXPECT_SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(
                       _T("\"") kIEBrowserExe _T("\t\""),
                       &exe,
                       &args));
  EXPECT_STREQ(kIEBrowserExe, exe);
  EXPECT_TRUE(args.IsEmpty());
}

}  // namespace omaha

