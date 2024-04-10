// Copyright 2008-2009 Google Inc.
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

#include "omaha/common/command_line_builder.h"
#include <shellapi.h>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"

namespace omaha {

CommandLineBuilder::CommandLineBuilder(CommandLineMode mode)
    : mode_(mode),
      is_interactive_set_(false),
      is_machine_set_(false),
      is_silent_set_(false),
      is_always_launch_cmd_set_(false),
      is_eula_required_set_(false),
      is_enterprise_set_(false) {
}

CommandLineBuilder::~CommandLineBuilder() {
}

void CommandLineBuilder::set_is_interactive_set(bool is_interactive_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_REPORTCRASH);
  is_interactive_set_ = is_interactive_set;
}

void CommandLineBuilder::set_is_machine_set(bool is_machine_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_REPORTCRASH ||
          mode_ == COMMANDLINE_MODE_UA);
  is_machine_set_ = is_machine_set;
}

void CommandLineBuilder::set_is_silent_set(bool is_silent_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_silent_set_ = is_silent_set;
}

void CommandLineBuilder::set_is_always_launch_cmd_set(
    bool is_always_launch_cmd_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_always_launch_cmd_set_ = is_always_launch_cmd_set;
}

void CommandLineBuilder::set_is_eula_required_set(bool is_eula_required_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_eula_required_set_ = is_eula_required_set;
}

void CommandLineBuilder::set_is_enterprise_set(bool is_enterprise_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_enterprise_set_ = is_enterprise_set;
}

void CommandLineBuilder::set_extra_args(const CString& extra_args) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL ||
          mode_ == COMMANDLINE_MODE_REGISTER_PRODUCT ||
          mode_ == COMMANDLINE_MODE_UNREGISTER_PRODUCT);
  extra_args_ = extra_args;
}

void CommandLineBuilder::set_app_args(const CString& app_args) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  app_args_ = app_args;
}

void CommandLineBuilder::set_install_source(const CString& install_source) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL ||
          mode_ == COMMANDLINE_MODE_UA);
  install_source_ = install_source;
}

void CommandLineBuilder::set_session_id(const CString& session_id) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL ||
          mode_ == COMMANDLINE_MODE_UPDATE);
  session_id_ = session_id;
}

void CommandLineBuilder::set_crash_filename(const CString& crash_filename) {
  ASSERT1(mode_ == COMMANDLINE_MODE_REPORTCRASH);
  crash_filename_ = crash_filename;
}

void CommandLineBuilder::set_custom_info_filename(
    const CString& custom_info_filename) {
  ASSERT1(mode_ == COMMANDLINE_MODE_REPORTCRASH);
  custom_info_filename_ = custom_info_filename;
}

void CommandLineBuilder::set_code_red_metainstaller_path(
    const CString& code_red_metainstaller_path) {
  ASSERT1(mode_ == COMMANDLINE_MODE_RECOVER);
  code_red_metainstaller_path_ = code_red_metainstaller_path;
}

void CommandLineBuilder::set_ping_string(const CString& ping_string) {
  ASSERT1(mode_ == COMMANDLINE_MODE_PING);
  ping_string_ = ping_string;
}

HRESULT CommandLineBuilder::SetOfflineDirName(const CString& offline_dir) {
  ASSERT1(mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);

  CString offline_dir_name(GetFileFromPath(offline_dir));
  GUID offline_dir_guid = {0};
  HRESULT hr = StringToGuidSafe(offline_dir_name, &offline_dir_guid);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[invalid offline_dir_name. Needs to be a guid][%s][%#x]"),
                  offline_dir, hr));
    return E_INVALIDARG;
  }

  offline_dir_name_ = offline_dir_name;
  return S_OK;
}

