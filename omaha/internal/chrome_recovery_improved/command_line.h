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

// This class works with command lines: building and parsing.
// Arguments with prefixes ('--', '-', and on Windows, '/') are switches.
// Switches will precede all other arguments without switch prefixes.
// Switches can optionally have values, delimited by '=', e.g., "-switch=value".
// An argument of "--" will terminate switch parsing during initialization,
// interpreting subsequent tokens as non-switch arguments, regardless of prefix.

// There is a singleton read-only CommandLine that represents the command line
// that the current process was started with.  It must be initialized in main().

#ifndef OMAHA_INTERNAL_CHROME_RECOVERY_IMPROVED_COMMAND_LINE_H_
#define OMAHA_INTERNAL_CHROME_RECOVERY_IMPROVED_COMMAND_LINE_H_

#include <stddef.h>
#include <map>
#include <string>
#include <vector>

namespace omaha {

class CommandLine {
 public:
  using StringVector = std::vector<std::wstring>;
  using SwitchMap = std::map<std::wstring, std::wstring>;

  // A constructor for CommandLines that only carry switches and arguments.
  enum NoProgram { NO_PROGRAM };
  explicit CommandLine(NoProgram no_program);

  // Construct a new command line with |program| as argv[0].
  explicit CommandLine(const std::wstring& program);

  // Construct a new command line from an argument list.
  CommandLine(int argc, const wchar_t* const* argv);
  explicit CommandLine(const StringVector& argv);

  ~CommandLine();

  // Initialize the current process CommandLine singleton. On Windows, ignores
  // its arguments (we instead parse GetCommandLineW() directly) because we
  // don't trust the CRT's parsing of the command line, but it still must be
  // called to set up the command line. Returns false if initialization has
  // already occurred, and true otherwise. Only the caller receiving a 'true'
  // return value should take responsibility for calling Reset.
  static bool Init(int argc, const wchar_t* const* argv);

  // Destroys the current process CommandLine singleton. This is necessary if
  // you want to reset the base library to its initial state (for example, in an
  // outer library that needs to be able to terminate, and be re-initialized).
  // If Init is called only once, as in main(), Reset() is not necessary.
  static void Reset();

  // Get the singleton CommandLine representing the current process's
  // command line. Note: returned value is mutable, but not thread safe;
  // only mutate if you know what you're doing!
  static CommandLine* ForCurrentProcess();

  // Returns true if the CommandLine has been initialized for the given process.
  static bool InitializedForCurrentProcess();

  static CommandLine FromString(const std::wstring& command_line);

  // Initialize from an argv vector.
  void InitFromArgv(int argc, const wchar_t* const* argv);
  void InitFromArgv(const StringVector& argv);

  // Constructs and returns the represented command line string.
  std::wstring GetCommandLineString() const {
    return GetCommandLineStringInternal(false);
  }

  // Constructs and returns the represented command line string. Assumes the
  // command line contains placeholders (eg, %1) and quotes any program or
  // argument with a '%' in it. This should be avoided unless the placeholder is
  // required by an external interface (eg, the Windows registry), because it is
  // not generally safe to replace it with an arbitrary string. If possible,
  // placeholders should be replaced *before* converting the command line to a
  // string.
  std::wstring GetCommandLineStringWithPlaceholders() const {
    return GetCommandLineStringInternal(true);
  }

  // Constructs and returns the represented arguments string.
  std::wstring GetArgumentsString() const {
    return GetArgumentsStringInternal(false);
  }

  // Constructs and returns the represented arguments string. Assumes the
  // command line contains placeholders (eg, %1) and quotes any argument with a
  // '%' in it. This should be avoided unless the placeholder is required by an
  // external interface (eg, the Windows registry), because it is not generally
  // safe to replace it with an arbitrary string. If possible, placeholders
  // should be replaced *before* converting the arguments to a string.
  std::wstring GetArgumentsStringWithPlaceholders() const {
    return GetArgumentsStringInternal(true);
  }

  // Returns the original command line string as a vector of strings.
  const StringVector& argv() const { return argv_; }

  // Get and Set the program part of the command line string (the first item).
  std::wstring GetProgram() const;
  void SetProgram(const std::wstring& program);

  // Returns true if this command line contains the given switch.
  // Switch names must be lowercase.
  bool HasSwitch(const std::wstring& switch_string) const;
  bool HasSwitch(const wchar_t switch_constant[]) const;

  // Returns the value associated with the given switch. If the switch has no
  // value or isn't present, this method returns the empty string.
  // Switch names must be lowercase.
  std::wstring GetSwitchValue(const std::wstring& switch_string) const;

  // Get a copy of all switches, along with their values.
  const SwitchMap& GetSwitches() const { return switches_; }

  // Append a switch [with optional value] to the command line.
  // Note: Switches will precede arguments regardless of appending order.
  void AppendSwitch(const std::wstring& switch_string);
  void AppendSwitch(const std::wstring& switch_string,
                    const std::wstring& value);

  // Copy a set of switches (and any values) from another command line.
  // Commonly used when launching a subprocess.
  void CopySwitchesFrom(const CommandLine& source,
                        const wchar_t* const switches[],
                        size_t count);

  // Get the remaining arguments to the command.
  StringVector GetArgs() const;

  // Append an argument to the command line. Note that the argument is quoted
  // properly such that it is interpreted as one argument to the target command.
  // Note: Switches will precede arguments regardless of appending order.
  void AppendArg(const std::wstring& value);

  // Append the switches and arguments from another command line to this one.
  // If |include_program| is true, include |other|'s program as well.
  void AppendArguments(const CommandLine& other, bool include_program);

  // Initialize by parsing the given command line string.
  // The program name is assumed to be the first item in the string.
  void ParseFromString(const std::wstring& command_line);

 private:
  // Disallow default constructor; a program name must be explicitly specified.
  CommandLine();
  // Allow the copy constructor. A common pattern is to copy of the current
  // process's command line and then add some flags to it. For example:
  //   CommandLine cl(*CommandLine::ForCurrentProcess());
  //   cl.AppendSwitch(...);

  // Internal version of GetCommandLineString. If |quote_placeholders| is true,
  // also quotes parts with '%' in them.
  std::wstring GetCommandLineStringInternal(bool quote_placeholders) const;

  // Internal version of GetArgumentsString. If |quote_placeholders| is true,
  // also quotes parts with '%' in them.
  std::wstring GetArgumentsStringInternal(bool quote_placeholders) const;

  // The singleton CommandLine representing the current process's command line.
  static CommandLine* current_process_commandline_;

  // The argv array: { program, [(--|-|/)switch[=value]]*, [--], [argument]* }
  StringVector argv_;

  // Parsed-out switch keys and values.
  SwitchMap switches_;

  // The index after the program and switches, any arguments start here.
  size_t begin_args_;
};

}  // namespace omaha

#endif  // OMAHA_INTERNAL_CHROME_RECOVERY_IMPROVED_COMMAND_LINE_H_
