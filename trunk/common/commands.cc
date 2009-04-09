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

#include "omaha/common/commands.h"
#include <cstdlib>
#include "base/scoped_ptr.h"
#include "omaha/common/cgi.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

#define kNameValueChar  _T('=')
#define kTrueValue      _T("true")
#define kFalseValue     _T("false")
#define kOnValue        _T("on")
#define kOffValue       _T("off")


//
// Helper functions
//

template<class T>
HRESULT ConvertValue(const TCHAR* str_value, T* value);

// Convert the three-valued value from the string representation
template<>
HRESULT ConvertValue<ThreeValue>(const TCHAR* str_value, ThreeValue* value) {
  ASSERT1(value);

  *value = VALUE_NOT_SET;
  if (str_value && *str_value) {
    if (String_StrNCmp(str_value,
                       kTrueValue,
                       TSTR_SIZE(kTrueValue) + 1,
                       true) == 0 ||
        String_StrNCmp(str_value,
                       kOnValue,
                       TSTR_SIZE(kOnValue) + 1,
                       true) == 0) {
      *value = TRUE_VALUE;
    } else if (String_StrNCmp(str_value,
                              kFalseValue,
                              TSTR_SIZE(kFalseValue) + 1,
                              true) == 0 ||
               String_StrNCmp(str_value,
                              kOffValue,
                              TSTR_SIZE(kOffValue) + 1,
                              true) == 0) {
      *value = FALSE_VALUE;
    } else {
      return CI_E_INVALID_ARG;
    }
  }
  return S_OK;
}

// Convert the int value from the string representation
template<>
HRESULT ConvertValue<int>(const TCHAR* str_value, int* value) {
  ASSERT1(str_value && *str_value);
  ASSERT1(value);

  *value = _tcstol(str_value, NULL, 0);
  if (errno == ERANGE) {
    return CI_E_INVALID_ARG;
  }
  return S_OK;
}

// Convert the unsigned int value from the string representation
template<>
HRESULT ConvertValue<uint32>(const TCHAR* str_value, uint32* value) {
  ASSERT1(str_value && *str_value);
  ASSERT1(value);

  *value = _tcstoul(str_value, NULL, 0);
  if (errno == ERANGE) {
    return CI_E_INVALID_ARG;
  }
  return S_OK;
}

// Convert the string value from the string representation
HRESULT ConvertValue(const TCHAR* str_value, CString* value, bool to_unescape) {
  ASSERT1(str_value && *str_value);
  ASSERT1(value);

  *value = str_value;

  if (to_unescape) {
    int length = value->GetLength();
    scoped_array<TCHAR> unescaped_value(new TCHAR[length + 1]);
    RET_IF_FALSE(CGI::UnescapeString(*value, length, unescaped_value.get(),
      length + 1), CI_E_INVALID_ARG);
    *value = unescaped_value.get();
  }

  return S_OK;
}

//
// Struct CommandOption
//
void CommandOption::Init(const TCHAR* name, CommandOptionType type,
                         void* value, int max_value_len) {
  this->name = name;
  this->type = type;
  this->value = value;
  this->max_value_len = max_value_len;
}

void CommandOption::Copy(const CommandOption& option) {
  Init(option.name, option.type, option.value, option.max_value_len);
}

//
// Class CommandParsingSimple
//

// Constructor
CommandParsingSimple::CommandParsingSimple()
    : separator_(_T(' ')) {
}

// Constructor
CommandParsingSimple::CommandParsingSimple(TCHAR separator)
    : separator_(separator) {
}

