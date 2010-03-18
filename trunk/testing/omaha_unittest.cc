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

#include <windows.h>
#include <atlpath.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/app_util.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/net/network_config.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

//
// Subset of Google Test arguments.
//
const TCHAR* const kUnitTestBreakOnFailure   = _T("--gtest_break_on_failure");
const TCHAR* const kUnitTestFilter           = _T("--gtest_filter");
const TCHAR* const kUnitTestListTests        = _T("--gtest_list_tests");

//
// Omaha-specifc arguments.
//
const TCHAR* const kOmahaArgIsBuildSystem    = _T("--omaha_buildsystem");
const TCHAR* const kOmahaArgPsexecDir        = _T("--omaha_psexec_dir");
// Only use kOmahaArgAcceptPsexecEula for automated testing when you have
// already read and agreed to the EULA terms.
// If present both are present, kOmahaArgAcceptPsexecEula must appear after
// kOmahaArgPsexecDir on the command line.
const TCHAR* const kOmahaArgAcceptPsexecEula = _T("--omaha_accept_psexec_eula");

// Logs the start, end, and failures of each test in the Omaha log.
// TODO(omaha): Consider adding a logging category for tests.
class TestLogger : public ::testing::EmptyTestEventListener {
  // Called before a test starts.
  virtual void OnTestStart(const ::testing::TestInfo& test_info) {
    OPT_LOG(L3, (_T("*** TEST %s.%s starting."),
                 CString(CA2W(test_info.test_case_name())),
                 CString(CA2W(test_info.name()))));
  }

  // Called after a failed assertion or a SUCCESS().
  virtual void OnTestPartResult(
      const ::testing::TestPartResult& test_part_result) {
    OPT_LOG(L3, (_T("%s in %s:%d\n%s"),
                 (test_part_result.failed() ? _T("*** TEST Failure") :
                                              _T("TEST Success")),
                 CString(CA2W(test_part_result.file_name())),
                 test_part_result.line_number(),
                 CString(CA2W(test_part_result.summary()))));
  }

  // Called after a test ends.
  virtual void OnTestEnd(const ::testing::TestInfo& test_info) {
    OPT_LOG(L3, (_T("*** TEST %s.%s ending."),
                 CString(CA2W(test_info.test_case_name())),
                 CString(CA2W(test_info.name()))));
  }
};

void LogCommandLineAndEnvironment(int argc, TCHAR** argv) {
  ASSERT1(1 <= argc);
  ASSERT1(argv);

  CString command_line = argv[0];
  for (int i = 1; i < argc; ++i) {
    command_line.AppendFormat(_T(" %s"), argv[i]);
  }
  OPT_LOG(L1, (_T("[Omaha unit test command line][%s]"), command_line));

  TCHAR* env_vars = ::GetEnvironmentStrings();
  if (env_vars == NULL) {
    ASSERT1(false);
    return;
  }

  // Iterate through the environment variables string. The format of the string
  // is Name1=Value1\0Name2=Value2\0Name3=Value3\0\0.
  const TCHAR* const kPartialMatchToIgnore = _T("PASSW");
  const TCHAR* current = env_vars;
  CString environment_variables;
  while (*current) {
    size_t sub_length = _tcslen(current) + 1;
    if (!_tcsstr(current, kPartialMatchToIgnore)) {
      environment_variables.AppendFormat(_T("\t%s\r\n"), current);
    }
    current += sub_length;
  }

  OPT_LOG(L3, (_T("[Omaha unit test environment][\r\n%s]"),
               environment_variables));
}

bool ParseOmahaArgPsexecDir(const CString& arg) {
  CString psexec_dir_arg_begin;
  psexec_dir_arg_begin.Format(_T("%s="), kOmahaArgPsexecDir);

  if (arg.Left(psexec_dir_arg_begin.GetLength()) != psexec_dir_arg_begin) {
    return false;
  }

  SetPsexecDir(arg.Mid(psexec_dir_arg_begin.GetLength()));
  return true;
}

