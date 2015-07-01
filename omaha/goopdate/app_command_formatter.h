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

#ifndef OMAHA_GOOPDATE_APP_COMMAND_FORMATTER_H_
#define OMAHA_GOOPDATE_APP_COMMAND_FORMATTER_H_

#include <atlstr.h>
#include <winerror.h>
#include <vector>

namespace omaha {

// Formats parameterized application commands. Application commands map IDs to
// a command-line including an executable and its arguments. Application
// commands may include placeholders (%1-%9) that can be filled by numbered
// parameters. % signs before digits may be escaped by doubling them.
//
// If parameters 1->3 are AA, BB, and CC respectively:
//
// echo.exe %1 %%2 %%%3 -> echo.exe AA %%2 %%CC
//
// Placeholders may be embedded within arguments, and appropriate quoting of
// back-slash, double-quotes, space, and tab is applied if necessary after
// interpolation, according to
// http://msdn.microsoft.com/library/windows/desktop/17w5ykft(v=vs.85).aspx .
//
// Placeholders are NOT permitted in the process name.
//
// Each distinct argument in the input (according to the above-linked rules)
// will result in exactly one argument in the output. Input arguments with
// special characters should be escaped in the input. After formatting,
// arguments containing special characters will be escaped in the output.
// Note that the process name will not be escaped in the output.
class AppCommandFormatter {
 public:
  // Instantiates an AppCommandFormatter for the supplied application command.
  // This instance may be used repeatedly with different parameter lists.
  explicit AppCommandFormatter(const CString& command_format);

  // Format this application command with the supplied parameters. The unescaped
  // process name will be placed in |process_name| and the formatted, escaped
  // command-line arguments will be placed in |command_line_arguments|,
  // separated by spaces.
  //
  // Placeholder %N is replaced with |parameters[N - 1]|.
  //
  // Fails if the application command references a parameter %N where N >
  // |parameters.size()|.
  HRESULT Format(const std::vector<CString>& parameters,
                 CString* process_name,
                 CString* command_line_arguments) const;

 private:
  CString process_name_;
  std::vector<CString> command_line_argument_formats_;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_FORMATTER_H_