// Parse a command line string into args
HRESULT CommandParsingSimple::ParseSimple(const TCHAR* cmd_line) {
  ASSERT1(cmd_line);

  UTIL_LOG(L3, (_T("[CommandParsingSimple::ParseSimple][%s]"), cmd_line));

  args_.clear();

  // Split command line string into list of arguments
  for (const TCHAR* s = cmd_line; *s; ++s) {
    // Handle separator
    if (*s == separator_) {
      continue;
    }

    // Handle single/double quote
    if (*s == _T('"') || *s == _T('\'')) {
      int right_quote = String_FindChar(s + 1, *s);
      if (right_quote == -1) {
        UTIL_LOG(LE, (_T("[CommandParsingSimple::ParseSimple]")
                      _T("[single/double quote mismatches]")));
        return CI_E_INVALID_ARG;
      }
      args_.push_back(CString(s + 1, right_quote));
      s += right_quote + 1;
      continue;
    }

    // Handle all other char
    int next_space = String_FindChar(s + 1, separator_);
    if (next_space == -1) {
      args_.push_back(CString(s));
      break;
    } else {
      args_.push_back(CString(s, next_space + 1));
      s += next_space + 1;
    }
  }

  return S_OK;
}

// Get the arg at specified position from the command line
HRESULT CommandParsingSimple::GetAt(uint32 position, CString* arg) {
  ASSERT1(arg);
  ASSERT1(position < args_.size());

  if (!arg || position >= args_.size()) {
    return E_INVALIDARG;
  }

  *arg = args_[position];
  return S_OK;
}

// Remove the arg at specified position from the command line
HRESULT CommandParsingSimple::RemoveAt(uint32 position) {
  ASSERT1(position < args_.size());

  if (position >= args_.size()) {
    return E_INVALIDARG;
  }

  uint32 i = 0;
  std::vector<CString>::iterator it(args_.begin());
  for (; i < position; ++it, ++i) {
    ASSERT1(it != args_.end());
  }
  args_.erase(it);
  return S_OK;
}

// Converted to the string
HRESULT CommandParsingSimple::ToString(CString* cmd_line) {
  ASSERT1(cmd_line);

  bool is_first = true;
  cmd_line->Empty();
  for (std::vector<CString>::const_iterator it(args_.begin());
       it != args_.end();
       ++it) {
    if (is_first) {
      is_first = false;
    } else {
      cmd_line->AppendChar(separator_);
    }
    const TCHAR* arg = it->GetString();
    if (String_FindChar(arg, separator_) != -1) {
      cmd_line->AppendChar(_T('"'));
      cmd_line->Append(arg);
      cmd_line->AppendChar(_T('"'));
    } else {
      cmd_line->Append(arg);
    }
  }

  return S_OK;
}

// Static Helper function that splits a command line
// string into executable and any arguments
HRESULT CommandParsingSimple::SplitExeAndArgs(const TCHAR* cmd_line,
                                              CString* exe,
                                              CString* args) {
  ASSERT1(cmd_line);
  ASSERT1(exe);
  ASSERT1(args);

  // Do the parsing
  CommandParsingSimple cmd_parsing_simple;

  RET_IF_FAILED(cmd_parsing_simple.ParseSimple(cmd_line));
  RET_IF_FAILED(cmd_parsing_simple.GetAt(0, exe));
  RET_IF_FAILED(cmd_parsing_simple.RemoveAt(0));
  return (cmd_parsing_simple.ToString(args));
}

HRESULT CommandParsingSimple::SplitExeAndArgsGuess(const TCHAR* cmd_line,
                                                   CString* exe,
                                                   CString* args) {
  ASSERT1(cmd_line);
  ASSERT1(exe);
  ASSERT1(args);

  if (File::Exists(cmd_line)) {
    // Optimization for the single executable case.
    // Fill the [out] parameters and return.
    *exe = cmd_line;
    args->Empty();
    return S_OK;
  }

  CString command_line(cmd_line);
  // Check if the command line is properly enclosed, or that it does not have
  // spaces
  if (command_line.GetAt(0) != _T('"') && command_line.Find(_T(' ')) != -1) {
    // If not, need to find the executable, and if valid, enclose it in
    // double quotes
    const TCHAR* index_dot_exe = stristrW(command_line.GetString(), _T(".EXE"));

    if (index_dot_exe != NULL) {
      int dot_exe_end = (index_dot_exe - command_line.GetString())
                         + arraysize(_T(".EXE")) - 1;
      if (File::Exists(CString(command_line, dot_exe_end))) {
        // Enclose the EXE in double quotes
        command_line.Insert(dot_exe_end, _T('"'));
        command_line.Insert(0, _T('"'));
      } else {
        UTIL_LOG(L1, (_T("[CommandParsing::SplitExeAndArgsGuess]")
                      _T("[Could not guess the Executable file within [%s]. ")
                      _T("Passing on to SplitExeAndArgs as-is."),
                      command_line));
      }
    }
  }

  // Do the parsing
  return SplitExeAndArgs(command_line, exe, args);
}


