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

#include "omaha/goopdate/goopdate_command_line_validator.h"

#include "omaha/common/const_cmd_line.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/command_line_parser.h"
#include "omaha/goopdate/command_line_validator.h"
#include "omaha/goopdate/extra_args_parser.h"

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
  cmd_line.Format(_T("/%s [/%s"), kCmdLineCore, kCmdLineNoCrashHandler);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCore);

  // gu.exe /crashhandler
  cmd_line.Format(_T("/%s"), kCmdLineCrashHandler);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCrashHandler);

  // gu.exe /svc
  cmd_line.Format(_T("/%s"), kCmdLineService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnService);

  // gu.exe /regsvc
  cmd_line.Format(_T("/%s"), kCmdLineRegisterService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnServiceRegister);

  // gu.exe /unregsvc
  cmd_line.Format(_T("/%s"), kCmdLineUnregisterService);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnServiceUnregister);

  // gu.exe /regserver
  cmd_line.Format(_T("/%s"), kCmdRegServer);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRegServer);

  // gu.exe /unregserver
  cmd_line.Format(_T("/%s"), kCmdUnregServer);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUnregServer);

  // gu.exe /netdiags
  cmd_line.Format(_T("/%s"), kCmdLineNetDiags);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnNetDiags);

  // gu.exe /crash
  cmd_line.Format(_T("/%s"), kCmdLineCrash);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCrash);

  // gu.exe -Embedding. The -Embedding text is injected via COM.
  CreateScenario(kCmdLineComServerDash,
                 &GoopdateCommandLineValidator::OnComServer);

  // gu.exe /install <extraargs> [/appargs <appargs> [/installsource source
  //        [/silent [/eularequired [/oem [/installelevated
  cmd_line.Format(_T("/%s extra [/%s appargs [/%s src [/%s [/%s [/%s [/%s"),
                  kCmdLineInstall,
                  kCmdLineAppArgs,
                  kCmdLineInstallSource,
                  kCmdLineSilent,
                  kCmdLineEulaRequired,
                  kCmdLineOem,
                  kCmdLineInstallElevated);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnInstall);

  // gu.exe /update
  cmd_line.Format(_T("/%s"), kCmdLineUpdate);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUpdate);

  // gu.exe /ig <extraargs> [/appargs <appargs> [/installsource source
  //        [/silent [/eularequired [/offlineinstall
  cmd_line.Format(_T("/%s extraargs [/%s appargs [/%s source [/%s [/%s [/%s"),
                  kCmdLineFinishGoogleUpdateInstall,
                  kCmdLineAppArgs,
                  kCmdLineInstallSource,
                  kCmdLineSilent,
                  kCmdLineEulaRequired,
                  kCmdLineOfflineInstall);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnFinishInstallGoopdate);

  // gu.exe /handoff <extraargs> [/appargs <appargs> [/installsource source
  //        [/silent [/eularequired [/offlineinstall
  cmd_line.Format(_T("/%s extra [/%s appargs [/%s source [/%s [/%s [/%s"),
                  kCmdLineAppHandoffInstall,
                  kCmdLineAppArgs,
                  kCmdLineInstallSource,
                  kCmdLineSilent,
                  kCmdLineEulaRequired,
                  kCmdLineOfflineInstall);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnInstallHandoffWorker);

  // gu.exe /ua [/uninstall
  cmd_line.Format(_T("/%s [/%s"), kCmdLineUpdateApps, kCmdLineUninstall);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnUpdateApps);

  // gu.exe /ug [/machine
  cmd_line.Format(_T("/%s [/%s"),
                  kCmdLineFinishGoogleUpdateUpdate, kCmdLineMachine);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnFinishUpdateGoopdate);

  // gu.exe /report <crash_filename> [/machine
  //        [/custom_info <custom_info_filename>
  cmd_line.Format(_T("/%s filename [/%s [/%s customfilename"),
                  kCmdLineReport,
                  kCmdLineMachine,
                  kCmdLineCustomInfoFileName);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnReportCrash);

  // gu.exe /report /i <crash_filename> [/machine
  cmd_line.Format(_T("/%s /%s filename [/%s"),
                  kCmdLineReport,
                  kCmdLineInteractive,
                  kCmdLineMachine);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnReportCrashInteractive);

  // gu.exe /pi <domainurl> <args> /installsource oneclick
  cmd_line.Format(_T("/%s domainurl args /%s oneclick"),
                  kCmdLineWebPlugin,
                  kCmdLineInstallSource);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnWebPlugin);

  // gu.exe /cr
  cmd_line.Format(_T("/%s"), kCmdLineCodeRedCheck);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnCodeRed);

  // gu.exe /recover <repair_file>
  cmd_line.Format(_T("/%s repairfile"), kCmdLineRecover);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRecover);

  // gu.exe /recover /machine <repair_file>
  cmd_line.Format(_T("/%s /%s repairfile"), kCmdLineRecover, kCmdLineMachine);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRecoverMachine);

  // gu.exe /registerproduct "extraargs" [/installsource source
  cmd_line.Format(_T("/%s extraargs [/%s source"),
                  kCmdLineRegisterProduct,
                  kCmdLineInstallSource);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnRegisterProduct);

  // gu.exe /unregisterproduct "extraargs"
  cmd_line.Format(_T("/%s extraargs"), kCmdLineUnregisterProduct);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUnregisterProduct);

  //
  // Legacy support command lines.
  //

  // gu.exe /uiuser <manifestfilename>
  cmd_line.Format(_T("/%s filename"),
                  kCmdLineLegacyUserManifest);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUiUserManifest);

  // TODO(omaha):  Can we remove this case or is it here for back compat?
  // gu.exe /ui /lang en <manifestfilename>
  cmd_line.Format(_T("/%s /%s en filename"),
                  kCmdLineLegacyUi,
                  kCmdLineLegacyLang);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUiLangManifest);

  // gu.exe /ui <manifestfilename>
  cmd_line.Format(_T("/%s filename"), kCmdLineLegacyUi);
  CreateScenario(cmd_line, &GoopdateCommandLineValidator::OnUiManifest);

  // gu.exe /handoff <extraargs> /lang <en> [/installsource source
  cmd_line.Format(_T("/%s extraargs /%s en [/%s source"),
                  kCmdLineAppHandoffInstall,
                  kCmdLineLegacyLang,
                  kCmdLineInstallSource);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnInstallHandoffWorkerLegacy);

  // gu.exe /install <extraargs> /installsource source /lang en.
  cmd_line.Format(_T("/%s extraargs /%s source /%s en"),
                  kCmdLineInstall, kCmdLineInstallSource, kCmdLineLegacyLang);
  CreateScenario(cmd_line,
                 &GoopdateCommandLineValidator::OnInstallWithSourceLegacy);
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
  scenario_cmd_line.Format(_T("prog.exe %s"), cmd_line);

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
                                 &(args_->extra));
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

