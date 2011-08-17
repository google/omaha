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

#include "omaha/base/command_line_validator.h"

#include <atlbase.h>

#include "omaha/base/command_line_parser.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"

namespace omaha {

CommandLineValidator::CommandLineValidator() : scenario_sequence_number_(0) {
}

CommandLineValidator::~CommandLineValidator() {
  Clear();
}

HRESULT CommandLineValidator::CreateScenario(const CString& scenario_name) {
  if (scenarios_.find(scenario_name) != scenarios_.end()) {
    return E_INVALIDARG;
  }

  ScenarioParameters scenario_parameters;
  scenarios_[scenario_name] = scenario_parameters;
  return S_OK;
}

// TODO(Omaha): Instead of creating the scenario in the map then populating it,
// which requires these methods to know about the map, verify the scenario
// exists, etc. - why not build the scenario then add it to the map? That seems
// more straightforward.
HRESULT CommandLineValidator::AddScenarioParameter(
    const CString& scenario_name,
    const CString& switch_name,
    int num_required_parameters) {
  MapScenariosIter iter = scenarios_.find(scenario_name);
  if (iter == scenarios_.end()) {
    return E_INVALIDARG;
  }

  ScenarioParameter* scenario_parameter =
      new ScenarioParameter(switch_name, num_required_parameters);
  (*iter).second.required.push_back(scenario_parameter);
  return S_OK;
}

HRESULT CommandLineValidator::AddOptionalScenarioParameter(
    const CString& scenario_name,
    const CString& switch_name,
    int num_required_parameters) {
  MapScenariosIter iter = scenarios_.find(scenario_name);
  if (iter == scenarios_.end()) {
    return E_INVALIDARG;
  }

  ScenarioParameter* scenario_parameter =
      new ScenarioParameter(switch_name, num_required_parameters);
  (*iter).second.optional.push_back(scenario_parameter);
  return S_OK;
}

HRESULT CommandLineValidator::CreateScenarioFromCmdLine(
    const CString& command_line,
    CString* scenario_name) {
  ASSERT1(scenario_name);

  CommandLineParser parser;
  HRESULT hr = parser.ParseFromString(command_line);
  if (FAILED(hr)) {
    return hr;
  }

  // Generate a unique scenario name.
  CString scenario_name_str;
  do {
    ++scenario_sequence_number_;
    scenario_name_str.Format(_T("scenario_%d"), scenario_sequence_number_);
  } while (scenarios_.find(scenario_name_str) != scenarios_.end());

  CreateScenario(scenario_name_str);

  int switch_count = parser.GetSwitchCount();
  for (int idx_switch = 0; idx_switch < switch_count; ++idx_switch) {
    CString switch_name;
    hr = parser.GetSwitchNameAtIndex(idx_switch, &switch_name);
    if (FAILED(hr)) {
      return hr;
    }

    int arg_count = 0;
    hr = parser.GetSwitchArgumentCount(switch_name, &arg_count);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddScenarioParameter(scenario_name_str, switch_name, arg_count);
    if (FAILED(hr)) {
      return hr;
    }
  }

  switch_count = parser.GetOptionalSwitchCount();
  for (int idx_switch = 0; idx_switch < switch_count; ++idx_switch) {
    CString switch_name;
    hr = parser.GetOptionalSwitchNameAtIndex(idx_switch, &switch_name);
    if (FAILED(hr)) {
      return hr;
    }

    int arg_count = 0;
    hr = parser.GetOptionalSwitchArgumentCount(switch_name, &arg_count);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddOptionalScenarioParameter(scenario_name_str,
                                      switch_name,
                                      arg_count);
    if (FAILED(hr)) {
      return hr;
    }
  }

  *scenario_name = scenario_name_str;

  return S_OK;
}

HRESULT CommandLineValidator::Validate(
    const CommandLineParser& command_line_parser,
    CString* scenario_name) const {
  // Attempt to verify the data within the command_line_parser against each of
  // the scenarios.
  MapScenariosConstIter scenarios_iter;
  for (scenarios_iter = scenarios_.begin();
       scenarios_iter != scenarios_.end();
       ++scenarios_iter) {
    // Make sure we have a match for the number of switches in this scenario.
    int parser_switch_count = command_line_parser.GetSwitchCount();
    int scenario_required_switch_count =
        (*scenarios_iter).second.required.size();
    int scenario_optional_switch_count =
        (*scenarios_iter).second.optional.size();

    if (parser_switch_count < scenario_required_switch_count ||
        parser_switch_count > scenario_required_switch_count +
                              scenario_optional_switch_count) {
      continue;
    }

    if (DoesScenarioMatch(command_line_parser, (*scenarios_iter).second)) {
      *scenario_name = (*scenarios_iter).first;
      return S_OK;
    }
  }

  return GOOGLEUPDATE_COMMANDLINE_E_NO_SCENARIO_HANDLER_MATCHED;
}

bool CommandLineValidator::DoesScenarioMatch(
    const CommandLineParser& command_line_parser,
    const ScenarioParameters& scenario_parameters) const {
  // Make sure that each switch matches with the right number of arguments.
  ScenarioParameterVectorConstIter parameter_iter;
  for (parameter_iter = scenario_parameters.required.begin();
       parameter_iter != scenario_parameters.required.end();
       ++parameter_iter) {
    CString current_switch_name = (*parameter_iter)->switch_name_;
    // This would probably allow duplicate switches (i.e. /c /c) in a command
    // line.
    if (!command_line_parser.HasSwitch(current_switch_name)) {
      return false;
    }

    int arg_count = 0;
    HRESULT hr = command_line_parser.GetSwitchArgumentCount(current_switch_name,
                                                            &arg_count);
    if (FAILED(hr)) {
      return false;
    }

    int switch_arg_count = (*parameter_iter)->num_required_parameters_;
    if (arg_count != switch_arg_count) {
      return false;
    }
  }

  int parser_optional_switch_count = command_line_parser.GetSwitchCount() -
                                     scenario_parameters.required.size();
  for (parameter_iter = scenario_parameters.optional.begin();
       parser_optional_switch_count != 0 &&
           parameter_iter != scenario_parameters.optional.end();
       ++parameter_iter) {
    CString current_switch_name = (*parameter_iter)->switch_name_;
    // This would probably allow duplicate optional switches (i.e. /oem /oem) in
    // a command line.
    if (!command_line_parser.HasSwitch(current_switch_name)) {
      continue;
    }

    int arg_count = 0;
    HRESULT hr = command_line_parser.GetSwitchArgumentCount(current_switch_name,
                                                            &arg_count);
    if (FAILED(hr)) {
      return false;
    }

    int switch_arg_count = (*parameter_iter)->num_required_parameters_;
    if (arg_count != switch_arg_count) {
      return false;
    }
    --parser_optional_switch_count;
  }

  return parser_optional_switch_count == 0;
}

void CommandLineValidator::Clear() {
  MapScenariosIter scenarios_iter;
  for (scenarios_iter = scenarios_.begin();
       scenarios_iter != scenarios_.end();
       ++scenarios_iter) {
    ScenarioParameterVectorIter param_iter;
    for (param_iter = (*scenarios_iter).second.required.begin();
         param_iter != (*scenarios_iter).second.required.end();
         ++param_iter) {
      delete *param_iter;
    }
    for (param_iter = (*scenarios_iter).second.optional.begin();
         param_iter != (*scenarios_iter).second.optional.end();
         ++param_iter) {
      delete *param_iter;
    }
  }
  scenarios_.clear();
}

}  // namespace omaha

