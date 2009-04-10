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

#include "omaha/common/app_util.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/constants.h"
#include "omaha/common/error.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/system.h"
#include "omaha/common/vistautil.h"

#include "omaha/core/google_update_core.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/google_update_proxy.h"
#include "omaha/setup/setup_service.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class GoogleUpdateCoreTest : public testing::Test {
 protected:
  GoogleUpdateCoreTest() {
    GUID proxy_clsid = PROXY_CLSID_IS;
    proxy_guid_ = CW2A(GuidToString(proxy_clsid));

    regedit_uninstall_string_.Format(
        "Windows Registry Editor Version 5.00\r\n"
        "\r\n"
        "[-HKEY_LOCAL_MACHINE\\Software\\Google\\Update\\Clients\\{430FD4D0-B729-4F61-AA34-91526481799D}]\r\n"  // NOLINT
        "\r\n"
        "[-HKEY_LOCAL_MACHINE\\Software\\Google\\Update\\ClientState\\{430FD4D0-B729-4F61-AA34-91526481799D}]\r\n"  // NOLINT
        "\r\n"
        "[-HKEY_LOCAL_MACHINE\\Software\\Google\\Update\\network\\{430FD4D0-B729-4F61-AA34-91526481799D}]\r\n"  // NOLINT
        "\r\n"
        "[-HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Interface\\{909489C2-85A6-4322-AA56-D25278649D67}]\r\n"  // NOLINT
        "\r\n"
        "[-HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\CLSID\\%s]\r\n"
        "\r\n"

        // Registration for the user must be cleared too since COM looks up HKCR
        // and that resolves to either HKCU or HKLM classes.
        "[-HKEY_CURRENT_USER\\SOFTWARE\\Classes\\Interface\\{909489C2-85A6-4322-AA56-D25278649D67}]\r\n"  // NOLINT
        "\r\n"
        "[-HKEY_CURRENT_USER\\SOFTWARE\\Classes\\CLSID\\%s]\r\n"
        "\r\n",
        proxy_guid_, proxy_guid_);
  }

  virtual void SetUp() {
    if (!vista_util::IsUserAdmin()) {
      return;
    }
    System::AdjustPrivilege(SE_DEBUG_NAME, true);
    TerminateAllGoogleUpdateProcesses();
    SetupRegistry();
  }

  virtual void TearDown() {
    if (!vista_util::IsUserAdmin()) {
      return;
    }
    TerminateAllGoogleUpdateProcesses();
    TeardownRegistry();
  }

  CString GetCommandToLaunch(const TCHAR* app_guid, const TCHAR* cmd_id) {
    return GoogleUpdateCore::GetCommandToLaunch(app_guid, cmd_id);
  }

  HRESULT LaunchCmd(CString* cmd,
                    HANDLE caller_proc_handle,
                    ULONG_PTR* proc_handle) {
    return GoogleUpdateCore::LaunchCmd(cmd, caller_proc_handle, proc_handle);
  }

  // Starts the core and waits for it to register its shared memory section.
  HANDLE StartCore();

  // Creates the COM registration required by the test.
  void SetupRegistry();

  // Cleans up the COM registration.
  void TeardownRegistry();

  void DoLaunchCmdElevatedTests(IUnknown* core_object);

  // The guid of the proxy code. This is a build constant.
  CStringA proxy_guid_;

  CStringA regedit_uninstall_string_;
};

void WriteRegeditDataToRegistry(const CStringA& regedit_data) {
  EXPECT_TRUE(regedit_data.GetLength());

  CString temp_file;
  EXPECT_TRUE(::GetTempFileName(app_util::GetModuleDirectory(NULL),
                                _T("reg"),
                                0,
                                CStrBuf(temp_file, MAX_PATH)));

  CString reg_file_path = temp_file + _T(".reg");
  scoped_hfile file_handle(::CreateFile(reg_file_path,
                                        GENERIC_WRITE,
                                        FILE_SHARE_READ,
                                        NULL,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL));
  EXPECT_TRUE(file_handle);
  DWORD bytes_written = 0;
  EXPECT_TRUE(::WriteFile(get(file_handle),
                          regedit_data,
                          regedit_data.GetLength(),
                          &bytes_written,
                          NULL));
  EXPECT_TRUE(bytes_written);
  reset(file_handle);

  CString regedit_path(_T("regedit.exe"));
  CString cmd_line;
  cmd_line.Format(_T("/s \"%s\""), reg_file_path);

  if (vista_util::IsUserAdmin()) {
    EXPECT_SUCCEEDED(RegisterOrUnregisterExe(regedit_path, cmd_line));
  } else {
    // Elevate RegEdit.exe for medium integrity users on Vista and above.
    DWORD exit_code(0);
    EXPECT_SUCCEEDED(vista_util::RunElevated(regedit_path,
                                             cmd_line,
                                             SW_SHOWNORMAL,
                                             &exit_code));
    EXPECT_EQ(0, exit_code);
  }

  EXPECT_TRUE(::DeleteFile(temp_file));
  EXPECT_TRUE(::DeleteFile(reg_file_path));
}

