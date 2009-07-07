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

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_GOOPDUMP_H__
#define OMAHA_TOOLS_SRC_GOOPDUMP_GOOPDUMP_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

#include "omaha/tools/goopdump/dump_log.h"
#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"

namespace omaha {

class ConfigManager;

class Goopdump {
 public:
  // Constructor / Destructor
  Goopdump();
  ~Goopdump();

  HRESULT Main(const TCHAR* cmd_line, int argc, TCHAR** argv);

  ConfigManager* config_manager() const;
  CString cmd_line() const;

  GoopdumpCmdLineArgs& cmd_line_args() const;

 private:
  // Installs a user function that is to be called when operator new fails
  // to allocate memory.
  static void SetNewHandler();

  // Called by operator new or operator new[] when they cannot satisfy
  // a request for additional storage.
  static void OutOfMemoryHandler();

  void PrintProgramHeader();
  void PrintUsage();

  CString cmd_line_;            // Command line, as provided by the OS.
  GoopdumpCmdLineArgs args_;
  DumpLog dump_log_;

  DISALLOW_EVIL_CONSTRUCTORS(Goopdump);
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_GOOPDUMP_H__

