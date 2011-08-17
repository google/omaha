// Copyright 2009 Google Inc.
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

#include "omaha/base/constants.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/goopdate/cred_dialog.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class CredentialDialogTest : public testing::Test {
 protected:
  explicit CredentialDialogTest(bool is_machine)
      : is_machine_(is_machine) {}

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  virtual bool ShouldRunTest() {
    if (IsEnvironmentVariableSet(_T("OMAHA_TEST_UI"))) {
      return true;
    } else {
      std::wcout << _T("\tThis test did not run because 'OMAHA_TEST_UI' is ")
                    _T("not set in the environment.") << std::endl;
      return false;
    }
  }

  virtual void TestComObject(LPCTSTR server, LPCTSTR caption) {
    REFCLSID clsid = is_machine_ ? __uuidof(CredentialDialogMachineClass)
                                 : __uuidof(CredentialDialogUserClass);
    CComPtr<ICredentialDialog> dialog;
    EXPECT_SUCCEEDED(dialog.CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER));
    if (dialog) {
      CComBSTR server_bs(server);
      CComBSTR caption_bs(caption);
      CComBSTR user;
      CComBSTR pass;
      EXPECT_SUCCEEDED(dialog->QueryUserForCredentials(0,
                                                       server_bs,
                                                       caption_bs,
                                                       &user,
                                                       &pass));
    }
  }

  virtual void TestAuto(LPCTSTR server, LPCTSTR caption) {
    CString user;
    CString pass;
    EXPECT_SUCCEEDED(LaunchCredentialDialog(is_machine_,
                                            NULL,
                                            CString(server),
                                            CString(caption),
                                            &user,
                                            &pass));
  }

  // TestAutoImpersonating() assumes that omaha_unittest.exe is being executed
  // as LocalSystem; it will fail otherwise.
  virtual void TestAutoImpersonating(LPCTSTR server, LPCTSTR caption) {
    EXPECT_SUCCEEDED(InitializeClientSecurity());

    // Manually load the COM proxy into the process before we start; it will
    // assert on DLL load if we load it while impersonating.  (PSMachine should
    // always be loaded already in a production environment; this is just an
    // artifact of the unit test environment.)
    CPath proxy_path(goopdate_utils::BuildInstallDirectory(
                         is_machine_, omaha::GetVersionString()));
    EXPECT_TRUE(proxy_path.Append(kPSFileNameMachine));
    scoped_library psmachine_load(::LoadLibrary(proxy_path));
    EXPECT_TRUE(!!psmachine_load);

    scoped_handle logged_on_user_token(
        goopdate_utils::GetImpersonationTokenForMachineProcess(is_machine_));
    EXPECT_TRUE(valid(logged_on_user_token));
    if (valid(logged_on_user_token)) {
      scoped_impersonation impersonate_user(get(logged_on_user_token));
      HRESULT hr = HRESULT_FROM_WIN32(impersonate_user.result());
      EXPECT_SUCCEEDED(hr);
      if (SUCCEEDED(hr)) {
        TestAuto(server, caption);
      }
    }
  }

  const bool is_machine_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CredentialDialogTest);
};

class CredentialDialogMachineTest : public CredentialDialogTest {
 protected:
  CredentialDialogMachineTest() : CredentialDialogTest(true) {}
};

class CredentialDialogUserTest : public CredentialDialogTest {
 protected:
  CredentialDialogUserTest() : CredentialDialogTest(false) {}
};

TEST_F(CredentialDialogUserTest, COM) {
  if (ShouldRunTest()) {
    TestComObject(_T("test-u-com-server"), _T("test-u-com-caption"));
  }
}

TEST_F(CredentialDialogUserTest, Auto) {
  if (ShouldRunTest()) {
    TestAuto(_T("test-u-auto-server"), _T("test-u-auto-caption"));
  }
}

TEST_F(CredentialDialogMachineTest, COM) {
  if (ShouldRunTest()) {
    TestComObject(_T("test-m-com-server"), _T("test-m-com-caption"));
  }
}

TEST_F(CredentialDialogMachineTest, Auto) {
  if (ShouldRunTest()) {
    TestAuto(_T("test-m-auto-server"), _T("test-m-auto-caption"));
  }
}

TEST_F(CredentialDialogMachineTest, Impersonating) {
  bool is_system = false;
  EXPECT_SUCCEEDED(IsSystemProcess(&is_system));
  if (!is_system) {
    std::wcout << _T("\tThis test is only meaningful when the unit test is ")
                  _T("run as the SYSTEM account.") << std::endl;
  } else {
    TestAutoImpersonating(_T("test-m-imp-server"), _T("test-m-imp-caption"));
  }
}

}  // end namespace omaha

