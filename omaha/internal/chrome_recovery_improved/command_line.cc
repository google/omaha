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

#include <windows.h>
#include <shellapi.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/string.h"
#include "omaha/base/debug.h"

namespace omaha {

namespace {

#define WHITESPACE_UNICODE \
  0x0009, /* CHARACTER TABULATION */      \
  0x000A, /* LINE FEED (LF) */            \
  0x000B, /* LINE TABULATION */           \
  0x000C, /* FORM FEED (FF) */            \
  0x000D, /* CARRIAGE RETURN (CR) */      \
  0x0020, /* SPACE */                     \
  0x0085, /* NEXT LINE (NEL) */           \
  0x00A0, /* NO-BREAK SPACE */            \
  0x1680, /* OGHAM SPACE MARK */          \
  0x2000, /* EN QUAD */                   \
  0x2001, /* EM QUAD */                   \
  0x2002, /* EN SPACE */                  \
  0x2003, /* EM SPACE */                  \
  0x2004, /* THREE-PER-EM SPACE */        \
  0x2005, /* FOUR-PER-EM SPACE */         \
  0x2006, /* SIX-PER-EM SPACE */          \
  0x2007, /* FIGURE SPACE */              \
  0x2008, /* PUNCTUATION SPACE */         \
  0x2009, /* THIN SPACE */                \
  0x200A, /* HAIR SPACE */                \
  0x2028, /* LINE SEPARATOR */            \
  0x2029, /* PARAGRAPH SEPARATOR */       \
  0x202F, /* NARROW NO-BREAK SPACE */     \
  0x205F, /* MEDIUM MATHEMATICAL SPACE */ \
  0x3000, /* IDEOGRAPHIC SPACE */         \
  0

const wchar_t kWhitespaceWide[] = {
  WHITESPACE_UNICODE
};

const wchar_t kSwitchTerminator[] = L"--";
const wchar_t kSwitchValueSeparator[] = L"=";

// Since we use a lazy match, make sure that longer versions (like "--") are
// listed before shorter versions (like "-") of similar prefixes.

// By putting slash last, we can control whether it is treaded as a switch
// value by changing the value of switch_prefix_count to be one less than
// the array size.
const wchar_t* const kSwitchPrefixes[] = {L"--", L"-", L"/"};

const size_t switch_prefix_count = arraysize(kSwitchPrefixes);

size_t GetSwitchPrefixLength(const std::wstring& s) {
  for (size_t i = 0; i < switch_prefix_count; ++i) {
    const std::wstring prefix(kSwitchPrefixes[i]);
    if (s.compare(0, prefix.length(), prefix) == 0)
      return prefix.length();
  }
  return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const std::wstring& string,
              std::wstring* switch_string,
              std::wstring* switch_value) {
  switch_string->clear();
  switch_value->clear();
  const size_t prefix_length = GetSwitchPrefixLength(string);
  if (prefix_length == 0 || prefix_length == string.length())
    return false;

  const size_t equals_position = string.find(kSwitchValueSeparator);
  *switch_string = string.substr(0, equals_position);
  if (equals_position != std::wstring::npos)
    *switch_value = string.substr(equals_position + 1);
  return true;
}

bool TrimWhitespace(const std::wstring& input, std::wstring* output) {
  if (input.empty()) {
    output->clear();
    return false;
  }
  const std::wstring::size_type left = input.find_first_not_of(kWhitespaceWide);
  const std::wstring::size_type right = input.find_last_not_of(kWhitespaceWide);

  if (left == std::wstring::npos || right == std::wstring::npos) {
    output->clear();
    return true;
  }

  *output = input.substr(left, right - left + 1);
  return true;
}

// Append switches and arguments, keeping switches before arguments.
void AppendSwitchesAndArguments(CommandLine* command_line,
                                const CommandLine::StringVector& argv) {
  bool parse_switches = true;
  for (size_t i = 1; i < argv.size(); ++i) {
    std::wstring arg = argv[i];

    TrimWhitespace(arg, &arg);

    std::wstring switch_string;
    std::wstring switch_value;
    parse_switches &= (arg != kSwitchTerminator);
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      command_line->AppendSwitch(switch_string, switch_value);
    } else {
      command_line->AppendArg(arg);
    }
  }
}

// Quote a string as necessary for CommandLineToArgvW compatiblity *on Windows*.
std::wstring QuoteForCommandLineToArgvW(const std::wstring& arg,
                                    bool quote_placeholders) {
  // We follow the quoting rules of CommandLineToArgvW.
  // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  std::wstring quotable_chars(L" \\\"");
  // We may also be required to quote '%', which is commonly used in a command
  // line as a placeholder. (It may be substituted for a string with spaces.)
  if (quote_placeholders)
    quotable_chars.push_back(L'%');
  if (arg.find_first_of(quotable_chars) == std::wstring::npos) {
    // No quoting necessary.
    return arg;
  }

  std::wstring out;
  out.push_back(L'"');
  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i] == '\\') {
      // Find the extent of this run of backslashes.
      size_t start = i, end = start + 1;
      for (; end < arg.size() && arg[end] == '\\'; ++end) {}
      size_t backslash_count = end - start;

      // Backslashes are escapes only if the run is followed by a double quote.
      // Since we also will end the string with a double quote, we escape for
      // either a double quote or the end of the string.
      if (end == arg.size() || arg[end] == '"') {
        // To quote, we need to output 2x as many backslashes.
        backslash_count *= 2;
      }
      for (size_t j = 0; j < backslash_count; ++j)
        out.push_back('\\');

      // Advance i to one before the end to balance i++ in loop.
      i = end - 1;
    } else if (arg[i] == '"') {
      out.push_back('\\');
      out.push_back('"');
    } else {
      out.push_back(arg[i]);
    }
  }
  out.push_back('"');

  return out;
}

}  // namespace


