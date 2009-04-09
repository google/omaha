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

#include <shellapi.h>

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"

namespace omaha {

namespace internal {

void CommandLineParserArgs::Reset() {
  switch_arguments_.clear();
}

// Assumes switch_name is already lower case.
HRESULT CommandLineParserArgs::AddSwitch(const CString& switch_name) {
  ASSERT1(CString(switch_name).MakeLower().Compare(switch_name) == 0);
  if (switch_arguments_.find(switch_name) != switch_arguments_.end()) {
    return E_INVALIDARG;
  }

  StringVector string_vector;
  switch_arguments_[switch_name] = string_vector;
  return S_OK;
}

// Assumes switch_name is already lower case.
HRESULT CommandLineParserArgs::AddSwitchArgument(const CString& switch_name,
                                                 const CString& value) {
  ASSERT1(CString(switch_name).MakeLower().Compare(switch_name) == 0);
  ASSERT1(!switch_name.IsEmpty());
  if (switch_name.IsEmpty()) {
    // We don't have a switch yet, so this is just a base argument.
    // Example command line:  "foo.exe myarg /someswitch"
    // Here, myarg would be a base argument.
    // TODO(omaha): base_args_.push_back(switch_name_str);
    return E_INVALIDARG;
  }

  SwitchAndArgumentsMap::iterator iter = switch_arguments_.find(switch_name);
  if (iter == switch_arguments_.end()) {
    return E_UNEXPECTED;
  }
  (*iter).second.push_back(value);

  return S_OK;
}

int CommandLineParserArgs::GetSwitchCount() const {
  return switch_arguments_.size();
}

bool CommandLineParserArgs::HasSwitch(const CString& switch_name) const {
  CString switch_name_lower = switch_name;
  switch_name_lower.MakeLower();
  return switch_arguments_.find(switch_name_lower) != switch_arguments_.end();
}

// The value at a particular index may change if switch_names are added
// since we're using a map underneath.  But this keeps us from having to write
// an interator and expose it externally.
HRESULT CommandLineParserArgs::GetSwitchNameAtIndex(int index,
                                                    CString* name) const {
  ASSERT1(name);

  if (index >= static_cast<int>(switch_arguments_.size())) {
    return E_INVALIDARG;
  }

  SwitchAndArgumentsMapIter iter = switch_arguments_.begin();
  for (int i = 0; i < index; ++i) {
    ++iter;
  }

  *name = (*iter).first;

  return S_OK;
}

HRESULT CommandLineParserArgs::GetSwitchArgumentCount(
            const CString& switch_name, int* count) const {
  ASSERT1(count);

  CString switch_name_lower = switch_name;
  switch_name_lower.MakeLower();

  SwitchAndArgumentsMapIter iter = switch_arguments_.find(switch_name_lower);
  if (iter == switch_arguments_.end()) {
    return E_INVALIDARG;
  }

  *count = (*iter).second.size();
  return S_OK;
}

HRESULT CommandLineParserArgs::GetSwitchArgumentValue(
    const CString& switch_name,
    int argument_index,
    CString* argument_value) const {
  ASSERT1(argument_value);

  CString switch_name_lower = switch_name;
  switch_name_lower.MakeLower();

  int count = 0;
  HRESULT hr = GetSwitchArgumentCount(switch_name_lower, &count);
  if (FAILED(hr)) {
    return hr;
  }

  if (argument_index >= count) {
    return E_INVALIDARG;
  }

  SwitchAndArgumentsMapIter iter = switch_arguments_.find(switch_name_lower);
  if (iter == switch_arguments_.end()) {
    return E_INVALIDARG;
  }

  *argument_value = (*iter).second[argument_index];
  return S_OK;
}

}  // namespace internal

CommandLineParser::CommandLineParser() {
}

CommandLineParser::~CommandLineParser() {
}

HRESULT CommandLineParser::ParseFromString(const wchar_t* command_line) {
  CString command_line_str(command_line);
  command_line_str.Trim(_T(" "));

  int argc = 0;
  wchar_t** argv = ::CommandLineToArgvW(command_line_str, &argc);
  if (!argv) {
    return HRESULTFromLastError();
  }

  HRESULT hr = ParseFromArgv(argc, argv);
  ::LocalFree(argv);
  return hr;
}

// TODO(Omaha): Move the rule parser into a separate class.
// TODO(Omaha): Fail the regular command parser if [/ switch is passed.
// ParseFromArgv parses either a rule or a command line.
//
// Rules have required and optional parameters. An example of a rule is:
//     "gu.exe /install <extraargs> [/oem [/appargs <appargs> [/silent"
// This creates a rule for a command line that requires "/install" for the rule
// to match. The other parameters are optional, indicated by prefixes of "[/".
//
// Command lines do not use "[/", and use "/" for all parameters.
// A command line that looks like this:
//     "gu.exe /install <extraargs> /oem /appargs <appargs>"
// will match the rule above.
HRESULT CommandLineParser::ParseFromArgv(int argc, wchar_t** argv) {
  if (argc == 0 || !argv) {
    return E_INVALIDARG;
  }

  CORE_LOG(L5, (_T("[CommandLineParser::ParseFromArgv][argc=%d]"), argc));

  Reset();

  if (argc == 1) {
    // We only have the program name.  So, we're done parsing.
    ASSERT1(!IsSwitch(argv[0]));
    return S_OK;
  }

  CString current_switch_name;
  bool is_optional_switch = false;

  // Start parsing at the first argument after the program name (index 1).
  for (int i = 1; i < argc; ++i) {
    HRESULT hr = S_OK;
    CString token = argv[i];
    token.Trim(_T(" "));
    CORE_LOG(L5, (_T("[Parsing arg][i=%d][argv[i]=%s]"), i, token));
    if (IsSwitch(token)) {
      hr = StripSwitchNameFromArgv(token, &current_switch_name);
      if (FAILED(hr)) {
        return hr;
      }
      hr = AddSwitch(current_switch_name);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[AddSwitch failed][%s][0x%x]"),
                      current_switch_name, hr));
        return hr;
      }
      is_optional_switch = false;
    } else if (IsOptionalSwitch(token)) {
      hr = StripOptionalSwitchNameFromArgv(token, &current_switch_name);
      if (FAILED(hr)) {
        return hr;
      }
      hr = AddOptionalSwitch(current_switch_name);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[AddOptionalSwitch failed][%s][0x%x]"),
                      current_switch_name, hr));
        return hr;
      }
      is_optional_switch = true;
    } else {
      hr = is_optional_switch ?
          AddOptionalSwitchArgument(current_switch_name, token) :
          AddSwitchArgument(current_switch_name, token);

      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[Adding switch argument failed][%d][%s][%s][0x%x]"),
                      is_optional_switch, current_switch_name, token, hr));
        return hr;
      }
    }
  }

  return S_OK;
}

