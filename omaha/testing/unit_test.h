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
//
// Common include file for unit testing.

#ifndef OMAHA_TESTING_UNIT_TEST_H_
#define OMAHA_TESTING_UNIT_TEST_H_

#include <windows.h>
#include <atlstr.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "omaha/testing/unittest_debug_helper.h"

namespace omaha {

const TCHAR* const kUnittestName = _T("omaha_unittest.exe");

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

// Returns true if the variable exists in the environment, even if it is "0".
bool IsEnvironmentVariableSet(const TCHAR* name);

// Returns true if current unit test process owner is LOCALSYSTEM.
bool IsTestRunByLocalSystem();

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

// Returns a SZ registry value from the registry. Assumes the value exists.
// Useful for inline comparisons in EXPECT_STREQ.
CString GetSzValue(const CString& full_key_name, const CString& value_name);

// Converts string to GUID. Assumes the string is a valid GUID.
GUID StringToGuid(const CString& str);

const TCHAR* const kRegistryHiveOverrideRoot =
    _T("HKCU\\Software\\") _T(PATH_COMPANY_NAME_ANSI)
    _T("\\") _T(PRODUCT_NAME_ANSI)
    _T("\\UnitTest\\");
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

// Specifies the location of psexec.exe. Only call during initialization.
void SetPsexecDir(const CString& dir);

// Returns the location of psexec.exe.
CString GetPsexecDir();

// Accepts the psexec.exe EULA. Only use for automated testing when you have
// already read and agreed to the EULA terms.
// Returns true if the process was successfully started.
bool AcceptPsexecEula();

// Specifies that the tests are running on or on behalf of the build system.
void SetIsBuildSystem();

// Returns whether tests are running on or on behalf of the build system.
bool IsBuildSystem();

// Sets TestSource=buildsystem.
void SetBuildSystemTestSource();

// Terminates all processes named GoogleUpdate.exe or GoogleCrashHandler.exe.
void TerminateAllGoogleUpdateProcesses();

// Launches a process and returns its handle.
void LaunchProcess(const CString& exe_path,
                   const CString& args,
                   bool as_system,
                   HANDLE* process);

// Launches a process as system and returns its handle. The function uses
// psexec to run the process.
void LaunchProcessAsSystem(const CString& launch_cmd, HANDLE* process);

// Copies Omaha installation files under omaha_path.
void CopyGoopdateFiles(const CString& omaha_path, const CString& version);

// A generic test fixture that overrides the HKLM and HKCU hives.
class RegistryProtectedTest : public testing::Test {
 protected:
  RegistryProtectedTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  virtual void SetUp();
  virtual void TearDown();

  const CString hive_override_key_name_;
};

// Returns the full path of a unique directory under the user temp directory.
CString GetUniqueTempDirectoryName();

// Runs the command as an administrator.
void RunAsAdmin(const CString& exe_path, const CString& cmd_line);

void RegisterOrUnregisterGoopdateLocalServer(bool reg);

void RegisterOrUnregisterGoopdateService(bool reg);

struct FileStruct {
  const TCHAR* const filename;
  const DWORD file_attributes;
};

void CreateDirs(const TCHAR* parent_dir,
                const TCHAR* const directories[],
                size_t number_of_directories);
void CreateFiles(const TCHAR* parent_dir,
                 const FileStruct files[],
                 size_t number_of_files);

HRESULT SetPolicy(const TCHAR* policy_name, DWORD value);
HRESULT SetPolicyString(const TCHAR* policy_name, const CString& value);

// Deletes the group policy registry keys for Omaha and reloads the
// ConfigManager policies to reset them.
void ClearGroupPolicies();

}  // namespace omaha

// TODO(omaha): Replace custom predicates with EXPECT_HRESULT_SUCCEEDED/FAILED.
#define ASSERT_SUCCEEDED(x) ASSERT_PRED_FORMAT1(omaha::Succeeded, x)
#define EXPECT_SUCCEEDED(x) EXPECT_PRED_FORMAT1(omaha::Succeeded, x)
#define ASSERT_FAILED(x) ASSERT_PRED_FORMAT1(omaha::Failed, x)
#define EXPECT_FAILED(x) EXPECT_PRED_FORMAT1(omaha::Failed, x)

// As of Google Test 1.4.0, expressions get converted to 'bool', resulting in
// "warning C4800: 'BOOL' : forcing value to bool 'true' or 'false' (performance
// warning)" in some uses.
// These must be kept in sync with gtest.h.
// TODO(omaha): Try to get this fixed in Google Test.
#undef EXPECT_TRUE
#define EXPECT_TRUE(condition) \
  GTEST_TEST_BOOLEAN_(!!(condition), #condition, false, true, \
                      GTEST_NONFATAL_FAILURE_)
#undef ASSERT_TRUE
#define ASSERT_TRUE(condition) \
  GTEST_TEST_BOOLEAN_(!!(condition), #condition, false, true, \
                      GTEST_FATAL_FAILURE_)

// GMock's ACTION* macros have 10 parameters, most of which go unused.
// This macro can be used inside ACTION* definitions to suppress warning
// C4100: unreferenced formal parameter.
#define UNREFERENCED_ACTION_PARAMETERS \
    UNREFERENCED_PARAMETER(args); \
    UNREFERENCED_PARAMETER(arg0); \
    UNREFERENCED_PARAMETER(arg1); \
    UNREFERENCED_PARAMETER(arg2); \
    UNREFERENCED_PARAMETER(arg3); \
    UNREFERENCED_PARAMETER(arg4); \
    UNREFERENCED_PARAMETER(arg5); \
    UNREFERENCED_PARAMETER(arg6); \
    UNREFERENCED_PARAMETER(arg7); \
    UNREFERENCED_PARAMETER(arg8); \
    UNREFERENCED_PARAMETER(arg9)

// Teach Google Test how to print a CString.
std::ostream& operator<<(std::ostream& os, const CString& str);

#endif  // OMAHA_TESTING_UNIT_TEST_H_
