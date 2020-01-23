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

#ifndef OMAHA_COMMON_GOOPDATE_COMMAND_LINE_VALIDATOR_H__
#define OMAHA_COMMON_GOOPDATE_COMMAND_LINE_VALIDATOR_H__

#include <windows.h>
#include <atlstr.h>
#include <map>
#include <memory>

#include "base/basictypes.h"

namespace omaha {

struct CommandLineArgs;
class CommandLineParser;
class CommandLineValidator;

// Validates all of the command line permutations for googleupdate.exe.
class GoopdateCommandLineValidator {
 public:
  typedef HRESULT (GoopdateCommandLineValidator::*ScenarioHandler)();
  typedef std::map<CString, ScenarioHandler> MapScenarioHandlers;
  typedef MapScenarioHandlers::iterator MapScenarioHandlersIter;

  GoopdateCommandLineValidator();
  ~GoopdateCommandLineValidator();

  // Sets up the scenarios.
  HRESULT Setup();

  // Validates a pre-parsed parser against the scenarios and returns a
  // CommandLineArgs structure filled in with the proper values.
  HRESULT Validate(const CommandLineParser* parser, CommandLineArgs* args);

 private:
  // Specific command-line scenario handlers.
  HRESULT OnNoArgs();
  HRESULT OnCore();
  HRESULT OnCrashHandler();
  HRESULT OnService();
  HRESULT OnMediumService();
  HRESULT OnServiceRegister();
  HRESULT OnServiceUnregister();
  HRESULT OnRegServer();
  HRESULT OnUnregServer();
  HRESULT OnCrash();
  HRESULT OnComServer();
  HRESULT OnComBroker();
  HRESULT OnDemand();
  HRESULT OnInstall();
  HRESULT OnUpdate();
  HRESULT OnInstallHandoffWorker();
  HRESULT OnUpdateApps();
  HRESULT OnReportCrash();
  HRESULT OnReportCrashInteractive();
  HRESULT OnCodeRed();
  HRESULT OnRecover();
  HRESULT OnRecoverMachine();
  HRESULT OnUninstall();
  HRESULT OnRegisterProduct();
  HRESULT OnUnregisterProduct();
  HRESULT OnPing();
  HRESULT OnHealthCheck();
  HRESULT OnRegisterMsiHelper();

  void CreateScenario(const TCHAR* cmd_line, ScenarioHandler handler);

  HRESULT GetExtraAndAppArgs(const CString& switch_name);

  const CommandLineParser* parser_;
  CommandLineArgs* args_;
  std::unique_ptr<CommandLineValidator> validator_;
  MapScenarioHandlers scenario_handlers_;

  DISALLOW_COPY_AND_ASSIGN(GoopdateCommandLineValidator);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_GOOPDATE_COMMAND_LINE_VALIDATOR_H__

