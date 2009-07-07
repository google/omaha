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

#include "omaha/goopdate/command_line_validator.h"

#include "omaha/goopdate/command_line_parser.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR* kScenario1Name = _T("core");
const TCHAR* kScenario2Name = _T("SomeScenario");
const TCHAR* kScenario3Name = _T("OtherMechanism");

const TCHAR* kScenario1CmdLine = _T("program.exe /lang foo");
const TCHAR* kScenario2CmdLine = _T("program.exe /install x y /service");
const TCHAR* kScenario3CmdLine = _T("prog.exe /install x y /service /lang en");

const TCHAR* kLangSwitch = _T("lang");
const int kLangSwitchArgCount = 1;
const TCHAR* kInstallSwitch = _T("install");
const int kInstallSwitchArgCount = 2;
const TCHAR* kServiceSwitch = _T("service");
const int kServiceSwitchArgCount = 0;

class CommandLineValidatorTest : public testing::Test {
 public:

 protected:
  CommandLineValidatorTest() {
  }

  virtual void SetUp() {
    scenario_match_name_.Empty();

    // This validator only has one scenario.
    validator1_.Clear();
    // "program.exe /lang foo"
    validator1_.CreateScenario(kScenario1Name);
    validator1_.AddScenarioParameter(kScenario1Name,
                                     kLangSwitch,
                                     kLangSwitchArgCount);

    // This validator has three scenarios.
    validator2_.Clear();
    // "program.exe /lang foo"
    validator2_.CreateScenario(kScenario1Name);
    validator2_.AddScenarioParameter(kScenario1Name,
                                     kLangSwitch,
                                     kLangSwitchArgCount);

    // "program.exe /install x y /service"
    validator2_.CreateScenario(kScenario2Name);
    validator2_.AddScenarioParameter(kScenario2Name,
                                     kInstallSwitch,
                                     kInstallSwitchArgCount);
    validator2_.AddScenarioParameter(kScenario2Name,
                                     kServiceSwitch,
                                     kServiceSwitchArgCount);

    // "program.exe /install x y /service /lang en"
    validator2_.CreateScenario(kScenario3Name);
    validator2_.AddScenarioParameter(kScenario3Name,
                                     kInstallSwitch,
                                     kInstallSwitchArgCount);
    validator2_.AddScenarioParameter(kScenario3Name,
                                     kServiceSwitch,
                                     kServiceSwitchArgCount);
    validator2_.AddScenarioParameter(kScenario3Name,
                                     kLangSwitch,
                                     kLangSwitchArgCount);
  }

  virtual void TearDown() {
  }

  CommandLineValidator validator1_;
  CommandLineValidator validator2_;
  CommandLineParser parser_;
  CString scenario_match_name_;
};

TEST_F(CommandLineValidatorTest, BasicScenarioPass) {
  EXPECT_SUCCEEDED(parser_.ParseFromString(kScenario1CmdLine));
  EXPECT_SUCCEEDED(validator1_.Validate(parser_, &scenario_match_name_));
  EXPECT_STREQ(kScenario1Name, scenario_match_name_);
}

TEST_F(CommandLineValidatorTest, BasicScenarioFail) {
  EXPECT_SUCCEEDED(parser_.ParseFromString(_T("goopdate.exe /something bad")));
  EXPECT_FAILED(validator1_.Validate(parser_, &scenario_match_name_));
}

TEST_F(CommandLineValidatorTest, Scenario1PassMulti) {
  EXPECT_SUCCEEDED(parser_.ParseFromString(kScenario1CmdLine));
  EXPECT_SUCCEEDED(validator2_.Validate(parser_, &scenario_match_name_));
  EXPECT_STREQ(kScenario1Name, scenario_match_name_);
}

TEST_F(CommandLineValidatorTest, Scenario2PassMulti) {
  EXPECT_SUCCEEDED(parser_.ParseFromString(kScenario2CmdLine));
  EXPECT_SUCCEEDED(validator2_.Validate(parser_, &scenario_match_name_));
  EXPECT_STREQ(kScenario2Name, scenario_match_name_);
}

TEST_F(CommandLineValidatorTest, Scenario3PassMulti) {
  EXPECT_SUCCEEDED(parser_.ParseFromString(kScenario3CmdLine));
  EXPECT_SUCCEEDED(validator2_.Validate(parser_, &scenario_match_name_));
  EXPECT_STREQ(kScenario3Name, scenario_match_name_);
}

TEST_F(CommandLineValidatorTest, ScenarioFailMulti) {
  EXPECT_SUCCEEDED(parser_.ParseFromString(_T("Goopdate.exe /fail me /here")));
  EXPECT_FAILED(validator2_.Validate(parser_, &scenario_match_name_));
}

}  // namespace omaha

