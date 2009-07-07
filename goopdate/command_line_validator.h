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


#ifndef OMAHA_GOOPDATE_COMMAND_LINE_VALIDATOR_H__
#define OMAHA_GOOPDATE_COMMAND_LINE_VALIDATOR_H__

#include <windows.h>
#include <atlstr.h>

#include <map>
#include <vector>

#include "base/basictypes.h"

namespace omaha {

class CommandLineParser;

// This class allows creation of scenarios for command line combinations and
// then provides a mechanism to validate a command line against those scenarios
// to determine if there's a match.
class CommandLineValidator {
 public:
  class ScenarioParameter {
   public:
    ScenarioParameter(const TCHAR* switch_name, int num_required_parameters)
      : switch_name_(switch_name),
        num_required_parameters_(num_required_parameters) {
    }
    ~ScenarioParameter() {}

    CString switch_name_;
    int num_required_parameters_;

   private:
    DISALLOW_EVIL_CONSTRUCTORS(ScenarioParameter);
  };

  typedef std::vector<ScenarioParameter*> ScenarioParameterVector;
  typedef ScenarioParameterVector::iterator ScenarioParameterVectorIter;
  typedef ScenarioParameterVector::const_iterator
      ScenarioParameterVectorConstIter;

  struct ScenarioParameters {
   public:
    ScenarioParameterVector required;
    ScenarioParameterVector optional;
  };

  typedef std::map<CString, ScenarioParameters> MapScenarios;
  typedef MapScenarios::iterator MapScenariosIter;
  typedef MapScenarios::const_iterator MapScenariosConstIter;

  CommandLineValidator();
  ~CommandLineValidator();

  void Clear();

  // Parses a command line rule and builds a scenario from it.  Returns a
  // generated scenario name.
  // Rules have required and optional parameters. An example of a rule is:
  //     "gu.exe /install <extraargs> [/oem [/appargs <appargs> [/silent"
  HRESULT CreateScenarioFromCmdLine(const CString& command_line,
                                    CString* scenario_name);

  // Validates a CommandLineParser against all scenarios.  If a match, returns
  // S_OK and the scenario_name.  Fails if not a match.
  // command_line_parser must already be compiled before calling.
  HRESULT Validate(const CommandLineParser& command_line_parser,
                   CString* scenario_name) const;

  // Creates a scenario by name.
  HRESULT CreateScenario(const CString& scenario_name);

  // Adds a switch and its parameter count to an existing scenario.
  HRESULT AddScenarioParameter(const CString& scenario_name,
                               const CString& switch_name,
                               int num_required_parameters);
  HRESULT AddOptionalScenarioParameter(const CString& scenario_name,
                                       const CString& switch_name,
                                       int num_required_parameters);

 private:
  bool DoesScenarioMatch(const CommandLineParser& command_line_parser,
                         const ScenarioParameters& scenario_parameters) const;

  int scenario_sequence_number_;
  MapScenarios scenarios_;
  DISALLOW_EVIL_CONSTRUCTORS(CommandLineValidator);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COMMAND_LINE_VALIDATOR_H__

