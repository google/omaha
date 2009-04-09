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
//
// Parse command-line options
//
// Class CommandParsing supports two kinds of command line options:
// 1) Traditional option, like "-b -v 100"
// 2) Name-value-pairs, like "b=&v=100"

#ifndef OMAHA_COMMON_COMMANDS_H_
#define OMAHA_COMMON_COMMANDS_H_

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

enum ThreeValue {
  VALUE_NOT_SET = 0,
  TRUE_VALUE = 1,
  FALSE_VALUE = 2
};

enum CommandOptionType {
  COMMAND_OPTION_BOOL = 0x1,
  COMMAND_OPTION_INT = 0x2,
  COMMAND_OPTION_UINT = 0x3,
  COMMAND_OPTION_STRING = 0x4,
  COMMAND_OPTION_THREE = 0x5,
  COMMAND_OPTION_UNESCAPE = 0x1000,
  COMMAND_OPTION_MULTIPLE = 0x2000
};

#define COMMAND_OPTION_FLAGS_MASK   0x0FFF

struct CommandOption {
  void Init(const TCHAR* name, CommandOptionType type,
            void* value, int max_value_len);
  void Copy(const CommandOption& option);

  CString name;
  CommandOptionType type;
  void* value;
  int max_value_len;
};

class CommandParsingSimple {
 public:
  // Static Helper function that splits a command line
  // string into executable and any arguments
  static HRESULT SplitExeAndArgs(const TCHAR* cmd_line,
                                 CString* exe,
                                 CString* args);

  // Static Helper function that splits a command line
  // string into executable and any arguments. Tries to
  // guess the positioning of the EXE argument in cases
  // where the EXE argument has spaces and is not enclosed
  // in quotes. For instance, earlier versions of Google Desktop
  // used to have an "Uninstall" string of the form:
  // C:\Program Files\Google\Google Toolbar\GoogleToolbarSetup.exe -uninstall
  // This function is meant to accomodate such cases.
  static HRESULT SplitExeAndArgsGuess(const TCHAR* cmd_line,
                                      CString* exe,
                                      CString* args);

  // Static Helper function that returns the number of arguments
  // in the passed in cmd_line
  static HRESULT GetNumberOfArgs(const TCHAR* cmd_line, uint32* number_of_args);

  // Converted to a string
  HRESULT ToString(CString* cmd_line);

 protected:
  // Constructor
  CommandParsingSimple();

  // Constructor
  explicit CommandParsingSimple(TCHAR separator);

  // Parse a command line string into args
  HRESULT ParseSimple(const TCHAR* cmd_line);

  // Get the arg at specified position from the command line
  HRESULT GetAt(uint32 position, CString* arg);

  // Remove the arg at specified position from the command line
  HRESULT RemoveAt(uint32 position);

  TCHAR separator_;                   // Separator
  std::vector<CString> args_;         // Splitted args


 private:
  DISALLOW_EVIL_CONSTRUCTORS(CommandParsingSimple);
};


class CommandParsing : public CommandParsingSimple {
 public:
  // Constructor
  CommandParsing(CommandOption* options, int options_count);

  CommandParsing(CommandOption* options, int options_count,
                 TCHAR separator, bool as_name_value_pair);

  // Parse a command line string
  HRESULT Parse(const TCHAR* cmd_line, bool ignore_unknown_args);

  // Parse a list of command line arguments
  HRESULT ParseArguments(int argc, TCHAR* argv[]);

  // Remove an option from the command line
  HRESULT Remove(const TCHAR* option_name);

 private:
  // Internal parsing
  HRESULT InternalParse(bool ignore_unknown_args);

  // Extract the name
  HRESULT ExtractName(CString* name, std::vector<CString>::const_iterator* it);

  // Extract the value
  // Also validate the value length if necessary
  HRESULT ExtractValue(const CommandOption& option,
                       CString* value,
                       std::vector<CString>::const_iterator* it,
                       const std::vector<CString>::const_iterator& end);

  // Set the parsed value
  template<class T>
  static void SetParsedValue(const CommandOption& option, const T& value);

  // Helper function to find an option in the CommandOption list
  int FindOption(const TCHAR* option_name);

  CommandOption* options_;            // Command-line option list
  int options_count_;                 // Count of command-line options
  bool as_name_value_pair_;           // Parse as name-value-pair

  DISALLOW_EVIL_CONSTRUCTORS(CommandParsing);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_COMMANDS_H_
