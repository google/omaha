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
#include <regstr.h>
#include "omaha/mi_exe_stub/mi_step_test.h"
#include "omaha/test/step_test.h"

class MIWatcher : public StepTestWatcher {
public:
  MIWatcher() : StepTestWatcher(MI_STEP_TEST_NAME),
    system_should_be_clean_(true) {
  }

  ~MIWatcher() {
  }

protected:
  bool VerifyStep(DWORD step, bool *testing_complete) {
    bool result = true;
    switch (step) {
      case MI_STEP_VERIFY_PRECONDITIONS:
        if (system_should_be_clean_) {
          result = VerifyCleanSystem();
        } else {
          result = false;
          _tprintf(_T("Unexpected state.\r\n"));
        }
        break;
      case MI_STEP_PROGRAM_START:
        if (system_should_be_clean_) {
          result = VerifyCleanSystem();
          system_should_be_clean_ = false;
        } else {
          result = false;
          _tprintf(_T("Unexpected state.\r\n"));
        }
          break;
      case MI_STEP_PROGRAM_END:
        result  = VerifyTestFooInstalled();
        result &= VerifyOmahaInstalled();
        break;
    }
    *testing_complete = (step >= MI_STEP_PROGRAM_END);
    return result;
  }

  void PrintIntroduction() {
    _tprintf(
      _T("Ready to test mi.exe.\r\n")
      _T("System should not have any version of Omaha installed on it.\r\n")
      _T("System should not have any version of \"!!! Test Foo\" installed on it.\r\n")
      _T("Please run SampleSetup.exe now.\r\n")
    );
  }

  void PrintConclusion() {
    _tprintf(
      _T("Test of mi.exe is complete.\r\n")
    );
  }

private:
  bool system_should_be_clean_;

#define TEST_FOO_REG_UNINST_KEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{8EE7241A-9AFD-324A-884C-B62DC5DAE506}"
#define TEST_FOO_V2_REG_UNINST_KEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{EE93BF87-0CDD-3F0B-A4D6-3B3A1C54E3FA}"

  bool VerifyCleanSystem() {
    if (RegistryKeyExists(HKEY_LOCAL_MACHINE, _T(TEST_FOO_REG_UNINST_KEY))) {
      _tprintf(_T("Product !!! Test Foo (old version) appears to be installed on this system.\r\n"));
      return false;
    }
    if (RegistryKeyExists(HKEY_LOCAL_MACHINE, _T(TEST_FOO_V2_REG_UNINST_KEY))) {
      _tprintf(_T("Product !!! Test Foo appears to be installed on this system.\r\n"));
      return false;
    }
    if (RegistryValueExists(HKEY_LOCAL_MACHINE, REGSTR_PATH_RUN, _T("Google Update"))) {
      _tprintf(_T("Omaha Run key is present on this system.\r\n"));
      return false;
    }
    return true;
  }

  bool VerifyTestFooInstalled() {
    if (!RegistryKeyExists(HKEY_LOCAL_MACHINE, _T(TEST_FOO_V2_REG_UNINST_KEY))) {
      _tprintf(_T("Product !!! Test Foo is not installed on this system.\r\n"));
      return false;
    }
    return true;
  }

  bool VerifyOmahaInstalled() {
    if (!RegistryValueExists(HKEY_LOCAL_MACHINE, REGSTR_PATH_RUN, _T("Google Update"))) {
      _tprintf(_T("Omaha Run key is missing on this system.\r\n"));
      return false;
    }
    return true;
  }
};

int main() {

  MIWatcher watcher;
  watcher.Go();
  return 0;
}
