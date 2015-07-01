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

#include "omaha/goopdate/app_command_formatter.h"
#include <shellapi.h>
#include <wtypes.h>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/scoped_any.h"

namespace omaha {

namespace {

// Formats a single unescaped argument. Placeholder %N is replaced with
// parameters[N - 1]. Returns false if a placeholder %N is encountered where N >
// parameters.size().
// A series of % characters followed by a digit may be escaped by doubling the %
// characters. See examples in the class comments for AppCommandFormatter.
bool InterpolateArgument(const std::vector<CString>& parameters,
                         const CString& raw_argument,
                         CString* formatted_argument) {
  size_t percent_count = 0;
  int input_length = raw_argument.GetLength();
  *formatted_argument = _T("");

  for (int i = 0; i < input_length; ++i) {
    TCHAR current = raw_argument[i];
    if (current >= _T('1') && current <= _T('9')) {
      // Before a digit, dump out escaped percent-signs.
      while (percent_count > 1) {
        formatted_argument->AppendChar(_T('%'));
        percent_count -= 2;
      }

      if (percent_count) {
        // There was an odd number of percent-signs. Evaluate the last one as a
        // placeholder.
        ASSERT1(percent_count == 1);

        size_t parameter_index = current - _T('1');
        if (parameter_index >= parameters.size()) {
          *formatted_argument = _T("");
          return false;
        }

        formatted_argument->Append(parameters[parameter_index]);
        // dump out parameters[current - _T('1')]
        percent_count = 0;
      } else {
        // There was an even number of percent-signs. Output the digit itself.
        formatted_argument->AppendChar(current);
      }
    } else if (current != _T('%') || i + 1 == input_length) {
      // At the end of the string, or before a regular character, dump out
      // queued percent-signs.
      while (percent_count > 0) {
        formatted_argument->AppendChar(_T('%'));
        --percent_count;
      }
      // If this is a percent-sign, it's also the last character. Otherwise, it
      // is just a regular (non-digit, non-percent-sign) character.
      formatted_argument->AppendChar(current);
    } else if (current == _T('%')) {
      // This is a percent-sign, possibly followed by a digit, not the last
      // character. Queue it up and we will output it later.
      percent_count++;
    }
  }

  return true;
}

// Escape a string so that it will be interpreted as a single command-line
// argument according to the rules at
// http://msdn.microsoft.com/library/windows/desktop/17w5ykft(v=vs.85).aspx .
CString QuoteForCommandLine(const CString& input) {
  CString output;

  if (input.IsEmpty()) {
    return _T("\"\"");
  }

  bool contains_whitespace = input.FindOneOf(_T(" \t")) != -1;
  if (contains_whitespace) {
    output.AppendChar(_T('"'));
  }

  size_t slash_count = 0;
  int input_length = input.GetLength();

  for (int i = 0; i < input_length; ++i) {
    if (input[i] == _T('"')) {
      // Before a quote, dump out (doubled) slashes
      while (slash_count > 0) {
        output.Append(_T("\\\\"));
        --slash_count;
      }
      output.Append(_T("\\\""));
    } else if (input[i] != _T('\\') || i + 1 == input_length) {
      // At the end of the string, or before a regular character, dump out
      // queued slashes.
      while (slash_count > 0) {
        output.AppendChar(_T('\\'));
        --slash_count;
      }
      // If this is a slash, it's also the last character. Otherwise, it is just
      // a regular (non-quote, non-slash) character.
      output.AppendChar(input[i]);
    } else if (input[i] == _T('\\')) {
      // This is a slash, possibly followed by a quote, not the last character.
      // Queue it up and we will output it later.
      slash_count++;
    }
  }

  if (contains_whitespace) {
    output.AppendChar(_T('"'));
  }

  return output;
}

}  // namespace

AppCommandFormatter::AppCommandFormatter(const CString& command_format) {
  int num_args = 0;
  CString temp(command_format);
  scoped_hlocal argv_handle(::CommandLineToArgvW(temp.GetBuffer(),
                                                 &num_args));
  if (!valid(argv_handle) || num_args < 1) {
    // Empty process name will cause Format to always fail.
    return;
  } else {
    WCHAR** original_argv = reinterpret_cast<WCHAR**>(get(argv_handle));
    process_name_ = original_argv[0];
    for (int i = 1; i < num_args; ++i) {
      command_line_argument_formats_.push_back(original_argv[i]);
    }
  }
}

HRESULT AppCommandFormatter::Format(const std::vector<CString>& parameters,
                                    CString* process_name,
                                    CString* command_line_arguments) const {
  ASSERT1(process_name);
  ASSERT1(command_line_arguments);
  if (!process_name || !command_line_arguments) {
    return E_INVALIDARG;
  }

  if (process_name_.IsEmpty()) {
    return E_FAIL;
  }

  CORE_LOG(L3, (_T("[AppCommandFormatter::Format][%s][%s]"),
                *process_name, *command_line_arguments));

  CString temp_arguments(_T(""));

  for (size_t i = 0; i < command_line_argument_formats_.size(); ++i) {
    int format_length = command_line_argument_formats_[i].GetLength();

    CString formatted_argument;
    if (!InterpolateArgument(parameters,
                             command_line_argument_formats_[i],
                             &formatted_argument)) {
      CORE_LOG(LE, (_T("[InterpolateArgument failed]")));
      return E_INVALIDARG;
    }
    temp_arguments.Append(QuoteForCommandLine(formatted_argument));

    if (i + 1 < command_line_argument_formats_.size()) {
      temp_arguments.AppendChar(_T(' '));
    }
  }

  *process_name = process_name_;

  *command_line_arguments = temp_arguments;

  return S_OK;
}

}  // namespace omaha
