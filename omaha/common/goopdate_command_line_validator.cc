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

#include "omaha/common/goopdate_command_line_validator.h"
#include "omaha/base/command_line_parser.h"
#include "omaha/base/command_line_validator.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/extra_args_parser.h"

namespace omaha {

GoopdateCommandLineValidator::GoopdateCommandLineValidator() {
}

GoopdateCommandLineValidator::~GoopdateCommandLineValidator() {
}

HRESULT GoopdateCommandLineValidator::Setup() {
  validator_.reset(new CommandLineValidator);

  CString cmd_line;

  // gu.exe
  cmd_line.Empty();
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnNoArgs);

  // gu.exe /c [/nocrashserver
  SafeCStringFormat(&cmd_line, _T("/%s [/%s"),
                    kCmdLineCore, kCmdLineNoCrashHandler);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCore);

  // gu.exe /crashhandler
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineCrashHandler);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCrashHandler);

  // gu.exe /svc
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnService);

  // gu.exe /medsvc
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineMediumService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnMediumService);

  // gu.exe /regsvc
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineRegisterService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnServiceRegister);

  // gu.exe /unregsvc
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineUnregisterService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnServiceUnregister);

  // gu.exe /regserver
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdRegServer);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRegServer);

  // gu.exe /unregserver
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdUnregServer);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUnregServer);

  // gu.exe /crash
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineCrash);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCrash);

  // gu.exe -Embedding. The -Embedding text is injected via COM.
  CreateScenario(kCmdLineComServerDash,
                 &GoopdateCommandLineValidator::OnComServer);

  // COM server mode, but only for the broker.
  CreateScenario(kCmdLineComBroker, &GoopdateCommandLineValidator::OnComBroker);

  // COM server mode, but only for the OnDemand.
  CreateScenario(kCmdLineOnDemand, &GoopdateCommandLineValidator::OnDemand);

  // gu.exe /install <extraargs> [/appargs <appargs> [/installsource source
  //        [/silent [/alwayslaunchcmd [/eularequired [/oem [/installelevated
  //        [/sessionid <sid> [/enterprise
  SafeCStringFormat(
      &cmd_line, _T("/%s extra [/%s appargs [/%s src [/%s [/%s [/%s [/%s [/%s ")
                 _T("[/%s sid [/%s"),
                 kCmdLineInstall,
                 kCmdLineAppArgs,
                 kCmdLineInstallSource,
                 kCmdLineSilent,
                 kCmdLineAlwaysLaunchCmd,
                 kCmdLineEulaRequired,
                 kCmdLineOem,
                 kCmdLineInstallElevated,
                 kCmdLineSessionId,
                 kCmdLineEnterprise);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnInstall);

  // gu.exe /update [/sessionid <sid>
  SafeCStringFormat(&cmd_line, _T("/%s [/%s sid"),
                    kCmdLineUpdate, kCmdLineSessionId);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUpdate);

  // gu.exe /handoff <extraargs> [/appargs <appargs> [/installsource source
  //        [/silent [/alwayslaunchcmd [/eularequired [/offlineinstall
  //        [/offlinedir <dir> [/sessionid <sid> [/enterprise
  SafeCStringFormat(
      &cmd_line, _T("/%s extra [/%s appargs [/%s src [/%s [/%s [/%s [/%s ")
                 _T("[/%s dir [/%s sid [/%s"),
                 kCmdLineAppHandoffInstall,
                 kCmdLineAppArgs,
                 kCmdLineInstallSource,
                 kCmdLineSilent,
                 kCmdLineAlwaysLaunchCmd,
                 kCmdLineEulaRequired,
                 kCmdLineLegacyOfflineInstall,
                 kCmdLineOfflineDir,
                 kCmdLineSessionId,
                 kCmdLineEnterprise);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnInstallHandoffWorker);

  // gu.exe /ua [/installsource source [/machine
  SafeCStringFormat(&cmd_line, _T("/%s [/%s source [/%s"),
                    kCmdLineUpdateApps, kCmdLineInstallSource, kCmdLineMachine);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnUpdateApps);

  // gu.exe /report <crash_filename> [/machine
  //        [/custom_info <custom_info_filename>
  SafeCStringFormat(&cmd_line, _T("/%s filename [/%s [/%s customfilename"),
                    kCmdLineReport,
                    kCmdLineMachine,
                    kCmdLineCustomInfoFileName);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnReportCrash);

  // gu.exe /report /i <crash_filename> [/machine
  SafeCStringFormat(&cmd_line, _T("/%s /%s filename [/%s"),
                    kCmdLineReport,
                    kCmdLineInteractive,
                    kCmdLineMachine);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnReportCrashInteractive);

  // gu.exe /cr
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineCodeRedCheck);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCodeRed);

  // gu.exe /recover <repair_file>
  SafeCStringFormat(&cmd_line, _T("/%s repairfile"), kCmdLineRecover);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRecover);

  // gu.exe /recover /machine <repair_file>
  SafeCStringFormat(&cmd_line, _T("/%s /%s repairfile"),
                    kCmdLineRecover, kCmdLineMachine);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRecoverMachine);

  // gu.exe /uninstall
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineUninstall);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUninstall);

  // gu.exe /registerproduct "extraargs" [/installsource source
  SafeCStringFormat(&cmd_line, _T("/%s extraargs [/%s source"),
                  kCmdLineRegisterProduct,
                  kCmdLineInstallSource);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRegisterProduct);

  // gu.exe /unregisterproduct "extraargs"
  SafeCStringFormat(&cmd_line, _T("/%s extraargs"), kCmdLineUnregisterProduct);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUnregisterProduct);

  // gu.exe /ping pingstring
  SafeCStringFormat(&cmd_line, _T("/%s pingstring"), kCmdLinePing);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnPing);

  // gu.exe /healthcheck
  SafeCStringFormat(&cmd_line, _T("/%s"), kCmdLineHealthCheck);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnHealthCheck);

  return S_OK;
}

