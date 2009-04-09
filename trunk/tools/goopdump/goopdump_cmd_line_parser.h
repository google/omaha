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


#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_GOOPDUMP_CMD_LINE_PARSER_H_
#define OMAHA_TOOLS_SRC_GOOPDUMP_GOOPDUMP_CMD_LINE_PARSER_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

struct GoopdumpCmdLineArgs {
  GoopdumpCmdLineArgs()
      : is_machine(false),
        is_user(false),
        is_monitor(false),
        is_dump_app_manager(false),
        is_dump_oneclick(false),
        is_dump_general(false),
        is_write_to_file(false) {}
  bool is_machine;            // Display per-machine data.
  bool is_user;               // Display per-user data (for current user only).
  bool is_monitor;            // Is monitor mode (monitors real time activity).
  bool is_dump_app_manager;   // Dump AppManager data.
  bool is_dump_oneclick;      // Dump OneClick data.
  bool is_dump_general;       // Dump general OS and Omaha data.
  bool is_write_to_file;      // Dump data to file.
  CString log_filename;       // Filename of log to write to.
};

HRESULT ParseGoopdumpCmdLine(int argc, TCHAR** argv, GoopdumpCmdLineArgs* args);

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_GOOPDUMP_CMD_LINE_PARSER_H_