void GoogleUpdateCoreTest::SetupRegistry() {
  CStringA regedit_data(regedit_uninstall_string_);

  CString unittest_dir = app_util::GetModuleDirectory(NULL);
  CStringA goopdate_dll(CW2A(ConcatenatePath(unittest_dir, kGoopdateDllName)));
  goopdate_dll.Replace("\\", "\\\\");

  regedit_data.AppendFormat(
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Interface\\{909489C2-85A6-4322-AA56-D25278649D67}]\r\n"  // NOLINT
      "@=\"IGoogleUpdateCore\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Interface\\{909489C2-85A6-4322-AA56-D25278649D67}\\NumMethods]\r\n"  // NOLINT
      "@=\"%d\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Interface\\{909489C2-85A6-4322-AA56-D25278649D67}\\ProxyStubClsid32]\r\n"  // NOLINT
      "@=\"%s\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\CLSID\\%s]\r\n"
      "@=\"PSFactoryBuffer\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\CLSID\\%s\\InProcServer32]\r\n"
      "@=\"%s\"\r\n"
      "\"ThreadingModel\"=\"Both\"\r\n"
      "\r\n",
      4, proxy_guid_, proxy_guid_, proxy_guid_, goopdate_dll);

      // Create one mock client so that core does not try to start an
      // uninstall worker when the unit test is manipulating the clients key.
      // And create entries to satisfy Setup::CheckInstallStateConsistency().
  regedit_data.Append(
      "[HKEY_LOCAL_MACHINE\\Software\\Google\\Update\\Clients\\{430FD4D0-B729-4F61-AA34-91526481799D}]\r\n"  // NOLINT
      "\"fc\"=\"fc /?\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\Software\\Google\\Update\\network\\{430FD4D0-B729-4F61-AA34-91526481799D}]\r\n"  // NOLINT
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\Software\\Google\\Update]\r\n"
      "\"path\"=\"blah\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\Software\\Google\\Update]\r\n"
      "\"version\"=\"0.0.0.1\"\r\n"
      "\r\n"
      "[HKEY_LOCAL_MACHINE\\Software\\Google\\Update\\ClientState\\{430FD4D0-B729-4F61-AA34-91526481799D}]\r\n"  // NOLINT
      "\"pv\"=\"0.0.0.1\"\r\n"
      "\r\n");

  WriteRegeditDataToRegistry(regedit_data);
}

void GoogleUpdateCoreTest::TeardownRegistry() {
  WriteRegeditDataToRegistry(regedit_uninstall_string_);
}

HANDLE GoogleUpdateCoreTest::StartCore() {
  CString unittest_dir = app_util::GetModuleDirectory(NULL);
  CString google_update = ConcatenatePath(unittest_dir, kGoopdateFileName);
  EnclosePath(&google_update);
  HANDLE handle = NULL;
  LaunchProcessAsSystem(google_update + _T(" /c"), &handle);
  EXPECT_TRUE(handle != NULL);
  if (!handle) {
    return NULL;
  }

  // Give the core some time to start and register its shared memory section.
  // Sometimes psexec is slow to start the machine core, so give it some
  // time to run.
  size_t count(0), kMaxCount(10);
  while (count++ < kMaxCount) {
    const TCHAR* shmem_name = kGoogleUpdateCoreSharedMemoryName;
    scoped_file_mapping shmem_handle(::OpenFileMapping(FILE_MAP_READ,
                                                       false,
                                                       shmem_name));
    if (shmem_handle) {
      break;
    }

    ::Sleep(1000);
  }

  return handle;
}