// TODO(Omaha): Add check that each scenario is unique and does not overlap an
// existing one in DBG builds.
HRESULT GoopdateCommandLineValidator::Validate(const CommandLineParser* parser,
                                               CommandLineArgs* args) {
  ASSERT1(parser);
  ASSERT1(args);

  parser_ = parser;
  args_ = args;

  CString scenario_name;
  HRESULT hr = validator_->Validate(*parser_, &scenario_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GoopdateCommandLineValidator::Validate Failed][0x%x]"),
                  hr));
    return hr;
  }

  MapScenarioHandlersIter iter = scenario_handlers_.find(scenario_name);
  if (iter == scenario_handlers_.end()) {
    ASSERT1(false);
    return GOOGLEUPDATE_COMMANDLINE_E_NO_SCENARIO_HANDLER;
  }

  ScenarioHandler handler = (*iter).second;
  return (this->*handler)();
}

void GoopdateCommandLineValidator::CreateScenario(const TCHAR* cmd_line,
                                                  ScenarioHandler handler) {
  // Prepend the program name onto the cmd_line.
  CString scenario_cmd_line;
  SafeCStringFormat(&scenario_cmd_line, _T("prog.exe %s"), cmd_line);

  CString scenario_name;
  validator_->CreateScenarioFromCmdLine(scenario_cmd_line, &scenario_name);
  // TODO(omaha): Make sure it doesn't already exist.
  scenario_handlers_[scenario_name] = handler;
}

HRESULT GoopdateCommandLineValidator::GetExtraAndAppArgs(const CString& name) {
  HRESULT hr = parser_->GetSwitchArgumentValue(name,
                                               0,
                                               &args_->extra_args_str);
  if (FAILED(hr)) {
    return hr;
  }

  hr = parser_->GetSwitchArgumentValue(kCmdLineAppArgs,
                                       0,
                                       &args_->app_args_str);
  if (FAILED(hr)) {
    args_->app_args_str.Empty();
  }

  ExtraArgsParser extra_args_parser;
  return extra_args_parser.Parse(args_->extra_args_str,
                                 args_->app_args_str,
                                 &args_->extra);
}