CommandLine* CommandLine::current_process_commandline_ = NULL;

CommandLine::CommandLine(NoProgram /*no_program*/)
    : argv_(1),
      begin_args_(1) {
}

CommandLine::CommandLine(const std::wstring& program)
    : argv_(1),
      begin_args_(1) {
  SetProgram(program);
}

CommandLine::CommandLine(int argc, const wchar_t* const* argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argv);
}

CommandLine::~CommandLine() {
}

bool CommandLine::Init(int /*argc*/, const wchar_t* const* /*argv*/) {
  if (current_process_commandline_) {
    // If this is intentional, Reset() must be called first. If we are using
    // the shared build mode, we have to share a single object across multiple
    // shared libraries.
    return false;
  }

  current_process_commandline_ = new CommandLine(NO_PROGRAM);
  current_process_commandline_->ParseFromString(::GetCommandLineW());

  return true;
}

void CommandLine::Reset() {
  ASSERT1(current_process_commandline_);
  delete current_process_commandline_;
  current_process_commandline_ = NULL;
}

CommandLine* CommandLine::ForCurrentProcess() {
  ASSERT1(current_process_commandline_);
  return current_process_commandline_;
}


bool CommandLine::InitializedForCurrentProcess() {
  return !!current_process_commandline_;
}

CommandLine CommandLine::FromString(const std::wstring& command_line) {
  CommandLine cmd(NO_PROGRAM);
  cmd.ParseFromString(command_line);
  return cmd;
}

void CommandLine::InitFromArgv(int argc,
                               const wchar_t* const* argv) {
  StringVector new_argv;
  for (int i = 0; i < argc; ++i)
    new_argv.push_back(argv[i]);
  InitFromArgv(new_argv);
}

void CommandLine::InitFromArgv(const StringVector& argv) {
  argv_ = StringVector(1);
  switches_.clear();
  begin_args_ = 1;
  SetProgram(argv.empty() ? std::wstring() : std::wstring(argv[0]));
  AppendSwitchesAndArguments(this, argv);
}

std::wstring CommandLine::GetProgram() const {
  return std::wstring(argv_[0]);
}

void CommandLine::SetProgram(const std::wstring& program) {
  TrimWhitespace(program, &argv_[0]);
}

