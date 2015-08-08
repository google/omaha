// Copyright 2012 Google Inc.
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

#include <atlstr.h>
#include <shellapi.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/scoped_any.h"
#include "omaha/goopdate/app_command_formatter.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(AppCommandFormatterTest, NoArguments) {
  CString process_name;
  CString arguments;
  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process")).Format(
      std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T(""), arguments);

  ASSERT_SUCCEEDED(
      AppCommandFormatter(_T("\"c:\\path to\\process.exe\"")).Format(
          std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("c:\\path to\\process.exe"), process_name);
  ASSERT_STREQ(_T(""), arguments);
}

TEST(AppCommandFormatterTest, NoParameters) {
  CString process_name;
  CString arguments;
  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process abc=1")).Format(
      std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("abc=1"), arguments);

  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process abc=1 xyz=2")).Format(
      std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("abc=1 xyz=2"), arguments);

  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process  abc=1  xyz=2   q ")).Format(
      std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("abc=1 xyz=2 q"), arguments);

  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process \"abc = 1\"")).Format(
      std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("\"abc = 1\""), arguments);
  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process abc\" = \"1")).Format(
      std::vector<CString>(), &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("\"abc = 1\""), arguments);
}

TEST(AppCommandFormatterTest, SimpleParameters) {
  std::vector<CString> parameters;
  parameters.push_back(_T("p1"));
  parameters.push_back(_T("p2"));
  parameters.push_back(_T("p3"));

  CString process_name;
  CString arguments;
  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process abc=%1")).Format(
      parameters, &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("abc=p1"), arguments);

  ASSERT_SUCCEEDED(AppCommandFormatter(_T("process abc=%1 %3 %2=x")).Format(
      parameters, &process_name, &arguments));
  ASSERT_STREQ(_T("process"), process_name);
  ASSERT_STREQ(_T("abc=p1 p3 p2=x"), arguments);

  ASSERT_FAILED(AppCommandFormatter(_T("process %4")).Format(
      parameters, &process_name, &arguments));
}

TEST(AppCommandFormatterTest, SimpleParametersNoFormatParameters) {
  CString process_name;
  CString arguments;

  EXPECT_FAILED(AppCommandFormatter(_T("process abc=%1")).Format(
      std::vector<CString>(), &process_name, &arguments));
}

TEST(AppCommandFormatterTest, Interpolation) {
  std::vector<CString> parameters;
  parameters.push_back(_T("p1"));
  parameters.push_back(_T("p2"));
  parameters.push_back(_T("p3"));

  struct {
    TCHAR* input;
    TCHAR* output;
  } test_cases[] = {
    {_T("%1"),
     _T("p1")},
    {_T("%%1"),
     _T("%1")},
    {_T("%%%1"),
     _T("%p1")},
    {_T("abc%def%"),
     _T("abc%def%")},
    {_T("abc%%def%%"),
     _T("abc%%def%%")},
    {_T("%"),
     _T("%")},
    {_T("%12"),
     _T("p12")},
    {_T("%1%2"),
     _T("p1p2")},
  };

  for (int i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    CString process_name;
    CString arguments;
    CString command_line(_T("process "));
    command_line.Append(test_cases[i].input);
    ASSERT_SUCCEEDED(AppCommandFormatter(command_line).Format(
        parameters, &process_name, &arguments)) << "Case " << i << " failed.";
    ASSERT_STREQ(_T("process"), process_name)  << "Case " << i << " failed.";
    ASSERT_STREQ(test_cases[i].output, arguments) << "Case " << i << " failed.";
  }
}

TEST(AppCommandFormatterTest, ParameterQuoting) {
  struct {
    TCHAR* input;
    TCHAR* output;
  } test_cases[] = {
    // embedded \ and \\.
    {_T("a\\b\\\\c"),
     _T("a\\b\\\\c")},
    // trailing \.
    {_T("a\\"),
     _T("a\\")},
    // trailing \\.
    {_T("a\\\\"),
     _T("a\\\\")},
    // only \\.
    {_T("\\\\"),
     _T("\\\\")},
    // empty.
    {_T(""),
     _T("\"\"")},
    // embedded quote.
    {_T("a\"b"),
     _T("a\\\"b")},
    // trailing quote.
    {_T("abc\""),
     _T("abc\\\"")},
    // embedded \\".
    {_T("a\\\\\"b"),
     _T("a\\\\\\\\\\\"b")},
    // trailing \\".
    {_T("abc\\\\\""),
     _T("abc\\\\\\\\\\\"")},
    // Embedded space.
    {_T("abc def"),
     _T("\"abc def\"")},
    // Trailing space.
    {_T("abcdef "),
     _T("\"abcdef \"")},
    // Leading space.
    {_T(" abcdef"),
     _T("\" abcdef\"")},
  };

  for (int i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::vector<CString> parameters;
    parameters.push_back(test_cases[i].input);

    CString process_name;
    CString arguments;
    ASSERT_SUCCEEDED(AppCommandFormatter(_T("process %1")).Format(
        parameters, &process_name, &arguments));
    ASSERT_STREQ(test_cases[i].output, arguments);

    // Now pass the formatted output through the OS and verify that it produces
    // the original input.
    int num_args = 0;
    CString command_line;
    command_line.Append(_T("process"));
    command_line.AppendChar(_T(' '));
    command_line.Append(arguments);
    scoped_hlocal argv_handle(::CommandLineToArgvW(command_line.GetBuffer(),
                                                   &num_args));
    ASSERT_TRUE(valid(argv_handle));
    ASSERT_EQ(2, num_args) << "Input '" << test_cases[i].input
                           << "' gave command line '"
                           << command_line.GetBuffer()
                           << "' which unexpectedly did not parse to a single "
                           << "argument.";

    ASSERT_STREQ(test_cases[i].input,
                 reinterpret_cast<WCHAR**>(get(argv_handle))[1])
        << "Input '" << test_cases[i].input << "' gave command line '"
        << command_line.GetBuffer() << "' which did not parse back to the "
        << "original input";
  }
}

}  // namespace omaha