HRESULT GoopdateCommandLineValidator::OnNoArgs() {
  args_->mode = COMMANDLINE_MODE_NOARGS;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnCore() {
  args_->mode = COMMANDLINE_MODE_CORE;
  args_->is_crash_handler_disabled = parser_->HasSwitch(kCmdLineNoCrashHandler);

  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnCrashHandler() {
  args_->mode = COMMANDLINE_MODE_CRASH_HANDLER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnService() {
  args_->mode = COMMANDLINE_MODE_SERVICE;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnMediumService() {
  args_->mode = COMMANDLINE_MODE_MEDIUM_SERVICE;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnServiceRegister() {
  args_->mode = COMMANDLINE_MODE_SERVICE_REGISTER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnServiceUnregister() {
  args_->mode = COMMANDLINE_MODE_SERVICE_UNREGISTER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnRegServer() {
  args_->mode = COMMANDLINE_MODE_REGSERVER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnUnregServer() {
  args_->mode = COMMANDLINE_MODE_UNREGSERVER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnCrash() {
  args_->mode = COMMANDLINE_MODE_CRASH;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnComServer() {
  args_->mode = COMMANDLINE_MODE_COMSERVER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnComBroker() {
  args_->mode = COMMANDLINE_MODE_COMBROKER;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnDemand() {
  args_->mode = COMMANDLINE_MODE_ONDEMAND;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnInstall() {
  args_->mode = COMMANDLINE_MODE_INSTALL;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &args_->install_source);
  parser_->GetSwitchArgumentValue(kCmdLineSessionId,
                                  0,
                                  &args_->session_id);
  args_->is_silent_set = parser_->HasSwitch(kCmdLineSilent);
  args_->is_always_launch_cmd_set = parser_->HasSwitch(kCmdLineAlwaysLaunchCmd);
  args_->is_enterprise_set = parser_->HasSwitch(kCmdLineEnterprise);
  args_->is_eula_required_set = parser_->HasSwitch(kCmdLineEulaRequired);
  args_->is_oem_set = parser_->HasSwitch(kCmdLineOem);
  args_->is_install_elevated = parser_->HasSwitch(kCmdLineInstallElevated);
  return GetExtraAndAppArgs(kCmdLineInstall);
}

HRESULT GoopdateCommandLineValidator::OnUpdate() {
  args_->mode = COMMANDLINE_MODE_UPDATE;
  parser_->GetSwitchArgumentValue(kCmdLineSessionId,
                                  0,
                                  &args_->session_id);
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnInstallHandoffWorker() {
  args_->mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &args_->install_source);
  parser_->GetSwitchArgumentValue(kCmdLineSessionId,
                                  0,
                                  &args_->session_id);
  args_->is_silent_set = parser_->HasSwitch(kCmdLineSilent);
  args_->is_always_launch_cmd_set = parser_->HasSwitch(kCmdLineAlwaysLaunchCmd);
  args_->is_enterprise_set = parser_->HasSwitch(kCmdLineEnterprise);
  args_->is_eula_required_set = parser_->HasSwitch(kCmdLineEulaRequired);
  args_->is_offline_set = parser_->HasSwitch(kCmdLineLegacyOfflineInstall) ||
                          parser_->HasSwitch(kCmdLineOfflineDir);

  CString offline_dir;
  parser_->GetSwitchArgumentValue(kCmdLineOfflineDir, 0, &offline_dir);
  if (!offline_dir.IsEmpty()) {
    CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
    HRESULT hr = builder.SetOfflineDirName(offline_dir);
    if (FAILED(hr)) {
      return hr;
    }

    args_->offline_dir_name = builder.offline_dir_name();
  }

  return GetExtraAndAppArgs(kCmdLineAppHandoffInstall);
}

HRESULT GoopdateCommandLineValidator::OnUpdateApps() {
  args_->mode = COMMANDLINE_MODE_UA;
  args_->is_machine_set = parser_->HasSwitch(kCmdLineMachine);
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &args_->install_source);
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnReportCrash() {
  args_->mode = COMMANDLINE_MODE_REPORTCRASH;
  args_->is_machine_set = parser_->HasSwitch(kCmdLineMachine);
  parser_->GetSwitchArgumentValue(kCmdLineCustomInfoFileName,
                                  0,
                                  &args_->custom_info_filename);
  return parser_->GetSwitchArgumentValue(kCmdLineReport,
                                         0,
                                         &args_->crash_filename);
}

HRESULT GoopdateCommandLineValidator::OnReportCrashInteractive() {
  args_->mode = COMMANDLINE_MODE_REPORTCRASH;
  args_->is_interactive_set = true;
  args_->is_machine_set = parser_->HasSwitch(kCmdLineMachine);
  return parser_->GetSwitchArgumentValue(kCmdLineInteractive,
                                         0,
                                         &args_->crash_filename);
}

HRESULT GoopdateCommandLineValidator::OnCodeRed() {
  args_->mode = COMMANDLINE_MODE_CODE_RED_CHECK;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnRecover() {
  args_->mode = COMMANDLINE_MODE_RECOVER;
  return parser_->GetSwitchArgumentValue(
      kCmdLineRecover,
      0,
      &args_->code_red_metainstaller_path);
}

HRESULT GoopdateCommandLineValidator::OnRecoverMachine() {
  args_->mode = COMMANDLINE_MODE_RECOVER;
  args_->is_machine_set = true;
  return parser_->GetSwitchArgumentValue(
      kCmdLineMachine,
      0,
      &args_->code_red_metainstaller_path);
}

HRESULT GoopdateCommandLineValidator::OnUninstall() {
  args_->mode = COMMANDLINE_MODE_UNINSTALL;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnRegisterProduct() {
  args_->mode = COMMANDLINE_MODE_REGISTER_PRODUCT;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &args_->install_source);
  return GetExtraAndAppArgs(kCmdLineRegisterProduct);
}

HRESULT GoopdateCommandLineValidator::OnUnregisterProduct() {
  args_->mode = COMMANDLINE_MODE_UNREGISTER_PRODUCT;
  return GetExtraAndAppArgs(kCmdLineUnregisterProduct);
}

HRESULT GoopdateCommandLineValidator::OnPing() {
  args_->mode = COMMANDLINE_MODE_PING;
  return parser_->GetSwitchArgumentValue(kCmdLinePing,
                                         0,
                                         &args_->ping_string);
}

HRESULT GoopdateCommandLineValidator::OnHealthCheck() {
  args_->mode = COMMANDLINE_MODE_HEALTH_CHECK;
  return S_OK;
}

}  // namespace omaha

