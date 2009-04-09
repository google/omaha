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
//
// The entry point for the omaha unit tests.

#include <vector>
#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/net/network_config.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

#define kUnitTestFilter _T("--gtest_filter")
#define kUnitTestBreakOnFailure _T("--gtest_break_on_failure")

int g_assert_count = 0;

void ListTests() {
  TCHAR* list_tests[3] = {
    _T(""),
    _T("--gtest_list_tests"),
    NULL,
  };
  int list_test_argc = arraysize(list_tests) - 1;

  testing::InitGoogleTest(&list_test_argc, list_tests);
  RUN_ALL_TESTS();
}

// Parse args. Print help message if invalid arguments.
bool ParseUnitTestArgs(int argc, TCHAR** argv) {
  testing::InitGoogleTest(&argc, argv);

  if (argc <= 1) {
    return true;
  }

  // Parse the arguments
  if (argc > 1) {
    _tprintf(_T("\nTest Cases:\n"));
    ListTests();

    _tprintf(_T("First invalid command option: %s\n\n"), argv[1]);
    _tprintf(_T("Valid options:\n"));
    _tprintf(_T("%25s Cause an av when a test fails (for use with debugger)\n"),
             kUnitTestBreakOnFailure);
    _tprintf(_T("%25s Sets a filter on the unit tests.\n")
             _T("%25s Format: %s=Filter[:Filter] where\n")
             _T("%25s Filter is TestCase[.Test] and * is a wildcard.\n"),
             kUnitTestFilter, _T(""), kUnitTestFilter, _T(""));
    return false;
  }

  return true;
}

int RunTests(int argc, TCHAR** argv) {
  InitializeVersionFromModule(NULL);
  if (!ParseUnitTestArgs(argc, argv)) {
    return -1;
  }
  FailOnAssert fail_on_assert;

  scoped_co_init co_init(COINIT_MULTITHREADED);
  VERIFY1(SUCCEEDED(co_init.hresult()));

  const bool is_build_system = IsBuildSystem();
  if (is_build_system) {
    // Some tests only run as admin. We want to know if the build system is no
    // longer running unit tests as admin.
    ASSERT1(vista_util::IsUserAdmin());

    SetBuildSystemTestSource();
  }
  TerminateAllGoogleUpdateProcesses();

  // Ensure that any system running unittests has testsource set.
  // Some unit tests generate pings, and these must be filtered.
  CString value;
  HRESULT hr =
      RegKey::GetValue(MACHINE_REG_UPDATE_DEV, kRegValueTestSource, &value);
  if (FAILED(hr) || value.IsEmpty()) {
    ADD_FAILURE() << _T("'") << kRegValueTestSource << _T("'")
                  << _T(" is not present in ")
                  << _T("'") << MACHINE_REG_UPDATE_DEV << _T("'")
                  << _T(" or it is empty. Since you are running Omaha unit ")
                  << _T("tests, it should probably be set to 'dev' or 'qa'.");
    return -1;
  }

  // Many unit tests require the network configuration be initialized.
  // On Windows Vista only admins can write to HKLM therefore the
  // initialization of the NetworkConfig must correspond to the integrity
  // level the user is running as.
  bool is_machine = vista_util::IsUserAdmin();
  VERIFY1(SUCCEEDED(goopdate_utils::ConfigureNetwork(is_machine, false)));

  int result = RUN_ALL_TESTS();

  NetworkConfig::DeleteInstance();

  if (is_build_system) {
    TerminateAllGoogleUpdateProcesses();
  }

  return result;
}

}  // namespace omaha

int _tmain(int argc, TCHAR** argv) {
  return omaha::RunTests(argc, argv);
}