bool CommandLine::HasSwitch(const std::wstring& switch_string) const {
  return switches_.find(switch_string) != switches_.end();
}

bool CommandLine::HasSwitch(const wchar_t switch_constant[]) const {
  return HasSwitch(std::wstring(switch_constant));
}

std::wstring CommandLine::GetSwitchValue(
    const std::wstring& switch_string) const {
    SwitchMap::const_iterator it = switches_.find(switch_string);
    return it != switches_.end() ? it->second : std::wstring();
}

void CommandLine::AppendSwitch(const std::wstring& switch_string) {
  AppendSwitch(switch_string, std::wstring());
}

void CommandLine::AppendSwitch(const std::wstring& switch_string,
                               const std::wstring& value) {
  std::wstring switch_key = std::wstring(
      CString(switch_string.c_str()).MakeLower());

  std::wstring combined_switch_string(switch_key);
  size_t prefix_length = GetSwitchPrefixLength(combined_switch_string);
  std::pair<SwitchMap::iterator, bool> insertion =
      switches_.insert(make_pair(switch_key.substr(prefix_length), value));
  if (!insertion.second)
    insertion.first->second = value;
  // Preserve existing switch prefixes in |argv_|; only append one if necessary.
  if (prefix_length == 0)
    combined_switch_string = kSwitchPrefixes[0] + combined_switch_string;
  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + value;
  // Append the switch and update the switches/arguments divider |begin_args_|.
  argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

void CommandLine::CopySwitchesFrom(const CommandLine& source,
                                   const wchar_t* const switches[],
                                   size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (source.HasSwitch(switches[i]))
      AppendSwitch(switches[i], source.GetSwitchValue(switches[i]));
  }
}

CommandLine::StringVector CommandLine::GetArgs() const {
  // Gather all arguments after the last switch (may include kSwitchTerminator).
  StringVector args(argv_.begin() + begin_args_, argv_.end());
  // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
  StringVector::iterator switch_terminator =
      std::find(args.begin(), args.end(), kSwitchTerminator);
  if (switch_terminator != args.end())
    args.erase(switch_terminator);
  return args;
}

void CommandLine::AppendArg(const std::wstring& value) {
  argv_.push_back(value);
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  if (include_program)
    SetProgram(other.GetProgram());
  AppendSwitchesAndArguments(this, other.argv());
}

void CommandLine::ParseFromString(const std::wstring& command_line) {
  std::wstring command_line_string;
  TrimWhitespace(command_line, &command_line_string);
  if (command_line_string.empty())
    return;

  int num_args = 0;
  wchar_t** args = NULL;
  args = ::CommandLineToArgvW(command_line_string.c_str(), &num_args);

  InitFromArgv(num_args, args);
  ::LocalFree(args);
}

std::wstring CommandLine::GetCommandLineStringInternal(
    bool quote_placeholders) const {
  std::wstring string(argv_[0]);
  string = QuoteForCommandLineToArgvW(string, quote_placeholders);
  std::wstring params(GetArgumentsStringInternal(quote_placeholders));
  if (!params.empty()) {
    string.append(L" ");
    string.append(params);
  }
  return string;
}

std::wstring CommandLine::GetArgumentsStringInternal(
    bool quote_placeholders) const {
  std::wstring params;
  // Append switches and arguments.
  bool parse_switches = true;
  for (size_t i = 1; i < argv_.size(); ++i) {
    std::wstring arg = argv_[i];
    std::wstring switch_string;
    std::wstring switch_value;
    parse_switches &= arg != kSwitchTerminator;
    if (i > 1)
      params.append(L" ");
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      params.append(switch_string);
      if (!switch_value.empty()) {
        switch_value =
            QuoteForCommandLineToArgvW(switch_value, quote_placeholders);
        params.append(kSwitchValueSeparator + switch_value);
      }
    } else {
      arg = QuoteForCommandLineToArgvW(arg, quote_placeholders);
      params.append(arg);
    }
  }
  return params;
}

}  // namespace omaha
