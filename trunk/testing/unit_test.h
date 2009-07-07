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
// Common include file for unit testing.

#ifndef OMAHA_TESTING_UNIT_TEST_H__
#define OMAHA_TESTING_UNIT_TEST_H__

#include <windows.h>
#include <atlstr.h>
#include "base/scoped_ptr.h"
#include "omaha/testing/unittest_debug_helper.h"
#include "omaha/third_party/gtest/include/gtest/gtest.h"

namespace omaha {

// Predicates needed by ASSERT_PRED1 for function returning an HRESULT.
inline testing::AssertionResult Succeeded(const char* s, HRESULT hr) {
  if (SUCCEEDED(hr)) {
    return testing::AssertionSuccess();
  } else {
    CStringA text;
    text.AppendFormat("%s failed with error 0x%08x", s, hr);
    testing::Message msg;
    msg << text;
    return testing::AssertionFailure(msg);
  }
}

inline testing::AssertionResult Failed(const char* s, HRESULT hr) {
  if (FAILED(hr)) {
    return testing::AssertionSuccess();
  } else {
    CStringA text;
    text.AppendFormat("%s failed with error 0x%08x", s, hr);
    testing::Message msg;
    msg << text;
    return testing::AssertionFailure(msg);
  }
}

// Returns whether the tests are running on a Pulse build system.
inline bool IsBuildSystem() {
  TCHAR agent[MAX_PATH] = {0};
  return !!::GetEnvironmentVariable(_T("PULSE_AGENT"), agent, arraysize(agent));
}

// Returns the path to the base local app data directory for the user on the
// current OS.
CString GetLocalAppDataPath();

// Returns the path to the base Google directory for the user on the current OS.
CString GetGoogleUserPath();

// Returns the path to the base Google Update directory for the user on the
// current OS.
CString GetGoogleUpdateUserPath();

// Returns the path to the base Google Update directory for the per-machine
// install on the current OS.
CString GetGoogleUpdateMachinePath();

// Returns a DWORD registry value from the registry. Assumes the value exists.
// Useful for inline comparisons in EXPECT_EQ.
DWORD GetDwordValue(const CString& full_key_name, const CString& value_name);

const TCHAR* const kRegistryHiveOverrideRoot =
    _T("HKCU\\Software\\Google\\Update\\UnitTest\\");
const TCHAR* const kCsidlSystemIdsRegKey =
    _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion");
const TCHAR* const kCsidlProgramFilesRegValue =
    _T("ProgramFilesDir");

// TODO(omaha): consider renaming hive_override_key_name to new_key.
// TODO(omaha): consider making these utility functions, maybe extend the
//               RegKey class.
//
// Overrides the HKLM and HKCU registry hives so that accesses go to the
// specified registry key instead. The function creates the
// hive_override_key_name. In other words, overriding HKCU with
// "HKCU\\Software\\Google\\Update\\UnitTest\\" and accessing HKCU\\Foo results
// in an access at "HKCU\\Software\\Google\\Update\\UnitTest\\Foo".
// This method is most often used in SetUp().
void OverrideRegistryHives(const CString& hive_override_key_name);

// Overrides only the specified hives.
// This is useful when modifying registry settings in one hive while using
// code (e.g. WinHttp) that relies on valid registry entries that are difficult
// to reproduce.
//
// TODO(omaha): Consider renaming to:
// void OverrideRegistryHive(HKEY hive, const CString& new_key);
void OverrideSpecifiedRegistryHives(const CString& hive_override_key_name,
                                    bool override_hklm,
                                    bool override_hkcu);

// Overrides the HKLM and HKCU registry hives so that accesses go to the
// specified registry key instead. Provides permissions to execute local files.
void OverrideRegistryHivesWithExecutionPermissions(
         const CString& hive_override_key_name);

// Restores HKLM and HKCU registry accesses to the real hives.
// This method is most often used in TearDown(). It does not cleanup the
// registry key that is created by OverrideRegistryHives.
void RestoreRegistryHives();

// Sets TestSource=pulse.
void SetBuildSystemTestSource();

// Returns whether large tests should be run. Large tests are always run on the
// build system and if the "OMAHA_RUN_ALL_TESTS" environment variable is set.
bool ShouldRunLargeTest();

// Terminates all processes named GoogleUpdate.exe or GoogleCrashHandler.exe.
void TerminateAllGoogleUpdateProcesses();

// Launches a process and returns its handle.
void LaunchProcess(const CString& cmd_line,
                   const CString& args,
                   bool as_system,
                   HANDLE* process);

// Launches a process as system and returns its handle. The function uses
// psexec to run the process.
void LaunchProcessAsSystem(const CString& launch_cmd, HANDLE* process);

}  // namespace omaha

#define ASSERT_SUCCEEDED(x) ASSERT_PRED_FORMAT1(omaha::Succeeded, x)
#define EXPECT_SUCCEEDED(x) EXPECT_PRED_FORMAT1(omaha::Succeeded, x)
#define ASSERT_FAILED(x) ASSERT_PRED_FORMAT1(omaha::Failed, x)
#define EXPECT_FAILED(x) EXPECT_PRED_FORMAT1(omaha::Failed, x)

#define kUnittestName _T("omaha_unittest.exe")

#endif  // OMAHA_TESTING_UNIT_TEST_H__