CString CommandLineBuilder::GetCommandLineArgs() const {
  CString cmd_line_args;

  switch (mode_) {
    case COMMANDLINE_MODE_NOARGS:
      cmd_line_args.Empty();
      break;
    case COMMANDLINE_MODE_CORE:
      cmd_line_args = GetCore();
      break;
    case COMMANDLINE_MODE_SERVICE:
      cmd_line_args = GetService();
      break;
    case COMMANDLINE_MODE_REGSERVER:
      cmd_line_args = GetRegServer();
      break;
    case COMMANDLINE_MODE_UNREGSERVER:
      cmd_line_args = GetUnregServer();
      break;
    case COMMANDLINE_MODE_CRASH:
      cmd_line_args = GetCrash();
      break;
    case COMMANDLINE_MODE_REPORTCRASH:
      cmd_line_args = GetReportCrash();
      break;
    case COMMANDLINE_MODE_INSTALL:
      cmd_line_args = GetInstall();
      break;
    case COMMANDLINE_MODE_UPDATE:
      cmd_line_args = GetUpdate();
      break;
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
      cmd_line_args = GetHandoffInstall();
      break;
    case COMMANDLINE_MODE_UA:
      cmd_line_args = GetUA();
      break;
    case COMMANDLINE_MODE_RECOVER:
      cmd_line_args = GetRecover();
      break;
    case COMMANDLINE_MODE_CODE_RED_CHECK:
      cmd_line_args = GetCodeRedCheck();
      break;
    case COMMANDLINE_MODE_COMSERVER:
      cmd_line_args = GetComServer();
      break;
    case COMMANDLINE_MODE_REGISTER_PRODUCT:
      cmd_line_args = GetRegisterProduct();
      break;
    case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
      cmd_line_args = GetUnregisterProduct();
      break;
    case COMMANDLINE_MODE_SERVICE_REGISTER:
      cmd_line_args = GetServiceRegister();
      break;
    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
      cmd_line_args = GetServiceUnregister();
      break;
    case COMMANDLINE_MODE_CRASH_HANDLER:
      cmd_line_args = GetCrashHandler();
      break;
    case COMMANDLINE_MODE_COMBROKER:
      cmd_line_args = GetComBroker();
      break;
    case COMMANDLINE_MODE_ONDEMAND:
      cmd_line_args = GetOnDemand();
      break;
    case COMMANDLINE_MODE_MEDIUM_SERVICE:
      cmd_line_args = GetMediumService();
      break;
    case COMMANDLINE_MODE_UNINSTALL:
      cmd_line_args = GetUninstall();
      break;
    case COMMANDLINE_MODE_PING:
      cmd_line_args = GetPing();
      break;
    case COMMANDLINE_MODE_HEALTH_CHECK:
      cmd_line_args = GetHealthCheck();
      break;
    case COMMANDLINE_MODE_UNKNOWN:
    default:
      ASSERT1(false);
      break;
  }

#ifdef _DEBUG
  CString full_command_line;
  SafeCStringFormat(&full_command_line, _T("gu.exe %s"),
      cmd_line_args.GetString());
  CommandLineArgs args;
  ASSERT1(SUCCEEDED(ParseCommandLine(full_command_line, &args)));
#endif

  return cmd_line_args;
}

CString CommandLineBuilder::GetCommandLine(const CString& program_name) const {
  // Do not pass the results of the the /update builder to GoogleUpdate.exe.
  // The command line for /update is intended to be passed to a metainstaller.
  // See GetUpdate() for more information.
  ASSERT1(COMMANDLINE_MODE_UPDATE != mode_ ||
          -1 == program_name.Find(kOmahaShellFileName));

  // Always enclose the program name in double quotes.
  CString enclosed_program_name(program_name);
  EnclosePath(&enclosed_program_name);
  CString cmd_line;
  SafeCStringFormat(&cmd_line, _T("%s %s"),
                    enclosed_program_name.GetString(),
                    GetCommandLineArgs().GetString());
  return cmd_line;
}

CString CommandLineBuilder::GetSingleSwitch(const CString& switch_name) const {
  CString cmd_line;
  SafeCStringFormat(&cmd_line, _T("/%s"), switch_name.GetString());
  return cmd_line;
}

CString CommandLineBuilder::GetCore() const {
  return GetSingleSwitch(kCmdLineCore);
}

CString CommandLineBuilder::GetCrashHandler() const {
  return GetSingleSwitch(kCmdLineCrashHandler);
}

CString CommandLineBuilder::GetService() const {
  return GetSingleSwitch(kCmdLineService);
}

CString CommandLineBuilder::GetServiceRegister() const {
  return GetSingleSwitch(kCmdLineRegisterService);
}

CString CommandLineBuilder::GetServiceUnregister() const {
  return GetSingleSwitch(kCmdLineUnregisterService);
}

CString CommandLineBuilder::GetRegServer() const {
  return GetSingleSwitch(kCmdRegServer);
}

CString CommandLineBuilder::GetUnregServer() const {
  return GetSingleSwitch(kCmdUnregServer);
}

CString CommandLineBuilder::GetCrash() const {
  CString cmd_line = GetSingleSwitch(kCmdLineCrash);
  return cmd_line;
}

CString CommandLineBuilder::GetReportCrash() const {
  ASSERT1(!crash_filename_.IsEmpty());
  if (crash_filename_.IsEmpty()) {
    return CString();
  }
  CString cmd_line = GetSingleSwitch(kCmdLineReport);
  if (is_interactive_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineInteractive);
  }
  CString enclosed_crash_filename_(crash_filename_);
  EnclosePath(&enclosed_crash_filename_);
  SafeCStringAppendFormat(&cmd_line, _T(" %s"), enclosed_crash_filename_);
  if (is_machine_set()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineMachine);
  }

  if (!custom_info_filename_.IsEmpty()) {
    CString enclosed_custom_info_filename_(custom_info_filename_);
    EnclosePath(&enclosed_custom_info_filename_);
    SafeCStringAppendFormat(&cmd_line, _T(" /%s %s"),
                            kCmdLineCustomInfoFileName,
                            enclosed_custom_info_filename_.GetString());
  }

  return cmd_line;
}