void GoogleUpdateCoreTest::DoLaunchCmdElevatedTests(IUnknown* core_object) {
  CComQIPtr<IGoogleUpdateCore> google_update_core = core_object;
  EXPECT_TRUE(google_update_core != NULL);
  if (!google_update_core) {
    return;
  }

  ULONG_PTR proc_handle = 0;
  DWORD caller_proc_id = ::GetCurrentProcessId();

  // Returns ERROR_BAD_IMPERSONATION_LEVEL when explicit security blanket is not
  // set.
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_BAD_IMPERSONATION_LEVEL),
            google_update_core->LaunchCmdElevated(kGoogleUpdateAppId,
                                                  _T("cmd"),
                                                  caller_proc_id,
                                                  &proc_handle));
  EXPECT_EQ(0, proc_handle);

  // Sets a security blanket that will allow the server to impersonate the
  // client.
  EXPECT_SUCCEEDED(::CoSetProxyBlanket(google_update_core,
      RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL,
      RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL,
      EOAC_DEFAULT));

  // Returns GOOPDATE_E_CORE_MISSING_CMD when the command is missing in
  // the registry.
  EXPECT_EQ(GOOPDATE_E_CORE_MISSING_CMD,
            google_update_core->LaunchCmdElevated(kGoogleUpdateAppId,
                                                  _T("cmd"),
                                                  caller_proc_id,
                                                  &proc_handle));
  EXPECT_EQ(0, proc_handle);

  // Returns E_INVALIDARG when the app_guid is not a guid.
  EXPECT_EQ(E_INVALIDARG,
            google_update_core->LaunchCmdElevated(_T("noguid"),
                                                  _T("cmd"),
                                                  caller_proc_id,
                                                  &proc_handle));

  EXPECT_SUCCEEDED(google_update_core->LaunchCmdElevated(kGoogleUpdateAppId,
                                                         _T("fc"),
                                                         caller_proc_id,
                                                         &proc_handle));
  EXPECT_NE(0, proc_handle);

  // TODO(Omaha): Perhaps attempt some negative tests here, either by testing
  // the permissions on the handle explicitly, or by attempting VM operations or
  // such on the process handle, since it's a serious security issue if the
  // handle permissions are too wide.
  HANDLE handle = reinterpret_cast<HANDLE>(proc_handle);
  EXPECT_NE(WAIT_FAILED, ::WaitForSingleObject(handle, 10000));
  EXPECT_TRUE(::CloseHandle(handle));
}

TEST_F(GoogleUpdateCoreTest, LaunchCmdElevated_CoreNotRunning) {
  if (!vista_util::IsUserAdmin()) {
    SUCCEED() << "\tTest did not run because the user is not an admin.";
    return;
  }

  SharedMemoryAttributes attr(kGoogleUpdateCoreSharedMemoryName,
                              CSecurityDesc());
  GoogleUpdateCoreProxy google_update_core_proxy(true, &attr);
  CComPtr<IGoogleUpdateCore> google_update_core;
  EXPECT_FAILED(google_update_core_proxy.GetObject(&google_update_core));
}

TEST_F(GoogleUpdateCoreTest, LaunchCmdElevated) {
  if (!vista_util::IsUserAdmin()) {
    SUCCEED() << "\tTest did not run because the user is not an admin.";
    return;
  }

  // Start the machine instance of the core.
  StartCore();

  // Get the proxy for the core interface.
  SharedMemoryAttributes attr(kGoogleUpdateCoreSharedMemoryName,
                              CSecurityDesc());
  GoogleUpdateCoreProxy google_update_core_proxy(true, &attr);
  CComPtr<IGoogleUpdateCore> google_update_core;

  HRESULT hr(google_update_core_proxy.GetObject(&google_update_core));
  if (FAILED(hr) && vista_util::IsUserAdmin()) {
    // This test is failing from time to time on pulse. A log of the core
    // process might give us more information on what exactly is going wrong.
    // Using goopdump.exe to dump this information, which should appear in the
    // pulse log.
    CString goopdump_path =
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        _T("GoopDump.exe"));
    EXPECT_SUCCEEDED(RegisterOrUnregisterExe(goopdump_path, _T(" ")));
  }

  EXPECT_SUCCEEDED(hr);
  EXPECT_TRUE(google_update_core != NULL);

  DoLaunchCmdElevatedTests(google_update_core);
}