// Static Helper function that returns the number of arguments
// in the passed in cmd_line
HRESULT CommandParsingSimple::GetNumberOfArgs(const TCHAR* cmd_line,
                                              uint32* number_of_args) {
  ASSERT1(cmd_line);
  ASSERT1(number_of_args);

  // Do the parsing
  CommandParsingSimple cmd_parsing_simple;

  RET_IF_FAILED(cmd_parsing_simple.ParseSimple(cmd_line));
  *number_of_args = cmd_parsing_simple.args_.size();
  return S_OK;
}


//
// Class CommandParsing
//

// Constructor
CommandParsing::CommandParsing(CommandOption* options, int options_count)
    : CommandParsingSimple(),
      options_(options),
      options_count_(options_count),
      as_name_value_pair_(false) {
}

// Constructor
CommandParsing::CommandParsing(CommandOption* options, int options_count,
                               TCHAR separator, bool as_name_value_pair)
    : CommandParsingSimple(separator),
      options_(options),
      options_count_(options_count),
      as_name_value_pair_(as_name_value_pair) {
}

// Parse a command line string
HRESULT CommandParsing::Parse(const TCHAR* cmd_line, bool ignore_unknown_args) {
  ASSERT1(cmd_line);

  UTIL_LOG(L3, (_T("[CommandParsing::Parse][%s][%d]"),
                cmd_line, ignore_unknown_args));

  // Parse into args_ vector
  RET_IF_FAILED(ParseSimple(cmd_line));

  // Do the internal parsing
  return InternalParse(ignore_unknown_args);
}

// Parse a list of command line arguments
HRESULT CommandParsing::ParseArguments(int argc, TCHAR* argv[]) {
  if (argc <= 1) {
    return S_OK;
  }

  // Push each argument
  args_.clear();
  for (int i = 1; i < argc; ++i) {
    args_.push_back(CString(argv[i]));
  }

  // Do the internal parsing
  return InternalParse(false);
}

// Internal parsing
HRESULT CommandParsing::InternalParse(bool ignore_unknown_args) {
  CString name, value;
  for (std::vector<CString>::const_iterator it(args_.begin());
       it != args_.end();
       ++it) {
    RET_IF_FAILED(ExtractName(&name, &it));

    int i = FindOption(name);
    if (i == -1) {
      if (ignore_unknown_args) {
        UTIL_LOG(L3, (_T("[CommandParsing::Parse][unknown arg %s]"), name));
        continue;
      } else {
        UTIL_LOG(LE, (_T("[CommandParsing::Parse][invalid arg %s]"), name));
        return CI_E_INVALID_ARG;
      }
    }

    if (options_[i].type != COMMAND_OPTION_BOOL) {
      RET_IF_FAILED(ExtractValue(options_[i], &value, &it, args_.end()));
    }

    switch (options_[i].type & COMMAND_OPTION_FLAGS_MASK) {
      case COMMAND_OPTION_BOOL: {
        bool bool_value = true;
        SetParsedValue(options_[i], bool_value);
        break;
      }

      case COMMAND_OPTION_THREE: {
        ThreeValue three_value = VALUE_NOT_SET;
        RET_IF_FAILED(ConvertValue(value, &three_value));
        SetParsedValue(options_[i], three_value);
        break;
      }

      case COMMAND_OPTION_INT: {
        int int_value = 0;
        RET_IF_FAILED(ConvertValue(value, &int_value));
        SetParsedValue(options_[i], int_value);
        break;
      }

      case COMMAND_OPTION_UINT: {
        int uint_value = 0;
        RET_IF_FAILED(ConvertValue(value, &uint_value));
        SetParsedValue(options_[i], uint_value);
        break;
      }

      case COMMAND_OPTION_STRING: {
        CString str_value;
        bool is_unescape = (options_[i].type & COMMAND_OPTION_UNESCAPE) != 0;
        RET_IF_FAILED(ConvertValue(value, &str_value, is_unescape));
        SetParsedValue(options_[i], str_value);
        break;
      }

      default:
        ASSERT1(false);
        break;
    }
  }

  return S_OK;
}

