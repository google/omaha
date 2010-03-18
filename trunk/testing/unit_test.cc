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

#include "testing/unit_test.h"
#include "omaha/common/app_util.h"
#include "omaha/common/constants.h"
#include "omaha/common/path.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/system.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/const_goopdate.h"

namespace omaha {

namespace {

static bool is_buildsystem = false;
static TCHAR psexec_dir[MAX_PATH] = {0};

// Returns whether all unit tests should be run.
bool ShouldRunAllTests() {
  if (is_buildsystem) {
    return true;
  }

  TCHAR var[100] = {0};
  DWORD res = ::GetEnvironmentVariable(_T("OMAHA_RUN_ALL_TESTS"),
                                       var,
                                       arraysize(var));
  if (0 == res) {
    ASSERT1(ERROR_ENVVAR_NOT_FOUND == ::GetLastError());
    return false;
  } else {
    return true;
  }
}

}  // namespace

CString GetLocalAppDataPath() {
  CString expected_local_app_data_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_LOCAL_APPDATA | CSIDL_FLAG_DONT_VERIFY,
                                 &expected_local_app_data_path));
  expected_local_app_data_path.Append(_T("\\"));
  return expected_local_app_data_path;
}

CString GetGoogleUserPath() {
  return GetLocalAppDataPath() + _T("Google\\");
}

CString GetGoogleUpdateUserPath() {
  return GetGoogleUserPath() + _T("Update\\");
}

CString GetGoogleUpdateMachinePath() {
  CString program_files;
  GetFolderPath(CSIDL_PROGRAM_FILES, &program_files);
  return program_files + _T("\\Google\\Update");
}

DWORD GetDwordValue(const CString& full_key_name, const CString& value_name) {
  DWORD value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(full_key_name, value_name, &value));
  return value;
}

void OverrideRegistryHives(const CString& hive_override_key_name) {
  OverrideSpecifiedRegistryHives(hive_override_key_name, true, true);
}

void OverrideSpecifiedRegistryHives(const CString& hive_override_key_name,
                                    bool override_hklm,
                                    bool override_hkcu) {
  // Override the destinations of HKLM and HKCU to use a special location
  // for the unit tests so that we don't disturb the actual Omaha state.
  RegKey machine_key;
  RegKey user_key;
  ASSERT_SUCCEEDED(machine_key.Create(hive_override_key_name + MACHINE_KEY));
  ASSERT_SUCCEEDED(user_key.Create(hive_override_key_name + USER_KEY));
  if (override_hklm) {
    ASSERT_SUCCEEDED(::RegOverridePredefKey(HKEY_LOCAL_MACHINE,
                                            machine_key.Key()));
  }
  if (override_hkcu) {
    ASSERT_SUCCEEDED(::RegOverridePredefKey(HKEY_CURRENT_USER,
                                            user_key.Key()));
  }
}

// When tests execute programs (i.e. with ShellExecute or indirectly), Windows
// looks at kMyComputerSecurityZoneKeyPathL:kMiscSecurityZonesValueName
// to see if it should run the program.
// Normally, these reads are not redirected to the override key even though
// it seems like they should be. In this case, the execution succeeds.
// In certain cases, the reads are redirected and the program execution
// fails due to permission denied errors.
// This has been observed when XmlUtilsTest::LoadSave() is run before such
// tests. Specifically, the my_xmldoc->load() call in LoadXMLFromFile()
// appears to somehow cause the redirection to occur.
//
// See http://support.microsoft.com/kb/182569 for information on security
// zones.
void OverrideRegistryHivesWithExecutionPermissions(
         const CString& hive_override_key_name) {
  OverrideRegistryHives(hive_override_key_name);

  const TCHAR kMyComputerSecurityZoneKeyPath[] =
      _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\")
      _T("Internet Settings\\Zones\\0");
  const TCHAR kMiscSecurityZonesValueName[] = _T("1806");
  const DWORD kPermitAction = 0;

  RegKey my_computer_zone_key;
  ASSERT_SUCCEEDED(my_computer_zone_key.Create(
      kMyComputerSecurityZoneKeyPath));
  ASSERT_SUCCEEDED(my_computer_zone_key.SetValue(kMiscSecurityZonesValueName,
                                                 kPermitAction));
}

void RestoreRegistryHives() {
  ASSERT_SUCCEEDED(::RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL));
  ASSERT_SUCCEEDED(::RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
}

void SetPsexecDir(const CString& dir) {
  _tcscpy_s(psexec_dir, arraysize(psexec_dir), dir.GetString());
}

// If psexec_dir is not already set - by SetPsexecDir or a previous call to this
// method - read the environment variable.
CString GetPsexecDir() {
  if (!_tcsnlen(psexec_dir, arraysize(psexec_dir))) {
    EXPECT_TRUE(::GetEnvironmentVariable(_T("OMAHA_PSEXEC_DIR"),
                                         psexec_dir,
                                         arraysize(psexec_dir)));
  }

  return psexec_dir;
}

// Must be called after SetPsexecDir().
// Does not wait for the EULA accepting process to complete.
bool AcceptPsexecEula() {
  CString psexec_dir = GetPsexecDir();
  if (psexec_dir.IsEmpty()) {
    return false;
  }

  CString psexec_path = ConcatenatePath(psexec_dir, _T("psexec.exe"));
  return SUCCEEDED(System::StartProcessWithArgs(psexec_path,
                                                _T("/accepteula")));
}

void SetIsBuildSystem() {
  is_buildsystem = true;
}

bool IsBuildSystem() {
  return is_buildsystem;
}