HRESULT GoopdateCommandLineValidator::OnNetDiags() {
  args_->mode = COMMANDLINE_MODE_NETDIAGS;
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

HRESULT GoopdateCommandLineValidator::OnInstall() {
  args_->mode = COMMANDLINE_MODE_INSTALL;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &(args_->install_source));
  args_->is_silent_set = parser_->HasSwitch(kCmdLineSilent);
  args_->is_eula_required_set = parser_->HasSwitch(kCmdLineEulaRequired);
  args_->is_oem_set = parser_->HasSwitch(kCmdLineOem);
  args_->is_install_elevated = parser_->HasSwitch(kCmdLineInstallElevated);
  return GetExtraAndAppArgs(kCmdLineInstall);
}

HRESULT GoopdateCommandLineValidator::OnInstallWithSourceLegacy() {
  args_->mode = COMMANDLINE_MODE_INSTALL;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &(args_->install_source));
  parser_->GetSwitchArgumentValue(kCmdLineLegacyLang,
                                  0,
                                  &(args_->extra.language));
  return GetExtraAndAppArgs(kCmdLineInstall);
}

HRESULT GoopdateCommandLineValidator::OnUpdate() {
  args_->mode = COMMANDLINE_MODE_UPDATE;
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnInstallHandoffWorker() {
  args_->mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &(args_->install_source));
  args_->is_silent_set = parser_->HasSwitch(kCmdLineSilent);
  args_->is_eula_required_set = parser_->HasSwitch(kCmdLineEulaRequired);
  args_->is_offline_set = parser_->HasSwitch(kCmdLineOfflineInstall);
  return GetExtraAndAppArgs(kCmdLineAppHandoffInstall);
}

HRESULT GoopdateCommandLineValidator::OnInstallHandoffWorkerLegacy() {
  args_->mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  HRESULT hr = GetExtraAndAppArgs(kCmdLineAppHandoffInstall);
  if (FAILED(hr)) {
    return hr;
  }
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &(args_->install_source));
  return parser_->GetSwitchArgumentValue(kCmdLineLegacyLang,
                                         0,
                                         &(args_->extra.language));
}

HRESULT GoopdateCommandLineValidator::OnUpdateApps() {
  args_->mode = COMMANDLINE_MODE_UA;
  args_->is_uninstall_set = parser_->HasSwitch(kCmdLineUninstall);
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnFinishInstallGoopdate() {
  args_->mode = COMMANDLINE_MODE_IG;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &(args_->install_source));
  args_->is_silent_set = parser_->HasSwitch(kCmdLineSilent);
  args_->is_eula_required_set = parser_->HasSwitch(kCmdLineEulaRequired);
  args_->is_offline_set = parser_->HasSwitch(kCmdLineOfflineInstall);
  return GetExtraAndAppArgs(kCmdLineFinishGoogleUpdateInstall);
}

