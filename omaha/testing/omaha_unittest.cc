// Copyright 2007-2010 Google Inc.
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

#include "omaha/testing/omaha_unittest.h"
#include <atlpath.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

//
// Omaha-specific arguments.
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
    SafeCStringAppendFormat(&command_line, _T(" %s"), argv[i]);
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
      SafeCStringAppendFormat(&environment_variables, _T("\t%s\r\n"), current);
    }
    current += sub_length;
  }

  OPT_LOG(L3, (_T("[Omaha unit test environment][\r\n%s]"),
               environment_variables));
}

// Sets values based on environment variables.
void ProcessEnvironmentVariables() {
  if (IsEnvironmentVariableSet(_T("OMAHA_TEST_BUILD_SYSTEM"))) {
    SetIsBuildSystem();
  }

  TCHAR psexec_dir[MAX_PATH] = {0};
  if (::GetEnvironmentVariable(_T("OMAHA_PSEXEC_DIR"),
                                psexec_dir,
                                arraysize(psexec_dir))) {
    SetPsexecDir(psexec_dir);
  }
}

bool ParseOmahaArgPsexecDir(const CString& arg) {
  CString psexec_dir_arg_begin;
  SafeCStringAppendFormat(&psexec_dir_arg_begin, _T("%s="), kOmahaArgPsexecDir);

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

  _tprintf(_T("ERROR: Invalid Command line: %s\n"), argv[1]);
  _tprintf(_T("  First invalid command option: %s\n\n"), argv[1]);
  _tprintf(_T("Use --help command line to see what flags can be used to ")
           _T("control test selection, execution, output, and assertion ")
           _T("behavior.\n"));

  return false;
}

}  // namespace

// If a test launches or checks other processes, uses shared resources, or uses
// the network, it is a medium or larger test.
// COM is always initialized.
int RunTests(bool is_medium_or_large_test,
             bool load_resources,
             int argc,
             TCHAR** argv) {
  ASSERT1(!is_medium_or_large_test || load_resources);

  // TODO(omaha): Add executable name.
  OPT_LOG(L1, (_T("[Starting Omaha unit tests]")));
  LogCommandLineAndEnvironment(argc, argv);

  // Process the environment variables before the args to allow the args to take
  // precedence.
  ProcessEnvironmentVariables();

  if (!ParseUnitTestArgs(argc, argv)) {
    return -1;
  }
  FailOnAssert fail_on_assert;

  InitializeShellVersion();
  InitializeVersionFromModule(NULL);

  scoped_co_init co_init(COINIT_MULTITHREADED);
  VERIFY_SUCCEEDED(co_init.hresult());

  const bool is_build_system = IsBuildSystem();

  if (is_build_system) {
    // Some tests only run as admin. We want to know if the build system is no
    // longer running unit tests as admin.
    if (!vista_util::IsUserAdmin()) {
      _tprintf(_T("\nUser is not an admin. All tests may not run.\n"));
    }

    // TODO(omaha): Remove this and the app_util.h, file.h, and atlpath.h
    // includes once the test system does this for us.
    const TCHAR* const kDllRequiredForCoverageRuns = _T("VSCover80.dll");
    CPath source_path(app_util::GetCurrentModuleDirectory());
    source_path.Append(kDllRequiredForCoverageRuns);
    if (File::Exists(source_path)) {
      CPath target_path(app_util::GetSystemDir());
      target_path.Append(kDllRequiredForCoverageRuns);
      _tprintf(_T("\nCopying '%s' to '%s'.\n"),
               static_cast<const TCHAR*>(source_path),
               static_cast<const TCHAR*>(target_path));
      VERIFY_SUCCEEDED(File::Copy(source_path, target_path, false));
    }
  }

  if (is_medium_or_large_test) {
    TerminateAllGoogleUpdateProcesses();
  }

  int result = InitializeNetwork();
  if (result) {
    return result;
  }

  if (load_resources) {
    // Load a resource DLL so that strings can be loaded during tests and add it
    // to the list of modules used for CString.LoadString and CreateDialog
    // calls. The unittest executable includes unittest-specific resources.
    HMODULE resource_dll = ::LoadLibraryEx(_T("goopdateres_en.dll"),
                            NULL,
                            LOAD_LIBRARY_AS_DATAFILE);
    ASSERT1(resource_dll);
    _AtlBaseModule.AddResourceInstance(resource_dll);
  }

  // A COM module is required to create COM objects.
  // Create it regardless of whether COM is actually used by this executable.
  CComModule module;

  // Add an event listener. Google Test takes the ownership.
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestLogger);

  result = RUN_ALL_TESTS();

  DeinitializeNetwork();

  if (is_build_system && is_medium_or_large_test) {
    TerminateAllGoogleUpdateProcesses();
  }

  return result;
}

int g_assert_count = 0;

}  // namespace omaha