void SetBuildSystemTestSource() {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueTestSource,
                                    _T("buildsystem")));
}

bool ShouldRunLargeTest() {
  if (ShouldRunAllTests()) {
    return true;
  }

  TCHAR var[100] = {0};
  DWORD res = ::GetEnvironmentVariable(_T("OMAHA_RUN_LARGE_TESTS"),
                                       var,
                                       arraysize(var));
  if (0 == res) {
    ASSERT1(ERROR_ENVVAR_NOT_FOUND == ::GetLastError());
    std::wcout << _T("\tThis large test did not run because neither ")
                  _T("'OMAHA_RUN_LARGE_TESTS' or 'OMAHA_RUN_ALL_TESTS' is set ")
                  _T("in the environment.") << std::endl;
    return false;
  } else {
    return true;
  }
}

bool ShouldRunEnormousTest() {
  if (ShouldRunAllTests()) {
    return true;
  }

  std::wcout << _T("\tThis large test did not run because ")
              _T("'OMAHA_RUN_ALL_TESTS' is not set in the environment.")
           << std::endl;
  return false;
}

void TerminateAllProcessesByName(const TCHAR* process_name) {
  std::vector<uint32> process_pids;
  ASSERT_SUCCEEDED(Process::FindProcesses(0,  // No flags.
                                          process_name,
                                          true,
                                          &process_pids));

  for (size_t i = 0; i < process_pids.size(); ++i) {
    scoped_process process(::OpenProcess(PROCESS_TERMINATE,
                                         FALSE,
                                         process_pids[i]));
    EXPECT_TRUE(process);
    EXPECT_TRUE(::TerminateProcess(get(process), static_cast<uint32>(-3)));
  }
}

void TerminateAllGoogleUpdateProcesses() {
  TerminateAllProcessesByName(kGoopdateFileName);
  TerminateAllProcessesByName(kGoopdateCrashHandlerFileName);
}

// The exit code of psexec is the pid it started when -d is used.
// Wait for psexec to exit, get the exit code, and use it to get a handle
// to the GoogleUpdate.exe instance.
void LaunchProcessAsSystem(const CString& launch_cmd, HANDLE* process) {
  ASSERT_TRUE(process);

  CString app_launcher = ConcatenatePath(GetPsexecDir(), _T("psexec.exe"));
  CString cmd_line_args;
  cmd_line_args.Format(_T("-s -d %s"), launch_cmd);

  PROCESS_INFORMATION pi = {0};
  EXPECT_SUCCEEDED(System::StartProcessWithArgsAndInfo(app_launcher,
                                                       cmd_line_args,
                                                       &pi));
  ::CloseHandle(pi.hThread);
  scoped_handle started_process(pi.hProcess);
  ASSERT_TRUE(started_process);

  DWORD google_update_pid = 0;
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(started_process), 30000));
  EXPECT_TRUE(::GetExitCodeProcess(get(started_process), &google_update_pid));
  DWORD desired_access =
      PROCESS_QUERY_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE;
  *process = ::OpenProcess(desired_access, false, google_update_pid);
  DWORD last_error(::GetLastError());

  // psexec sometimes returns errors instead of PIDs and there is no way to
  // tell the difference. We see ERROR_SERVICE_MARKED_FOR_DELETE (1072)
  // intermittently on the build server, but do not expect any other errors.
  EXPECT_TRUE(*process ||
              ERROR_SERVICE_MARKED_FOR_DELETE == google_update_pid) <<
      _T("::OpenProcess failed in a case where psexec did not return ")
      _T("ERROR_SERVICE_MARKED_FOR_DELETE.") << _T(" The error was ")
      << last_error << _T(".");
}

void LaunchProcess(const CString& cmd_line,
                   const CString& args,
                   bool as_system,
                   HANDLE* process) {
  ASSERT_TRUE(process);
  *process = NULL;

  CString launch_cmd = cmd_line + (args.IsEmpty() ? _T("") : _T(" ") + args);

  if (as_system) {
    // Retry the process launch if the process handle is invalid. Hopefully this
    // is robust against intermittent ERROR_SERVICE_MARKED_FOR_DELETE errors.
    for (int tries = 0; tries < 10 && !*process; ++tries) {
      LaunchProcessAsSystem(launch_cmd, process);
      if (!*process) {
        ::Sleep(1000);
      }
    }
  } else {
    PROCESS_INFORMATION pi = {0};
    EXPECT_SUCCEEDED(System::StartProcess(NULL, launch_cmd.GetBuffer(), &pi));
    ::CloseHandle(pi.hThread);
    *process = pi.hProcess;
  }

  ASSERT_TRUE(*process);
}

//
// Unit tests for helper functions in this file.
//

TEST(UnitTestHelpersTest, GetLocalAppDataPath) {
  const TCHAR kUserXpLocalAppDataPathFormat[] =
      _T("C:\\Documents and Settings\\%s\\Local Settings\\Application Data\\");
  const TCHAR kUserVistaLocalAppDataPathFormat[] =
      _T("C:\\Users\\%s\\AppData\\Local\\");

  TCHAR username[MAX_PATH] = {0};
  EXPECT_TRUE(::GetEnvironmentVariable(_T("USERNAME"),
                                       username,
                                       arraysize(username)));
  CString expected_path;
  expected_path.Format(vista_util::IsVistaOrLater() ?
                           kUserVistaLocalAppDataPathFormat :
                           kUserXpLocalAppDataPathFormat,
                       username);
  EXPECT_STREQ(expected_path, GetLocalAppDataPath());
}

}  // namespace omaha
