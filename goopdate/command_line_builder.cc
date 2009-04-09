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

#include "omaha/goopdate/command_line_builder.h"

#include <shellapi.h>

#include "omaha/common/const_cmd_line.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/const_goopdate.h"

namespace omaha {

CommandLineBuilder::CommandLineBuilder(CommandLineMode mode)
    : mode_(mode),
      is_interactive_set_(false),
      is_machine_set_(false),
      is_silent_set_(false),
      is_eula_required_set_(false),
      is_offline_set_(false),
      is_uninstall_set_(false) {
}

CommandLineBuilder::~CommandLineBuilder() {
}

void CommandLineBuilder::set_is_interactive_set(bool is_interactive_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_REPORTCRASH);
  is_interactive_set_ = is_interactive_set;
}

void CommandLineBuilder::set_is_machine_set(bool is_machine_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_UG ||
          mode_ == COMMANDLINE_MODE_REPORTCRASH);
  is_machine_set_ = is_machine_set;
}

void CommandLineBuilder::set_is_silent_set(bool is_silent_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_IG ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_silent_set_ = is_silent_set;
}

void CommandLineBuilder::set_is_eula_required_set(bool is_eula_required_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_IG ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_eula_required_set_ = is_eula_required_set;
}

void CommandLineBuilder::set_is_offline_set(bool is_offline_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_IG ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  is_offline_set_ = is_offline_set;
}

void CommandLineBuilder::set_is_uninstall_set(bool is_uninstall_set) {
  ASSERT1(mode_ == COMMANDLINE_MODE_UA);
  is_uninstall_set_ = is_uninstall_set;
}

void CommandLineBuilder::set_extra_args(const CString& extra_args) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_IG ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL ||
          mode_ == COMMANDLINE_MODE_REGISTER_PRODUCT ||
          mode_ == COMMANDLINE_MODE_UNREGISTER_PRODUCT);
  extra_args_ = extra_args;
}

void CommandLineBuilder::set_app_args(const CString& app_args) {
  ASSERT1(mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_IG ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL);
  app_args_ = app_args;
}

void CommandLineBuilder::set_install_source(const CString& install_source) {
  ASSERT1(mode_ == COMMANDLINE_MODE_WEBPLUGIN ||
          mode_ == COMMANDLINE_MODE_INSTALL ||
          mode_ == COMMANDLINE_MODE_HANDOFF_INSTALL ||
          mode_ == COMMANDLINE_MODE_IG);
  install_source_ = install_source;
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

void CommandLineBuilder::set_legacy_manifest_path(
    const CString& legacy_manifest_path) {
  ASSERT1(mode_ == COMMANDLINE_MODE_LEGACYUI ||
          mode_ == COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF);
  legacy_manifest_path_ = legacy_manifest_path;
}

void CommandLineBuilder::set_webplugin_url_domain(
    const CString& webplugin_url_domain) {
  ASSERT1(mode_ == COMMANDLINE_MODE_WEBPLUGIN);
  webplugin_url_domain_ = webplugin_url_domain;
}

void CommandLineBuilder::set_webplugin_args(const CString& webplugin_args) {
  ASSERT1(mode_ == COMMANDLINE_MODE_WEBPLUGIN);
  webplugin_args_ = webplugin_args;
}

void CommandLineBuilder::set_code_red_metainstaller_path(
    const CString& code_red_metainstaller_path) {
  ASSERT1(mode_ == COMMANDLINE_MODE_RECOVER);
  code_red_metainstaller_path_ = code_red_metainstaller_path;
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
    case COMMANDLINE_MODE_CRASH_HANDLER:
      cmd_line_args = GetCrashHandler();
      break;
    case COMMANDLINE_MODE_SERVICE:
      cmd_line_args = GetService();
      break;
    case COMMANDLINE_MODE_SERVICE_REGISTER:
      cmd_line_args = GetServiceRegister();
      break;
    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
      cmd_line_args = GetServiceUnregister();
      break;
    case COMMANDLINE_MODE_REGSERVER:
      cmd_line_args = GetRegServer();
      break;
    case COMMANDLINE_MODE_UNREGSERVER:
      cmd_line_args = GetUnregServer();
      break;
    case COMMANDLINE_MODE_NETDIAGS:
      cmd_line_args = GetNetDiags();
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
    case COMMANDLINE_MODE_IG:
      cmd_line_args = GetIG();
      break;
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
      cmd_line_args = GetHandoffInstall();
      break;
    case COMMANDLINE_MODE_UG:
      cmd_line_args = GetUG();
      break;
    case COMMANDLINE_MODE_UA:
      cmd_line_args = GetUA();
      break;
    case COMMANDLINE_MODE_RECOVER:
      cmd_line_args = GetRecover();
      break;
    case COMMANDLINE_MODE_WEBPLUGIN:
      cmd_line_args = GetWebPlugin();
      break;
    case COMMANDLINE_MODE_CODE_RED_CHECK:
      cmd_line_args = GetCodeRedCheck();
      break;
    case COMMANDLINE_MODE_COMSERVER:
      cmd_line_args = GetComServer();
      break;
    case COMMANDLINE_MODE_LEGACYUI:
      // No one in Omaha 2 should be using this mode.  It's only for
      // compatibility with previous versions.
      ASSERT1(false);
      break;
    case COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF:
      cmd_line_args = GetLegacyHandoff();
      break;
    case COMMANDLINE_MODE_REGISTER_PRODUCT:
      cmd_line_args = GetRegisterProduct();
      break;
    case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
      cmd_line_args = GetUnregisterProduct();
      break;
    case COMMANDLINE_MODE_UNKNOWN:
    default:
      ASSERT1(false);
      break;
  }

#ifdef _DEBUG
  CString full_command_line;
  full_command_line.Format(_T("gu.exe %s"), cmd_line_args);
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
          -1 == program_name.Find(kGoopdateFileName));

  // Always enclose the program name in double quotes.
  CString enclosed_program_name(program_name);
  EnclosePath(&enclosed_program_name);
  CString cmd_line;
  cmd_line.Format(_T("%s %s"), enclosed_program_name, GetCommandLineArgs());
  return cmd_line;
}