// Must be called after ParseOmahaArgPsexecDir().
bool ParseOmahaArgAcceptPsexecEula(const TCHAR* arg) {
  ASSERT1(arg);
  if (_tcsicmp(arg, kOmahaArgAcceptPsexecEula)) {
    return false;
  }

  EXPECT_TRUE(AcceptPsexecEula())
        << _T("Make sure '") << kOmahaArgPsexecDir << _T("' appears after '")
        << kOmahaArgAcceptPsexecEula << _T("' on the command line and that ")
        << _T("psexec.exe is in the specified location.");
  return true;
}

bool ParseOmahaArgIsBuildMachine(const TCHAR* arg) {
  ASSERT1(arg);
  if (_tcsicmp(arg, kOmahaArgIsBuildSystem)) {
    return false;
  }

  SetIsBuildSystem();
  return true;
}

// Parse args. Print help message if invalid arguments.
bool ParseUnitTestArgs(int argc, TCHAR** argv) {
  testing::InitGoogleTest(&argc, argv);

  if (argc > 1) {
    // One or more args were unparsed by the Google Test parser. Handle
    // Omaha-specific arguments that may be present. Code is based on
    // ParseGoogleTestFlagsOnlyImpl.
    for (int i = 1; i < argc; i++) {
      if (ParseOmahaArgPsexecDir(argv[i]) ||
          ParseOmahaArgAcceptPsexecEula(argv[i]) ||
          ParseOmahaArgIsBuildMachine(argv[i])) {
        // Yes.  Shift the remainder of the argv list left by one.  Note
        // that argv has (*argc + 1) elements, the last one always being
        // NULL.  The following loop moves the trailing NULL element as
        // well.
        for (int j = i; j != argc; j++) {
          argv[j] = argv[j + 1];
        }

        // Decrements the argument count.
        argc--;

        // We also need to decrement the iterator as we just removed
        // an element.
        i--;
      }
    }
  }

  if (argc <= 1) {
    return true;
  }

  _tprintf(_T("ERROR: Invalid Command line!\n"), argv[1]);
  _tprintf(_T("  First invalid command option: %s\n\n"), argv[1]);
  _tprintf(_T("Valid options:\n"));
  _tprintf(_T("%25s   Cause an av when a test fails (for use with debugger)\n"),
           kUnitTestBreakOnFailure);
  _tprintf(_T("%25s   Sets a filter on the unit tests.\n")
           _T("%25s   Format: %s=Filter[:Filter] where\n")
           _T("%25s   Filter is TestCase[.Test] and * is a wildcard.\n"),
           kUnitTestFilter, _T(""), kUnitTestFilter, _T(""));
  _tprintf(_T("%25s   Lists all tests\n"),
           kUnitTestListTests);
  return false;
}

int RunTests(int argc, TCHAR** argv) {
  OPT_LOG(L1, (_T("[Starting Omaha unit tests]")));
  LogCommandLineAndEnvironment(argc, argv);

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
    if (!vista_util::IsUserAdmin()) {
      _tprintf(_T("\nUser is not an admin. All tests may not run.\n"));
    }

    // TODO(omaha): Once tests are broken up into small, medium, and large,
    // and/or support running as non-admin on the build system, only call this
    // when running large tests and/or set this outside the tests.
    SetBuildSystemTestSource();

    // TODO(omaha): Remove this and the app_util.h, file.h, and atlpath.h
    // includes once the test system does this for us.
    const TCHAR* const kDllRequiredForCoverageRuns = _T("VSCover80.dll");
    CPath source_path(app_util::GetCurrentModuleDirectory());
    source_path.Append(kDllRequiredForCoverageRuns);
    if (File::Exists(source_path)) {
      CPath target_path(app_util::GetSystemDir());
      target_path.Append(kDllRequiredForCoverageRuns);
      _tprintf(_T("\nCopying '%s' to '%s'.\n"), source_path, target_path);
      VERIFY1(SUCCEEDED(File::Copy(source_path, target_path, false)));
    }
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

  // Add an event listener. Google Test takes the ownership.
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestLogger);

  int result = RUN_ALL_TESTS();

  NetworkConfig::DeleteInstance();

  if (is_build_system) {
    TerminateAllGoogleUpdateProcesses();
  }

  return result;
}

}  // namespace

int g_assert_count = 0;

}  // namespace omaha

int _tmain(int argc, TCHAR** argv) {
  return omaha::RunTests(argc, argv);
}
