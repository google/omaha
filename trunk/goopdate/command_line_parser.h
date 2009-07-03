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

#ifndef OMAHA_GOOPDATE_COMMAND_LINE_PARSER_H__
#define OMAHA_GOOPDATE_COMMAND_LINE_PARSER_H__

#include <windows.h>
#include <atlstr.h>

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "goopdate/command_line_parser-internal.h"

namespace omaha {

// This class will parse a command line either from a string or in argc/argv
// format.  It then provides information about the parsed command line.
// When passing the string, make sure it includes the program name as the first
// argument.
// A "switch" is an argument preceded by "/".  Each switch can take 0..n
// arguments.
// Example:  foo.exe /sw a "b b" /sw2 /sw3
// * foo.exe is the program name
// * sw, sw2, and sw3 are switches.
// * a and 'b b' (without the quotes) are the arguments to the sw switch.
// * sw has 2 arguments and sw2 and sw3 have no arguments.
class CommandLineParser {
 public:
  CommandLineParser();
  ~CommandLineParser();

  // Parses the command line from a string.  Must include the program name (e.g.
  // foo.exe) as the first value in the command line.
  HRESULT ParseFromString(const wchar_t* command_line);

  // Parses the command line form argc/argv syntax.  Makes the assumption that
  // argv[0] is the program name (e.g. foo.exe).
  HRESULT ParseFromArgv(int argc, wchar_t** argv);

  // TODO(Omaha): Name these methods "Required".
  // Gets the number of required switches in the parsed command line.
  int GetSwitchCount() const;

  // Returns the required switch at a particular index.
  HRESULT GetSwitchNameAtIndex(int index, CString* switch_name) const;

  // Returns true if a required switch with the name switch_name is found.
  bool HasSwitch(const CString& switch_name) const;

  // Returns the number of required arguments for required switch switch_name.
  HRESULT GetSwitchArgumentCount(const CString& switch_name,
                                 int* count) const;

  // Returns the value of a required switch argument at the specified offset.
  HRESULT GetSwitchArgumentValue(const CString& switch_name,
                                 int argument_index,
                                 CString* argument_value) const;

  // Functions that have the same functionality as the above functions,
  // except they operate on the optional switches.
  int GetOptionalSwitchCount() const;
  bool HasOptionalSwitch(const CString& switch_name) const;
  HRESULT GetOptionalSwitchNameAtIndex(int index, CString* switch_name) const;
  HRESULT GetOptionalSwitchArgumentCount(const CString& switch_name,
                                         int* count) const;
  HRESULT GetOptionalSwitchArgumentValue(
      const CString& switch_name,
      int argument_index,
      CString* argument_value) const;

 private:
  bool IsSwitch(const CString& param) const;
  HRESULT StripSwitchNameFromArgv(const CString& param, CString* switch_name);
  bool IsOptionalSwitch(const CString& param) const;
  HRESULT StripOptionalSwitchNameFromArgv(const CString& param, CString* name);

  void Reset();

  HRESULT AddSwitch(const CString& switch_name);
  HRESULT AddSwitchArgument(const CString& switch_name,
                            const CString& argument_value);
  HRESULT AddOptionalSwitch(const CString& switch_name);
  HRESULT AddOptionalSwitchArgument(const CString& switch_name,
                                    const CString& argument_value);

  internal::CommandLineParserArgs required_args_;
  internal::CommandLineParserArgs optional_args_;

  DISALLOW_EVIL_CONSTRUCTORS(CommandLineParser);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COMMAND_LINE_PARSER_H__