CString CommandLineBuilder::GetExtraAndAppArgs(
    const TCHAR* extra_switch_name) const {
  ASSERT1(extra_switch_name && *extra_switch_name);
  ASSERT1(!extra_args_.IsEmpty());
  if (extra_args_.IsEmpty()) {
    return CString();
  }

  CString cmd_line;
  CString enclosed_extra_args_(extra_args_);
  EnclosePath(&enclosed_extra_args_);
  SafeCStringFormat(&cmd_line, _T("/%s %s"),
                    extra_switch_name,
                    enclosed_extra_args_.GetString());

  if (!app_args_.IsEmpty()) {
    CString enclosed_app_args = app_args_;
    EnclosePath(&enclosed_app_args);
    SafeCStringAppendFormat(&cmd_line, _T(" /%s %s"),
                            kCmdLineAppArgs,
                            enclosed_app_args.GetString());
  }

  return cmd_line;
}

// Does not support /oem or /eularequired because we would never build that
// internally.
CString CommandLineBuilder::GetInstall() const {
  CString cmd_line(GetExtraAndAppArgs(kCmdLineInstall));
  if (cmd_line.IsEmpty()) {
    return CString();
  }

  if (!install_source_.IsEmpty()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s %s"),
                            kCmdLineInstallSource,
                            install_source_.GetString());
  }
  if (!session_id_.IsEmpty()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s \"%s\""),
                            kCmdLineSessionId,
                            session_id_.GetString());
  }
  if (is_silent_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineSilent);
  }
  if (is_always_launch_cmd_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineAlwaysLaunchCmd);
  }
  if (is_enterprise_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineEnterprise);
  }
  return cmd_line;
}

CString CommandLineBuilder::GetUpdate() const {
  CString cmd_line;
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineUpdate);
  if (!session_id_.IsEmpty()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s \"%s\""),
                            kCmdLineSessionId,
                            session_id_.GetString());
  }
  return cmd_line;
}

CString CommandLineBuilder::GetHandoffInstall() const {
  CString cmd_line(GetExtraAndAppArgs(kCmdLineAppHandoffInstall));
  if (cmd_line.IsEmpty()) {
    return CString();
  }

  if (!install_source_.IsEmpty()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s %s"),
                            kCmdLineInstallSource,
                            install_source_.GetString());
  }
  if (!session_id_.IsEmpty()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s \"%s\""),
                            kCmdLineSessionId,
                            session_id_.GetString());
  }
  if (is_silent_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineSilent);
  }
  if (is_always_launch_cmd_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineAlwaysLaunchCmd);
  }
  if (is_eula_required_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineEulaRequired);
  }
  if (is_enterprise_set_) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineEnterprise);
  }
  if (!offline_dir_name_.IsEmpty()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s \"%s\""),
                            kCmdLineOfflineDir,
                            offline_dir_name_.GetString());
  }
  return cmd_line;
}

CString CommandLineBuilder::GetUA() const {
  ASSERT1(!install_source_.IsEmpty());
  if (install_source_.IsEmpty()) {
    return CString();
  }

  CString cmd_line(GetSingleSwitch(kCmdLineUpdateApps));
  if (is_machine_set()) {
    SafeCStringAppendFormat(&cmd_line, _T(" /%s"), kCmdLineMachine);
  }
  SafeCStringAppendFormat(&cmd_line, _T(" /%s %s"),
                          kCmdLineInstallSource,
                          install_source_.GetString());

  return cmd_line;
}

CString CommandLineBuilder::GetRecover() const {
  ASSERT1(!code_red_metainstaller_path_.IsEmpty());
  if (code_red_metainstaller_path_.IsEmpty()) {
    return CString();
  }
  CString cmd_line;
  SafeCStringFormat(&cmd_line, _T("/%s %s"),
                    kCmdLineRecover,
                    code_red_metainstaller_path_.GetString());
  return cmd_line;
}

CString CommandLineBuilder::GetCodeRedCheck() const {
  return GetSingleSwitch(kCmdLineCodeRedCheck);
}

CString CommandLineBuilder::GetComServer() const {
  return kCmdLineComServerDash;
}

CString CommandLineBuilder::GetComBroker() const {
  return kCmdLineComBroker;
}

CString CommandLineBuilder::GetOnDemand() const {
  return kCmdLineOnDemand;
}

CString CommandLineBuilder::GetMediumService() const {
  return GetSingleSwitch(kCmdLineMediumService);
}

CString CommandLineBuilder::GetUninstall() const {
  return GetSingleSwitch(kCmdLineUninstall);
}

CString CommandLineBuilder::GetRegisterProduct() const {
  ASSERT1(app_args_.IsEmpty());
  return GetExtraAndAppArgs(kCmdLineRegisterProduct);
}

CString CommandLineBuilder::GetUnregisterProduct() const {
  ASSERT1(app_args_.IsEmpty());
  return GetExtraAndAppArgs(kCmdLineUnregisterProduct);
}

CString CommandLineBuilder::GetPing() const {
  CString cmd_line;
  SafeCStringFormat(&cmd_line, _T("/%s %s"), kCmdLinePing,
      ping_string_.GetString());
  return cmd_line;
}

CString CommandLineBuilder::GetHealthCheck() const {
  return GetSingleSwitch(kCmdLineHealthCheck);
}

}  // namespace omaha
