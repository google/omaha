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

#pragma warning(push)
// C4548: expression before comma has no effect
#pragma warning(disable : 4548)
#include <atlstr.h>
#pragma warning(pop)

// Overview
//
// The program to be tested instantiates a StepTestStepper, passing in a
// unique identifier (by convention, a GUID in registry format). It then
// calls StepTestStepper::Step() with values representing specific points in
// execution of the program.
//
// Then in test/omaha_system_test.cpp, derive a concrete subclass of
// StepTestWatcher. In particular, implement VerifyStep(), which takes each of
// the values passed in through the tested program's StepTestStepper::Step()
// function, and verifies that the state of the system matches what it should
// be.

class StepTestBase {
public:
  StepTestBase(const TCHAR *name);
  virtual ~StepTestBase();

protected:
  HANDLE step_reached_event_;
  HANDLE resume_event_;
  HKEY state_key_;

  static const TCHAR *RESUME_EVENT_TEMPLATE;
  static const TCHAR *STEP_REACHED_EVENT_TEMPLATE;
  static const TCHAR *STATE_REG_KEY_TEMPLATE;
  static const TCHAR *STATE_REG_VALUE_NAME;

  void Terminate();
};

class StepTestStepper : public StepTestBase {
public:
  StepTestStepper(const TCHAR *name);
  virtual ~StepTestStepper();

  void Step(DWORD step);
private:
  bool is_someone_listening_;
};

class StepTestWatcher : public StepTestBase {
public:
  StepTestWatcher(const TCHAR *name);
  virtual ~StepTestWatcher();

  void Go();

protected:
  // Checks the state of the machine once the tested program has reached the
  // given step.
  //
  // @param step: the step to be verified
  // @param testing_complete: pointer to bool that should be set to true iff
  // the testing instance is done testing and should now terminate.
  // @return true iff the test of this step passed.
  virtual bool VerifyStep(DWORD step, bool *testing_complete) = 0;

  // Prints instructions to the console telling the human tester what the
  // expected setup of the system should be.
  virtual void PrintIntroduction() = 0;

  // Prints a statement to the console indicating that testing is complete.
  virtual void PrintConclusion() = 0;

  // Returns true iff the given registry key exists on this machine.
  bool RegistryKeyExists(HKEY root, const TCHAR *name);

  // Returns true iff the given registry value exists on this machine.
  bool RegistryValueExists(HKEY root, const TCHAR *key_name,
    const TCHAR *value_name);

  CString name_;
};
