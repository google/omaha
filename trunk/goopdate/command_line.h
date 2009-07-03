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
//
// TODO(omaha): consider making all what can be passed on the command line
// "arguments". Our terminology to separate them in commands and options is not
// consistent.

#ifndef OMAHA_GOOPDATE_COMMAND_LINE_H__
#define OMAHA_GOOPDATE_COMMAND_LINE_H__

#include <tchar.h>
#include <atlstr.h>
#include <vector>
#include "omaha/common/constants.h"
#include "omaha/common/browser_utils.h"

namespace omaha {

// Replacement for the C runtime function to process the command line.
// The first token of the command line in Windows is the process name.
// What gets passed to WinMain by the C runtime must not include the first
// token. Since our tiny shell does not use the C runtime we must handle
// the command line by ourselves.
TCHAR* GetCmdLineTail(const TCHAR* cmd_line);

struct CommandLineAppArgs {
  CommandLineAppArgs()
      : app_guid(GUID_NULL),
        needs_admin(false) {}

  GUID app_guid;
  CString app_name;
  bool needs_admin;
  CString ap;
  CString tt_token;
  CString encoded_installer_data;
  CString install_data_index;
};

// Values may be sent in pings or stats. Do not remove or reuse existing values.
typedef enum CommandLineMode {
  COMMANDLINE_MODE_UNKNOWN = 0,
  COMMANDLINE_MODE_NOARGS = 1,  // See GoopdateCommandLineValidator.
  COMMANDLINE_MODE_CORE = 2,
  COMMANDLINE_MODE_SERVICE = 3,
  COMMANDLINE_MODE_REGSERVER = 4,
  COMMANDLINE_MODE_UNREGSERVER = 5,
  COMMANDLINE_MODE_NETDIAGS = 6,
  COMMANDLINE_MODE_CRASH = 7,
  COMMANDLINE_MODE_REPORTCRASH = 8,
  COMMANDLINE_MODE_INSTALL = 9,
  COMMANDLINE_MODE_UPDATE = 10,
  COMMANDLINE_MODE_IG = 11,
  COMMANDLINE_MODE_HANDOFF_INSTALL = 12,
  COMMANDLINE_MODE_UG = 13,
  COMMANDLINE_MODE_UA = 14,
  COMMANDLINE_MODE_RECOVER = 15,
  COMMANDLINE_MODE_WEBPLUGIN = 16,
  COMMANDLINE_MODE_CODE_RED_CHECK = 17,
  COMMANDLINE_MODE_COMSERVER = 18,
  COMMANDLINE_MODE_LEGACYUI = 19,
  COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF = 20,
  COMMANDLINE_MODE_REGISTER_PRODUCT = 21,
  COMMANDLINE_MODE_UNREGISTER_PRODUCT = 22,
  COMMANDLINE_MODE_SERVICE_REGISTER = 23,
  COMMANDLINE_MODE_SERVICE_UNREGISTER = 24,
  COMMANDLINE_MODE_CRASH_HANDLER = 25,
};

struct CommandLineExtraArgs {
  CommandLineExtraArgs()
      : installation_id(GUID_NULL),
        browser_type(BROWSER_UNKNOWN),
        usage_stats_enable(TRISTATE_NONE) {}

  GUID installation_id;
  CString brand_code;
  CString client_id;
  CString referral_id;
  CString language;
  BrowserType browser_type;
  Tristate usage_stats_enable;

  std::vector<CommandLineAppArgs> apps;
};

struct CommandLineArgs {
  CommandLineArgs()
      : mode(COMMANDLINE_MODE_UNKNOWN),
        is_interactive_set(false),
        is_machine_set(false),
        is_crash_handler_disabled(false),
        is_install_elevated(false),
        is_silent_set(false),
        is_eula_required_set(false),
        is_offline_set(false),
        is_oem_set(false),
        is_uninstall_set(false) {}

  CommandLineMode mode;
  bool is_interactive_set;
  bool is_machine_set;
  bool is_crash_handler_disabled;
  bool is_install_elevated;
  bool is_silent_set;
  bool is_eula_required_set;
  bool is_offline_set;
  bool is_oem_set;
  bool is_uninstall_set;
  CString extra_args_str;
  CString app_args_str;
  CString install_source;
  CString crash_filename;
  CString custom_info_filename;
  CString legacy_manifest_path;
  CString webplugin_urldomain;
  CString webplugin_args;
  CString code_red_metainstaller_path;
  CommandLineExtraArgs extra;
};

// Parses the goopdate command line.
HRESULT ParseCommandLine(const TCHAR* cmd_line, CommandLineArgs* args);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COMMAND_LINE_H__
