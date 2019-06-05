// Copyright 2008-2010 Google Inc.
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

#include "omaha/base/app_util.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system.h"
#include "omaha/base/timer.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/core/google_update_core.h"
#include "omaha/setup/setup_service.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

const TCHAR update_key[]      = MACHINE_REG_UPDATE;
const TCHAR appid_key[]       = MACHINE_REG_CLIENTS_GOOPDATE;
const TCHAR appid_state_key[] = MACHINE_REG_CLIENT_STATE_GOOPDATE;

}  // namespace

class GoogleUpdateCoreTest : public testing::Test {
 protected:
  GoogleUpdateCoreTest() {
  }

  static void SetUpTestCase() {
    if (vista_util::IsUserAdmin()) {
      System::AdjustPrivilege(SE_DEBUG_NAME, true);
      TerminateAllGoogleUpdateProcesses();
    }

    const CString shell_path = goopdate_utils::BuildGoogleUpdateExePath(true);
    EXPECT_SUCCEEDED(RegKey::SetValue(update_key,
                                      kRegValueInstalledPath,
                                      shell_path));

    EXPECT_SUCCEEDED(RegKey::SetValue(update_key,
                                      kRegValueInstalledVersion,
                                      GetVersionString()));

    EXPECT_SUCCEEDED(RegKey::SetValue(appid_key,
                                      kRegValueProductVersion,
                                      GetVersionString()));

    EXPECT_SUCCEEDED(RegKey::SetValue(appid_key, _T("fc"), _T("fc /?")));

    EXPECT_SUCCEEDED(RegKey::SetValue(appid_state_key,
                                      kRegValueProductVersion,
                                      GetVersionString()));

    CopyGoopdateFiles(GetGoogleUpdateMachinePath(), GetVersionString());
  }

  static void TearDownTestCase() {
    if (vista_util::IsUserAdmin()) {
      TerminateAllGoogleUpdateProcesses();
    }

    EXPECT_SUCCEEDED(RegKey::DeleteValue(appid_key, _T("fc")));
  }

  void DoLaunchCmdElevatedTests(IUnknown* core_object);
};

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

TEST_F(GoogleUpdateCoreTest, LaunchCmdElevated_LocalServerRegistered) {
  RegisterOrUnregisterGoopdateLocalServer(true);

  CComPtr<IUnknown> local_server_com;
  EXPECT_SUCCEEDED(System::CoCreateInstanceAsAdmin(
      NULL, __uuidof(GoogleUpdateCoreMachineClass),
      IID_PPV_ARGS(&local_server_com)));

  DoLaunchCmdElevatedTests(local_server_com);

  local_server_com.Release();

  RegisterOrUnregisterGoopdateLocalServer(false);
}

TEST_F(GoogleUpdateCoreTest,
       LaunchCmdElevated_ServiceAndLocalServerRegistered) {
  RegisterOrUnregisterGoopdateService(true);
  RegisterOrUnregisterGoopdateLocalServer(true);

  CComPtr<IUnknown> service_com;
  EXPECT_SUCCEEDED(
      service_com.CoCreateInstance(__uuidof(GoogleUpdateCoreClass)));

  DoLaunchCmdElevatedTests(service_com);

  CComPtr<IUnknown> local_server_com;
  EXPECT_SUCCEEDED(System::CoCreateInstanceAsAdmin(
      NULL, __uuidof(GoogleUpdateCoreMachineClass),
      IID_PPV_ARGS(&local_server_com)));

  DoLaunchCmdElevatedTests(local_server_com);

  service_com.Release();
  local_server_com.Release();

  RegisterOrUnregisterGoopdateLocalServer(false);
  RegisterOrUnregisterGoopdateService(false);
}

// TODO(omaha): This test is disabled because it frequently gets
// ERROR_SERVICE_CANNOT_ACCEPT_CTRL when trying to stop the service during
// cleanup.
TEST_F(GoogleUpdateCoreTest, DISABLED_LaunchCmdElevated_ServiceRunning) {
  if (!vista_util::IsUserAdmin()) {
    SUCCEED() << "\tTest did not run because the user is not an admin.";
    return;
  }

  RegisterOrUnregisterGoopdateService(true);

  // RegisterOrUnregisterGoopdateLocalServer is needed for handler registration.
  RegisterOrUnregisterGoopdateLocalServer(true);

  EXPECT_SUCCEEDED(SetupUpdateMediumService::StartService());
  CComPtr<IUnknown> service_com;

  // On slow machines, the service may take some time to start, and the first
  // couple of attempts at CoCreation fail on slower machines. Adding in 2
  // retries to address this issue.
  HRESULT hr_create_core_class = CO_E_SERVER_EXEC_FAILURE;
  for (int i = 0;
       hr_create_core_class == CO_E_SERVER_EXEC_FAILURE && i < 3;
       ++i) {
    hr_create_core_class =
        service_com.CoCreateInstance(__uuidof(GoogleUpdateCoreClass));
  }

  EXPECT_SUCCEEDED(hr_create_core_class);

  DoLaunchCmdElevatedTests(service_com);

  service_com.Release();
  EXPECT_SUCCEEDED(SetupUpdateMediumService::StopService());

  RegisterOrUnregisterGoopdateLocalServer(false);
  RegisterOrUnregisterGoopdateService(false);
}

}  // namespace omaha