CString CommandLineBuilder::GetSingleSwitch(const CString& switch_name) const {
  CString cmd_line;
  cmd_line.Format(_T("/%s"), switch_name);
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

CString CommandLineBuilder::GetNetDiags() const {
  return GetSingleSwitch(kCmdLineNetDiags);
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
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineInteractive);
  }
  CString enclosed_crash_filename_(crash_filename_);
  EnclosePath(&enclosed_crash_filename_);
  cmd_line.AppendFormat(_T(" %s"), enclosed_crash_filename_);
  if (is_machine_set()) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineMachine);
  }

  if (!custom_info_filename_.IsEmpty()) {
    CString enclosed_custom_info_filename_(custom_info_filename_);
    EnclosePath(&enclosed_custom_info_filename_);
    cmd_line.AppendFormat(_T(" /%s %s"),
                          kCmdLineCustomInfoFileName,
                          enclosed_custom_info_filename_);
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
  cmd_line.Format(_T("/%s %s"), extra_switch_name, enclosed_extra_args_);

  if (!app_args_.IsEmpty()) {
    CString enclosed_app_args = app_args_;
    EnclosePath(&enclosed_app_args);
    cmd_line.AppendFormat(_T(" /%s %s"), kCmdLineAppArgs, enclosed_app_args);
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
    cmd_line.AppendFormat(_T(" /%s %s"),
                          kCmdLineInstallSource,
                          install_source_);
  }
  if (is_silent_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineSilent);
  }
  return cmd_line;
}

CString CommandLineBuilder::GetUpdate() const {
  CString cmd_line;
  cmd_line.Format(_T("/%s"), kCmdLineUpdate);
  return cmd_line;
}

CString CommandLineBuilder::GetIG() const {
  CString cmd_line(GetExtraAndAppArgs(kCmdLineFinishGoogleUpdateInstall));
  if (cmd_line.IsEmpty()) {
    return CString();
  }

  if (!install_source_.IsEmpty()) {
    cmd_line.AppendFormat(_T(" /%s %s"),
                          kCmdLineInstallSource,
                          install_source_);
  }
  if (is_silent_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineSilent);
  }
  if (is_eula_required_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineEulaRequired);
  }
  if (is_offline_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineOfflineInstall);
  }
  return cmd_line;
}

