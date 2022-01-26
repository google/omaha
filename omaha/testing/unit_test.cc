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

#include "testing/unit_test.h"

#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

static bool is_buildsystem = false;
static TCHAR psexec_dir[MAX_PATH] = {0};

}  // namespace

bool IsEnvironmentVariableSet(const TCHAR* name) {
  ASSERT1(name);
  TCHAR var[100] = {0};
  DWORD res = ::GetEnvironmentVariable(name, var, arraysize(var));
  if (0 == res) {
    ASSERT1(ERROR_ENVVAR_NOT_FOUND == ::GetLastError());
    return false;
  } else {
    return true;
  }
}

bool IsTestRunByLocalSystem() {
  return user_info::IsRunningAsSystem();
}

CString GetLocalAppDataPath() {
  CString expected_local_app_data_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_LOCAL_APPDATA | CSIDL_FLAG_DONT_VERIFY,
                                 &expected_local_app_data_path));
  expected_local_app_data_path.Append(_T("\\"));
  return expected_local_app_data_path;
}

CString GetGoogleUserPath() {
  return GetLocalAppDataPath() + PATH_COMPANY_NAME + _T("\\");
}

// TODO(omaha): make GetGoogleUpdateUserPath and GetGoogleUpdateMachinePath
// consistent. They should end with \ or not.
CString GetGoogleUpdateUserPath() {
  return GetGoogleUserPath() + PRODUCT_NAME + _T("\\");
}

CString GetGoogleUpdateMachinePath() {
  CString program_files;
  GetFolderPath(CSIDL_PROGRAM_FILES, &program_files);
  return program_files + _T("\\") + PATH_COMPANY_NAME
                        + _T("\\") + PRODUCT_NAME;
}

DWORD GetDwordValue(const CString& full_key_name, const CString& value_name) {
  DWORD value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(full_key_name, value_name, &value));
  return value;
}

CString GetSzValue(const CString& full_key_name, const CString& value_name) {
  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(full_key_name, value_name, &value));
  return value;
}

GUID StringToGuid(const CString& str) {
  GUID guid(GUID_NULL);
  VERIFY(SUCCEEDED(StringToGuidSafe(str, &guid)), (_T("guid '%s'"), str));
  return guid;
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

CString GetPsexecDir() {
  EXPECT_TRUE(_tcsnlen(psexec_dir, arraysize(psexec_dir)));
  return psexec_dir;
}

// Must be called after SetPsexecDir().
// Does not wait for the EULA accepting process to complete.
bool AcceptPsexecEula() {
  CString psexec_directory = GetPsexecDir();
  if (psexec_directory.IsEmpty()) {
    return false;
  }

  psexec_directory = String_MakeEndWith(psexec_directory, _T("\\"), true);

  CString psexec_path = ConcatenatePath(psexec_directory, _T("psexec.exe"));
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
    BOOL terminate_process_result(::TerminateProcess(get(process),
                                                     static_cast<uint32>(-3)));
    HRESULT hr(S_OK);
    if (!terminate_process_result) {
      hr = HRESULTFromLastError();
      std::wcout << _T("\t::TerminateProcess failed " << hr) << std::endl;
    }
  }
}