TEST_F(GoogleUpdateCoreTest, GetCommandToLaunch) {
  if (!vista_util::IsUserAdmin()) {
    SUCCEED() << "\tTest did not run because the user is not an admin.";
    return;
  }
  EXPECT_STREQ(_T(""), GetCommandToLaunch(NULL, _T("foo")));
  EXPECT_STREQ(_T(""), GetCommandToLaunch(_T("bar"), NULL));

  CString cmd = GetCommandToLaunch(_T("{430FD4D0-B729-4F61-AA34-91526481799D}"),
                                   _T("cmd"));
  EXPECT_STREQ(_T(""), cmd);

  const TCHAR* key_name = _T("HKLM\\Software\\Google\\Update\\Clients\\")
                          _T("{430FD4D0-B729-4F61-AA34-91526481799D}");
  EXPECT_SUCCEEDED(RegKey::SetValue(key_name, _T("cmd"), _T("foobar")));

  cmd = GetCommandToLaunch(_T("{430FD4D0-B729-4F61-AA34-91526481799D}"),
                           _T("cmd"));
  EXPECT_STREQ(_T("foobar"), cmd);
}

TEST_F(GoogleUpdateCoreTest, LaunchCmd) {
  if (!vista_util::IsUserAdmin()) {
    SUCCEED() << "\tTest did not run because the user is not an admin.";
    return;
  }
  ULONG_PTR proc_handle = 0;
  scoped_process caller_proc_handle(::OpenProcess(PROCESS_DUP_HANDLE,
                                                  false,
                                                  ::GetCurrentProcessId()));
  EXPECT_TRUE(caller_proc_handle);

  CString cmd = _T("cmd /c \"dir > nul\"");
  EXPECT_SUCCEEDED(LaunchCmd(&cmd, get(caller_proc_handle), &proc_handle));

  EXPECT_NE(0, proc_handle);

  HANDLE handle = reinterpret_cast<HANDLE>(proc_handle);
  EXPECT_NE(WAIT_FAILED, ::WaitForSingleObject(handle, 10000));
  EXPECT_TRUE(::CloseHandle(handle));
}

class GoogleUpdateCoreServiceTest : public GoogleUpdateCoreTest {
 protected:
  virtual void SetUp() {
    SetupRegistry();
    RegisterOrUnregisterService(true);
  }

  virtual void TearDown() {
    RegisterOrUnregisterService(false);
    TeardownRegistry();
  }

  void RegisterOrUnregisterService(bool reg);
};

void GoogleUpdateCoreServiceTest::RegisterOrUnregisterService(bool reg) {
  CString unittest_dir = app_util::GetModuleDirectory(NULL);
  CString service_path = ConcatenatePath(unittest_dir, kServiceFileName);
  EnclosePath(&service_path);

  CommandLineBuilder builder(reg ? COMMANDLINE_MODE_SERVICE_REGISTER :
                                   COMMANDLINE_MODE_SERVICE_UNREGISTER);
  CString cmd_line = builder.GetCommandLineArgs();
  if (vista_util::IsUserAdmin()) {
    EXPECT_SUCCEEDED(RegisterOrUnregisterExe(service_path, cmd_line));
    return;
  }

  // Elevate for medium integrity users on Vista and above.
  DWORD exit_code(S_OK);
  EXPECT_SUCCEEDED(vista_util::RunElevated(service_path,
                                           cmd_line,
                                           SW_SHOWNORMAL,
                                           &exit_code));
  EXPECT_SUCCEEDED(exit_code);
}

TEST_F(GoogleUpdateCoreServiceTest, LaunchCmdElevated_ServiceNotRunning) {
  CComPtr<IUnknown> service_com;
  EXPECT_SUCCEEDED(
      service_com.CoCreateInstance(__uuidof(GoogleUpdateCoreClass)));

  DoLaunchCmdElevatedTests(service_com);

  service_com.Release();
}

TEST_F(GoogleUpdateCoreServiceTest, LaunchCmdElevated_ServiceRunning) {
  if (!vista_util::IsUserAdmin()) {
    SUCCEED() << "\tTest did not run because the user is not an admin.";
    return;
  }
  EXPECT_SUCCEEDED(SetupService::StartService());
  CComPtr<IUnknown> service_com;
  EXPECT_SUCCEEDED(
      service_com.CoCreateInstance(__uuidof(GoogleUpdateCoreClass)));

  DoLaunchCmdElevatedTests(service_com);

  service_com.Release();
}

}  // namespace omaha