CString CommandLineBuilder::GetHandoffInstall() const {
  CString cmd_line(GetExtraAndAppArgs(kCmdLineAppHandoffInstall));
  if (cmd_line.IsEmpty()) {
    return CString();
  }

  if (!install_source_.IsEmpty()) {
    cmd_line.AppendFormat(_T(" /%s %s"),
                          kCmdLineInstallSource,
                          install_source_);
  }
  if (is_silent_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineSilent);
  }
  if (is_eula_required_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineEulaRequired);
  }
  if (is_offline_set_) {
    cmd_line.AppendFormat(_T(" /%s"), kCmdLineOfflineInstall);
  }
  return cmd_line;
}

CString CommandLineBuilder::GetUG() const {
  CString cmd_line;
  if (is_machine_set_) {
    cmd_line.Format(_T("/%s /%s"),
                    kCmdLineFinishGoogleUpdateUpdate,
                    kCmdLineMachine);
  } else {
    cmd_line.Format(_T("/%s"), kCmdLineFinishGoogleUpdateUpdate);
  }
  return cmd_line;
}

CString CommandLineBuilder::GetUA() const {
  CString cmd_line;
  if (is_uninstall_set_) {
    cmd_line.Format(_T("/%s /%s"), kCmdLineUpdateApps, kCmdLineUninstall);
  } else {
    cmd_line = GetSingleSwitch(kCmdLineUpdateApps);
  }
  return cmd_line;
}

CString CommandLineBuilder::GetRecover() const {
  ASSERT1(!code_red_metainstaller_path_.IsEmpty());
  if (code_red_metainstaller_path_.IsEmpty()) {
    return CString();
  }
  CString cmd_line;
  cmd_line.Format(_T("/%s %s"), kCmdLineRecover, code_red_metainstaller_path_);
  return cmd_line;
}

CString CommandLineBuilder::GetWebPlugin() const {
  ASSERT1(!webplugin_url_domain_.IsEmpty());
  ASSERT1(!webplugin_args_.IsEmpty());
  ASSERT1(!install_source_.IsEmpty());
  if (webplugin_url_domain_.IsEmpty() ||
      webplugin_args_.IsEmpty() ||
      install_source_.IsEmpty()) {
    return CString();
  }
  CString cmd_line;
  CString enclosed_webplugin_url_domain_(webplugin_url_domain_);
  CString enclosed_webplugin_args_(webplugin_args_);
  EnclosePath(&enclosed_webplugin_url_domain_);
  EnclosePath(&enclosed_webplugin_args_);
  // TODO(omaha): Do we want this to handle the urlencoding for us?
  cmd_line.Format(_T("/%s %s %s /%s %s"),
                  kCmdLineWebPlugin,
                  enclosed_webplugin_url_domain_,
                  enclosed_webplugin_args_,
                  kCmdLineInstallSource,
                  install_source_);
  return cmd_line;
}

CString CommandLineBuilder::GetCodeRedCheck() const {
  return GetSingleSwitch(kCmdLineCodeRedCheck);
}

CString CommandLineBuilder::GetComServer() const {
  return kCmdLineComServerDash;
}

CString CommandLineBuilder::GetLegacyHandoff() const {
  ASSERT1(!legacy_manifest_path_.IsEmpty());
  if (legacy_manifest_path_.IsEmpty()) {
    return CString();
  }
  CString cmd_line;
  cmd_line.Format(_T("/%s %s"),
                  kCmdLineLegacyUserManifest,
                  legacy_manifest_path_);
  return cmd_line;
}

CString CommandLineBuilder::GetRegisterProduct() const {
  ASSERT1(app_args_.IsEmpty());
  return GetExtraAndAppArgs(kCmdLineRegisterProduct);
}

CString CommandLineBuilder::GetUnregisterProduct() const {
  ASSERT1(app_args_.IsEmpty());
  return GetExtraAndAppArgs(kCmdLineUnregisterProduct);
}

}  // namespace omaha
