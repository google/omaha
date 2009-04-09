// Copyright 2007-2009 Google Inc.
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

#include "omaha/goopdate/command_line.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/goopdate/command_line_parser.h"
#include "omaha/goopdate/goopdate_command_line_validator.h"

namespace omaha {

// Returns a pointer to the second token in the cmd_line parameter or an
// empty string. See the implementation in vc7\crt\src\wincmdln.c
//
// TODO(omaha): consider moving this function into the tiny shell, as it
// is logically part of our modified runtime environment.
TCHAR* GetCmdLineTail(const TCHAR* cmd_line) {
  ASSERT1(cmd_line);
  bool in_quote = false;

  // Skip past program name (first token in command line).
  // Check for and handle quoted program name.

  while ((*cmd_line > _T(' ')) ||
         (*cmd_line && in_quote)) {
    // Flip the in_quote if current character is '"'.
    if (*cmd_line == _T('"')) {
      in_quote = !in_quote;
    }
    ++cmd_line;
  }

  // Skip past any white space preceeding the second token.
  while (*cmd_line && (*cmd_line <= _T(' '))) {
    cmd_line++;
  }

  return const_cast<TCHAR*>(cmd_line);
}

// Assumption: The metainstaller has verified that space ' ' and
// double quote '"' characters do not appear in the "extra" arguments.
// This is important so that attackers can't create tags that provide a valid
// extra argument string followed by other commands.
HRESULT ParseCommandLine(const TCHAR* cmd_line, CommandLineArgs* args) {
  ASSERT1(cmd_line);
  ASSERT1(args);
  CORE_LOG(L3, (_T("[ParseCommandLine][%s]"), cmd_line));

  CommandLineParser parser;
  HRESULT hr = parser.ParseFromString(cmd_line);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ParseCommandLine][ParseFromString failed][0x%x]"), hr));
    return hr;
  }

  GoopdateCommandLineValidator validator;
  hr = validator.Setup();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ParseCommandLine][Validator Setup failed][0x%x]"), hr));
    return hr;
  }

  hr = validator.Validate(&parser, args);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ParseCommandLine][Validate failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

}  // namespace omaha