bool CommandLineParser::IsSwitch(const CString& param) const {
  // Switches must have a prefix (/) or (-), and at least one character.
  if (param.GetLength() < 2) {
    return false;
  }

  // All switches must start with / or -, and not contain any spaces.
  // Since the argv parser strips out the enclosing quotes around an argument,
  // we need to handle the following cases properly:
  // * foo.exe /switch arg     -- /switch is a switch, arg is an arg
  // * foo.exe /switch "/x y"  -- /switch is a switch, '/x y' is an arg and it
  //   will get here _without_ the quotes.
  // If param_str starts with / and contains no spaces, then it's a switch.
  return ((param[0] == _T('/')) || (param[0] == _T('-'))) &&
          (param.Find(_T(" ")) == -1) &&
          (param.Find(_T("%20")) == -1);
}

bool CommandLineParser::IsOptionalSwitch(const CString& param) const {
  // Optional switches must have a prefix ([/) or ([-), and at least one
  // character.
  return param[0] == _T('[') && IsSwitch(param.Right(param.GetLength() - 1));
}

HRESULT CommandLineParser::StripSwitchNameFromArgv(const CString& param,
                                                   CString* switch_name) {
  ASSERT1(switch_name);

  if (!IsSwitch(param)) {
    return E_INVALIDARG;
  }

  *switch_name = param.Right(param.GetLength() - 1);
  switch_name->Trim(_T(" "));
  switch_name->MakeLower();
  return S_OK;
}