void TerminateAllGoogleUpdateProcesses() {
  TerminateAllProcessesByName(kOmahaShellFileName);
  TerminateAllProcessesByName(kCrashHandlerFileName);
  TerminateAllProcessesByName(kCrashHandler64FileName);
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

void LaunchProcess(const CString& exe_path,
                   const CString& args,
                   bool as_system,
                   HANDLE* process) {
  ASSERT_TRUE(process);
  *process = NULL;

  CString launch_cmd = exe_path;
  EnclosePath(&launch_cmd);
  if (!args.IsEmpty()) {
    launch_cmd += CString(_T(" ")) + args;
  }

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

void RegistryProtectedTest::SetUp() {
  RegKey::DeleteKey(hive_override_key_name_, true);
  OverrideRegistryHives(hive_override_key_name_);
}

void RegistryProtectedTest::TearDown() {
  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
}

CString GetUniqueTempDirectoryName() {
  CString guid;
  EXPECT_HRESULT_SUCCEEDED(GetGuid(&guid));
  return ConcatenatePath(app_util::GetTempDir(), guid);
}

void RunAsAdmin(const CString& exe_path, const CString& cmd_line) {
  if (vista_util::IsUserAdmin()) {
    EXPECT_SUCCEEDED(RegisterOrUnregisterExe(exe_path, cmd_line));
    return;
  }

  // Elevate for medium integrity users on Vista and above.
  DWORD exit_code(S_OK);
  EXPECT_SUCCEEDED(vista_util::RunElevated(exe_path,
                                           cmd_line,
                                           SW_SHOWNORMAL,
                                           &exit_code));
  EXPECT_SUCCEEDED(exit_code);
}

void RegisterOrUnregisterGoopdateLocalServer(bool reg) {
  CString server_path = ConcatenatePath(GetGoogleUpdateMachinePath(),
                                        kOmahaShellFileName);
  EnclosePath(&server_path);

  CommandLineBuilder builder(reg ? COMMANDLINE_MODE_REGSERVER :
                                   COMMANDLINE_MODE_UNREGSERVER);
  CString cmd_line = builder.GetCommandLineArgs();
  RunAsAdmin(server_path, cmd_line);
}

void RegisterOrUnregisterGoopdateService(bool reg) {
  CString service_path = ConcatenatePath(GetGoogleUpdateMachinePath(),
                                         kServiceFileName);
  EnclosePath(&service_path);

  CommandLineBuilder builder(reg ? COMMANDLINE_MODE_SERVICE_REGISTER :
                                   COMMANDLINE_MODE_SERVICE_UNREGISTER);
  CString cmd_line = builder.GetCommandLineArgs();
  RunAsAdmin(service_path, cmd_line);
}

void CreateDirs(const TCHAR* parent_dir,
                const TCHAR* const directories[],
                size_t number_of_directories) {
  for (size_t i = 0; i != number_of_directories; ++i) {
    const CString dir(ConcatenatePath(parent_dir, directories[i]));
    EXPECT_SUCCEEDED(CreateDir(dir, NULL));
  }
}

void CreateFiles(const TCHAR* parent_dir,
                 const FileStruct files[],
                 size_t number_of_files) {
  for (size_t i = 0; i != number_of_files; ++i) {
    const CString filename(ConcatenatePath(parent_dir, files[i].filename));
    scoped_handle file_handle(::CreateFile(filename,
                                           GENERIC_WRITE,
                                           0,
                                           NULL,
                                           CREATE_ALWAYS,
                                           files[i].file_attributes,
                                           NULL));
    EXPECT_NE(INVALID_HANDLE_VALUE, get(file_handle));
  }
}

HRESULT SetPolicy(const TCHAR* policy_name, DWORD value) {
  ON_SCOPE_EXIT_OBJ(*ConfigManager::Instance(),
                    &ConfigManager::LoadPolicies,
                    true);
  return RegKey::SetValue(kRegKeyGoopdateGroupPolicy, policy_name, value);
}

HRESULT SetPolicyString(const TCHAR* policy_name, const CString& value) {
  ON_SCOPE_EXIT_OBJ(*ConfigManager::Instance(),
                    &ConfigManager::LoadPolicies,
                    true);
  return RegKey::SetValue(kRegKeyGoopdateGroupPolicy, policy_name, value);
}

void ClearGroupPolicies() {
  RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
  ConfigManager::Instance()->LoadPolicies(true);
}

}  // namespace omaha

std::ostream& operator<<(std::ostream& os, const CString& str) {
  ::testing::internal::UniversalPrint(str.GetString(), &os);
  return os;
}