// Extract the name
HRESULT CommandParsing::ExtractName(CString* name,
                                    std::vector<CString>::const_iterator* it) {
  ASSERT1(name);
  ASSERT1(it);

  if (as_name_value_pair_) {
    int idx = (*it)->Find(kNameValueChar);
    if (idx == -1) {
      return CI_E_INVALID_ARG;
    } else {
      *name = (*it)->Left(idx);
    }
  } else {
    *name = (*it)->GetString();
  }
  return S_OK;
}

// Extract the value
// Also validate the value length if necessary
HRESULT CommandParsing::ExtractValue(
    const CommandOption& option,
    CString* value,
    std::vector<CString>::const_iterator* it,
    const std::vector<CString>::const_iterator& end) {
  ASSERT1(value);
  ASSERT1(it);

  if (as_name_value_pair_) {
    int idx = (*it)->Find(kNameValueChar);
    if (idx == -1) {
      return CI_E_INVALID_ARG;
    } else {
      *value = (*it)->Right((*it)->GetLength() - idx - 1);
    }
  } else {
    ++(*it);
    if (*it == end) {
      UTIL_LOG(LE, (_T("[CommandParsing::ExtractValue]")
                    _T("[argument %s missing value]"), option.name));
      return CI_E_INVALID_ARG;
    }
    *value = (*it)->GetString();
  }

  if (option.max_value_len >= 0) {
    if (value->GetLength() > option.max_value_len) {
      return CI_E_INVALID_ARG;
    }
  }

  return S_OK;
}

// Set the parsed value
template<class T>
void CommandParsing::SetParsedValue(const CommandOption& option,
                                    const T& value) {
  if (option.type & COMMAND_OPTION_MULTIPLE) {
    ASSERT((option.type & COMMAND_OPTION_FLAGS_MASK) != COMMAND_OPTION_BOOL,
      (_T("COMMAND_OPTION_BOOL can't be used with COMMAND_OPTION_MULTIPLE")));
    ASSERT((option.type & COMMAND_OPTION_FLAGS_MASK) != COMMAND_OPTION_THREE,
      (_T("COMMAND_OPTION_THREE can't be used with COMMAND_OPTION_MULTIPLE")));

    std::vector<T>* ptr = reinterpret_cast<std::vector<T>*>(option.value);
    ptr->push_back(value);
  } else {
    T* ptr = reinterpret_cast<T*>(option.value);
    *ptr = value;
  }
}

// Helper function to find an option in the CommandOption list
int CommandParsing::FindOption(const TCHAR* option_name) {
  ASSERT1(option_name);

  for (int i = 0; i < options_count_; ++i) {
    if (String_StrNCmp(option_name,
                       options_[i].name,
                       options_[i].name.GetLength() + 1,
                       false) == 0) {
      return i;
    }
  }

  return -1;
}

// Remove an option from the command line
HRESULT CommandParsing::Remove(const TCHAR* option_name) {
  ASSERT1(option_name);

  for (std::vector<CString>::iterator it(args_.begin());
       it != args_.end();
       ++it) {
    if (*it == option_name) {
      int i = FindOption(option_name);
      if (i == -1) {
        return E_FAIL;
      }
      args_.erase(it);
      if (!as_name_value_pair_) {
        if (options_[i].type != COMMAND_OPTION_BOOL) {
          if (it == args_.end()) {
            return E_FAIL;
          }
          args_.erase(it);
        }
      }

      return S_OK;
    }
  }

  return E_FAIL;
}

}  // namespace omaha