HRESULT CommandLineParser::StripOptionalSwitchNameFromArgv(const CString& param,
                                                           CString* name) {
  ASSERT1(name);

  if (!IsOptionalSwitch(param)) {
    return E_INVALIDARG;
  }

  return StripSwitchNameFromArgv(param.Right(param.GetLength() - 1), name);
}

void CommandLineParser::Reset() {
  required_args_.Reset();
  optional_args_.Reset();
}

HRESULT CommandLineParser::AddSwitch(const CString& switch_name) {
  ASSERT1(switch_name == CString(switch_name).MakeLower());
  return required_args_.AddSwitch(switch_name);
}

HRESULT CommandLineParser::AddSwitchArgument(const CString& switch_name,
                                             const CString& argument_value) {
  ASSERT1(switch_name == CString(switch_name).MakeLower());
  return required_args_.AddSwitchArgument(switch_name, argument_value);
}

int CommandLineParser::GetSwitchCount() const {
  return required_args_.GetSwitchCount();
}

bool CommandLineParser::HasSwitch(const CString& switch_name) const {
  return required_args_.HasSwitch(switch_name);
}

// The value at a particular index may change if switch_names are added
// since we're using a map underneath.  But this keeps us from having to write
// an interator and expose it externally.
HRESULT CommandLineParser::GetSwitchNameAtIndex(int index,
                                                CString* switch_name) const {
  return required_args_.GetSwitchNameAtIndex(index, switch_name);
}

HRESULT CommandLineParser::GetSwitchArgumentCount(const CString& switch_name,
                                                  int* count) const {
  return required_args_.GetSwitchArgumentCount(switch_name, count);
}

HRESULT CommandLineParser::GetSwitchArgumentValue(
    const CString& switch_name,
    int argument_index,
    CString* argument_value) const {
  return required_args_.GetSwitchArgumentValue(switch_name,
                                               argument_index,
                                               argument_value);
}

HRESULT CommandLineParser::AddOptionalSwitch(const CString& switch_name) {
  ASSERT1(switch_name == CString(switch_name).MakeLower());
  return optional_args_.AddSwitch(switch_name);
}

HRESULT CommandLineParser::AddOptionalSwitchArgument(const CString& switch_name,
                                                     const CString& value) {
  ASSERT1(switch_name == CString(switch_name).MakeLower());
  return optional_args_.AddSwitchArgument(switch_name, value);
}

int CommandLineParser::GetOptionalSwitchCount() const {
  return optional_args_.GetSwitchCount();
}

bool CommandLineParser::HasOptionalSwitch(const CString& switch_name) const {
  return optional_args_.HasSwitch(switch_name);
}

// The value at a particular index may change if switch_names are added
// since we're using a map underneath.  But this keeps us from having to write
// an interator and expose it externally.
HRESULT CommandLineParser::GetOptionalSwitchNameAtIndex(int index,
                                                        CString* name) const {
  return optional_args_.GetSwitchNameAtIndex(index, name);
}

HRESULT CommandLineParser::GetOptionalSwitchArgumentCount(const CString& name,
                                                          int* count) const {
  return optional_args_.GetSwitchArgumentCount(name, count);
}

HRESULT CommandLineParser::GetOptionalSwitchArgumentValue(const CString& name,
                                                          int argument_index,
                                                          CString* val) const {
  return optional_args_.GetSwitchArgumentValue(name,
                                               argument_index,
                                               val);
}

}  // namespace omaha

