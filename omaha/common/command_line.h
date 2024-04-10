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

#ifndef OMAHA_COMMON_COMMAND_LINE_H_
#define OMAHA_COMMON_COMMAND_LINE_H_

#include <tchar.h>
#include <atlstr.h>
#include <vector>
#include "omaha/base/constants.h"
#include "omaha/base/browser_utils.h"
#include "omaha/common/const_goopdate.h"

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
        needs_admin(NEEDS_ADMIN_NO) {}

  GUID app_guid;
  CString app_name;
  NeedsAdmin needs_admin;
  CString ap;
  CString tt_token;
  CString encoded_installer_data;
  CString install_data_index;
  CString experiment_labels;
  CString untrusted_data;
};

// Values may be sent in pings or stats. Do not remove or reuse existing values.
enum CommandLineMode {
  COMMANDLINE_MODE_UNKNOWN = 0,
  COMMANDLINE_MODE_NOARGS = 1,
  COMMANDLINE_MODE_CORE = 2,
  COMMANDLINE_MODE_SERVICE = 3,
  COMMANDLINE_MODE_REGSERVER = 4,
  COMMANDLINE_MODE_UNREGSERVER = 5,
  // Obsolete: COMMANDLINE_MODE_NETDIAGS = 6,
  COMMANDLINE_MODE_CRASH = 7,
  COMMANDLINE_MODE_REPORTCRASH = 8,
  COMMANDLINE_MODE_INSTALL = 9,
  COMMANDLINE_MODE_UPDATE = 10,
  // Obsolete: COMMANDLINE_MODE_IG = 11,
  COMMANDLINE_MODE_HANDOFF_INSTALL = 12,
  // Obsolete: COMMANDLINE_MODE_UG = 13,
  COMMANDLINE_MODE_UA = 14,
  COMMANDLINE_MODE_RECOVER = 15,
  // Obsolete: COMMANDLINE_MODE_WEBPLUGIN = 16,
  COMMANDLINE_MODE_CODE_RED_CHECK = 17,
  COMMANDLINE_MODE_COMSERVER = 18,
  // Obsolete: COMMANDLINE_MODE_LEGACYUI = 19,
  // Obsolete: COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF = 20,
  COMMANDLINE_MODE_REGISTER_PRODUCT = 21,
  COMMANDLINE_MODE_UNREGISTER_PRODUCT = 22,
  COMMANDLINE_MODE_SERVICE_REGISTER = 23,
  COMMANDLINE_MODE_SERVICE_UNREGISTER = 24,
  COMMANDLINE_MODE_CRASH_HANDLER = 25,
  COMMANDLINE_MODE_COMBROKER = 26,
  COMMANDLINE_MODE_ONDEMAND = 27,
  COMMANDLINE_MODE_MEDIUM_SERVICE = 28,
  COMMANDLINE_MODE_UNINSTALL = 29,
  COMMANDLINE_MODE_PING = 30,
  COMMANDLINE_MODE_HEALTH_CHECK = 31,
  // Obsolete: COMMANDLINE_MODE_REGISTER_MSI_HELPER = 32,
};

struct CommandLineExtraArgs {
  CommandLineExtraArgs()
      : installation_id(GUID_NULL),
        browser_type(BROWSER_UNKNOWN),
        usage_stats_enable(TRISTATE_NONE),
        runtime_mode(RUNTIME_MODE_NOT_SET) {}

  CString bundle_name;
  GUID installation_id;
  CString brand_code;
  CString client_id;
  CString experiment_labels;
  CString referral_id;
  CString language;
#if defined(HAS_DEVICE_MANAGEMENT)
  CString enrollment_token;
#endif
  BrowserType browser_type;
  Tristate usage_stats_enable;
  RuntimeMode runtime_mode;

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
        is_always_launch_cmd_set(false),
        is_eula_required_set(false),
        is_offline_set(false),
        is_enterprise_set(false),
        is_oem_set(false) {}

  CommandLineMode mode;
  bool is_interactive_set;
  bool is_machine_set;
  bool is_crash_handler_disabled;
  bool is_install_elevated;
  bool is_silent_set;
  bool is_always_launch_cmd_set;
  bool is_eula_required_set;
  bool is_offline_set;
  bool is_enterprise_set;
  bool is_oem_set;
  CString extra_args_str;
  CString app_args_str;
  CString install_source;
  CString crash_filename;
  CString custom_info_filename;
  CString legacy_manifest_path;
  CString code_red_metainstaller_path;
  CString ping_string;
  CString offline_dir_name;
  CString session_id;
  CommandLineExtraArgs extra;
};

// Parses the goopdate command line.
HRESULT ParseCommandLine(const TCHAR* cmd_line, CommandLineArgs* args);

}  // namespace omaha

#endif  // OMAHA_COMMON_COMMAND_LINE_H_