HRESULT GoopdateCommandLineValidator::OnFinishUpdateGoopdate() {
  args_->mode = COMMANDLINE_MODE_UG;
  args_->is_machine_set = parser_->HasSwitch(kCmdLineMachine);
  return S_OK;
}

HRESULT GoopdateCommandLineValidator::OnReportCrash() {
  args_->mode = COMMANDLINE_MODE_REPORTCRASH;
  args_->is_machine_set = parser_->HasSwitch(kCmdLineMachine);
  parser_->GetSwitchArgumentValue(kCmdLineCustomInfoFileName,
                                  0,
                                  &(args_->custom_info_filename));
  return parser_->GetSwitchArgumentValue(kCmdLineReport,
                                         0,
                                         &(args_->crash_filename));
}

HRESULT GoopdateCommandLineValidator::OnReportCrashInteractive() {
  args_->mode = COMMANDLINE_MODE_REPORTCRASH;
  args_->is_interactive_set = true;
  args_->is_machine_set = parser_->HasSwitch(kCmdLineMachine);
  return parser_->GetSwitchArgumentValue(kCmdLineInteractive,
                                         0,
                                         &(args_->crash_filename));
}

HRESULT GoopdateCommandLineValidator::OnUiManifest() {
  args_->mode = COMMANDLINE_MODE_LEGACYUI;
  return parser_->GetSwitchArgumentValue(kCmdLineLegacyUi,
                                         0,
                                         &(args_->legacy_manifest_path));
}

HRESULT GoopdateCommandLineValidator::OnUiLangManifest() {
  args_->mode = COMMANDLINE_MODE_LEGACYUI;
  HRESULT hr = parser_->GetSwitchArgumentValue(kCmdLineLegacyLang,
                                               0,
                                               &(args_->extra.language));
  if (FAILED(hr)) {
    return hr;
  }

  return parser_->GetSwitchArgumentValue(kCmdLineLegacyLang,
                                         1,
                                         &(args_->legacy_manifest_path));
}

HRESULT GoopdateCommandLineValidator::OnUiUserManifest() {
  args_->mode = COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF;
  return parser_->GetSwitchArgumentValue(kCmdLineLegacyUserManifest,
                                         0,
                                         &(args_->legacy_manifest_path));
}

HRESULT GoopdateCommandLineValidator::OnWebPlugin() {
  HRESULT hr = parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                               0,
                                               &(args_->install_source));
  if (FAILED(hr)) {
    return hr;
  }
  // Validate install_source value.
  args_->install_source.MakeLower();
  if (args_->install_source.Compare(kCmdLineInstallSource_OneClick) != 0) {
    args_->install_source.Empty();
    return E_INVALIDARG;
  }

  args_->mode = COMMANDLINE_MODE_WEBPLUGIN;

  CString urldomain;
  hr = parser_->GetSwitchArgumentValue(kCmdLineWebPlugin,
                                       0,
                                       &urldomain);
  if (FAILED(hr)) {
    return hr;
  }
  hr = StringUnescape(urldomain, &(args_->webplugin_urldomain));
  if (FAILED(hr)) {
    return hr;
  }

  CString webplugin_args;
  hr = parser_->GetSwitchArgumentValue(kCmdLineWebPlugin,
                                       1,
                                       &webplugin_args);
  if (FAILED(hr)) {
    return hr;
  }
  return StringUnescape(webplugin_args, &(args_->webplugin_args));
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
      &(args_->code_red_metainstaller_path));
}

HRESULT GoopdateCommandLineValidator::OnRecoverMachine() {
  args_->mode = COMMANDLINE_MODE_RECOVER;
  args_->is_machine_set = true;
  return parser_->GetSwitchArgumentValue(
      kCmdLineMachine,
      0,
      &(args_->code_red_metainstaller_path));
}

HRESULT GoopdateCommandLineValidator::OnRegisterProduct() {
  args_->mode = COMMANDLINE_MODE_REGISTER_PRODUCT;
  parser_->GetSwitchArgumentValue(kCmdLineInstallSource,
                                  0,
                                  &(args_->install_source));
  return GetExtraAndAppArgs(kCmdLineRegisterProduct);
}

HRESULT GoopdateCommandLineValidator::OnUnregisterProduct() {
  args_->mode = COMMANDLINE_MODE_UNREGISTER_PRODUCT;
  return GetExtraAndAppArgs(kCmdLineUnregisterProduct);
}

}  // namespace omaha

