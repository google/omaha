// Copyright 2009 Google Inc.
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

#ifndef OMAHA_BASE_COMMAND_LINE_PARSER_INTERNAL_H_
#define OMAHA_BASE_COMMAND_LINE_PARSER_INTERNAL_H_

#include <windows.h>
#include <map>
#include <vector>

namespace omaha {

namespace internal {

// Repository for switches and corresponding switch arguments for a command
// line.
class CommandLineParserArgs {
 public:
  CommandLineParserArgs() {}
  ~CommandLineParserArgs() {}

  typedef std::vector<CString> StringVector;
  typedef StringVector::const_iterator StringVectorIter;
  typedef std::map<CString, StringVector > SwitchAndArgumentsMap;
  typedef SwitchAndArgumentsMap::const_iterator SwitchAndArgumentsMapIter;

  // Gets the number of switches in the parsed command line.  Will return 0 for
  // count if a parse has not occurred.
  int GetSwitchCount() const;

  // Returns the switch at a particular index.
  // This is meant for iteration only and is not guaranteed to be in the order
  // of the switches in the parsed command line.
  HRESULT GetSwitchNameAtIndex(int index, CString* switch_name) const;

  // Returns true if a switch with the name switch_name is found.
  bool HasSwitch(const CString& switch_name) const;

  // Returns the number of arguments for switch_name.  Will fail if switch_name
  // is not a valid switch.
  HRESULT GetSwitchArgumentCount(const CString& switch_name, int* count) const;

  // Returns the value of a switch argument at the specified offset.
  // Fails if switch_name is not a valid switch.
  HRESULT GetSwitchArgumentValue(const CString& switch_name,
                                 int argument_index,
                                 CString* argument_value) const;

  void Reset();
  HRESULT AddSwitch(const CString& switch_name);
  HRESULT AddSwitchArgument(const CString& switch_name,
                            const CString& argument_value);

 private:
  SwitchAndArgumentsMap switch_arguments_;

  DISALLOW_EVIL_CONSTRUCTORS(CommandLineParserArgs);
};

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_BASE_COMMAND_LINE_PARSER_INTERNAL_H_

