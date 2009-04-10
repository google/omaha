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

#include <windows.h>
#include <atlpath.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <map>
#include <vector>
#include "omaha/common/app_util.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/const_utils.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_ptr_cotask.h"
#include "omaha/common/string.h"
#include "omaha/common/time.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/common/vista_utils.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/extra_args_parser.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/goopdate_utils-internal.h"
#include "omaha/testing/resource.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

#define DUMMY_CLSID  _T("{6FC94136-0D4C-450e-99C2-BCDA72A9C8F0}")
const TCHAR* hkcr_key_name = _T("HKCR\\CLSID\\") DUMMY_CLSID;
const TCHAR* hklm_key_name = _T("HKLM\\Software\\Classes\\CLSID\\")
                             DUMMY_CLSID;
const TCHAR* hkcu_key_name = _T("HKCU\\Software\\Classes\\CLSID\\")
                             DUMMY_CLSID;

#define APP_GUID _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}")
const TCHAR* const kAppGuid = APP_GUID;
const TCHAR* const kAppMachineClientStatePath =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\") APP_GUID;
const TCHAR* const kAppUserClientStatePath =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\") APP_GUID;
const TCHAR* const kAppMachineClientStateMediumPath =
    _T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\") APP_GUID;

// This should never exist. This contant is only used to verify it is not used.
const TCHAR* const kAppUserClientStateMediumPath =
    _T("HKCU\\Software\\Google\\Update\\ClientStateMedium\\") APP_GUID;

// Constants used in the scheduled task tests.
const int kMaxWaitForProcessMs              = 5000;
const int kWaitForProcessIntervalMs         = 100;
const int kMaxWaitForProcessIterations      =
    kMaxWaitForProcessMs / kWaitForProcessIntervalMs;

// Verifies that one of the expected OS strings was found in the url.
// Returns the position along with the length of the OS string.
int VerifyOSInUrl(const CString& url, int* length) {
  ASSERT1(length);
  *length = 0;

  // The strings are in descending version order to avoid breaking on a
  // substring of the version we are looking for.
  const TCHAR* kExpectedOsStrings[] = {_T("6.0&sp=Service%20Pack%201"),
                                       _T("6.0&sp="),
                                       _T("5.2&sp=Service%20Pack%202"),
                                       _T("5.2&sp=Service%20Pack%201"),
                                       _T("5.1&sp=Service%20Pack%203"),
                                       _T("5.1&sp=Service%20Pack%202"),
                                      };

  bool found = false;
  int this_pos = 0;

  for (int i = 0; i < arraysize(kExpectedOsStrings); ++i) {
    this_pos = url.Find(kExpectedOsStrings[i]);
    if (-1 != this_pos) {
      found = true;
      *length = _tcslen(kExpectedOsStrings[i]);
      break;
    }
  }

  EXPECT_TRUE(found);
  return this_pos;
}

}  // namespace

namespace goopdate_utils {

namespace internal {

TEST(GoopdateUtilsTest, ScheduledTasks) {
  const TCHAR kSchedTestTaskName[]            = _T("TestScheduledTask");
  const TCHAR kScheduledTaskExecutable[]      = _T("netstat.exe");
  const TCHAR kScheduledTaskParameters[]      = _T("20");
  const TCHAR kSchedTestTaskComment[]         = _T("Google Test Task");

  CString task_path = ConcatenatePath(app_util::GetSystemDir(),
                                      kScheduledTaskExecutable);
  // Install/uninstall.
  EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                        task_path,
                                        _T(""),
                                        kSchedTestTaskComment,
                                        vista_util::IsUserAdmin(),
                                        vista_util::IsUserAdmin(),
                                        true));
  EXPECT_SUCCEEDED(UninstallScheduledTask(kSchedTestTaskName));

  // Calling InstallScheduledTask twice should succeed.
  for (int i = 0; i < 2; ++i) {
    EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                          task_path,
                                          _T(""),
                                          kSchedTestTaskComment,
                                          vista_util::IsUserAdmin(),
                                          vista_util::IsUserAdmin(),
                                          true));
  }

  // "Upgrade" to a new version, which now has parameters.
  EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                        task_path,
                                        kScheduledTaskParameters,
                                        kSchedTestTaskComment,
                                        vista_util::IsUserAdmin(),
                                        vista_util::IsUserAdmin(),
                                        true));

  // Start and stop.
  EXPECT_SUCCEEDED(StartScheduledTask(kSchedTestTaskName));
  HRESULT hr = E_FAIL;
  for (int tries = 0;
       tries < kMaxWaitForProcessIterations && SCHED_S_TASK_RUNNING != hr;
       ++tries) {
    ::Sleep(kWaitForProcessIntervalMs);
    hr = GetScheduledTaskStatus(kSchedTestTaskName);
  }
  EXPECT_EQ(SCHED_S_TASK_RUNNING, hr);

  // Without a wait in between the start and stop, stop failed intermittently.
  ::Sleep(500);

  EXPECT_SUCCEEDED(StopScheduledTask(kSchedTestTaskName));
  hr = SCHED_S_TASK_RUNNING;
  for (int tries = 0;
       tries < kMaxWaitForProcessIterations && SCHED_S_TASK_RUNNING == hr;
       ++tries) {
    ::Sleep(kWaitForProcessIntervalMs);
    hr = GetScheduledTaskStatus(kSchedTestTaskName);
  }
  EXPECT_NE(SCHED_S_TASK_RUNNING, hr);

  // Finally, uninstall.
  EXPECT_SUCCEEDED(UninstallScheduledTask(kSchedTestTaskName));
}

}  // namespace internal

static void Cleanup() {
  ASSERT_SUCCEEDED(goopdate_utils::RemoveRedirectHKCR());

  RegKey::DeleteKey(hkcr_key_name, true);
  RegKey::DeleteKey(hklm_key_name, true);
  RegKey::DeleteKey(hkcu_key_name, true);
}

static void TestGetBrowserToRestart(BrowserType stamped,
                                    bool found1,
                                    bool killed1,
                                    BrowserType def_browser,
                                    bool found2,
                                    bool killed2,
                                    BrowserType expected) {
  TerminateBrowserResult res(found1, killed1);
  TerminateBrowserResult def(found2, killed2);

  BrowserType type = BROWSER_UNKNOWN;
  if (expected == BROWSER_UNKNOWN) {
    ASSERT_FALSE(goopdate_utils::GetBrowserToRestart(stamped,
                                                     def_browser,
                                                     res,
                                                     def,
                                                     &type));
  } else {
    ASSERT_TRUE(goopdate_utils::GetBrowserToRestart(stamped,
                                                   def_browser,
                                                   res,
                                                   def,
                                                   &type));
  }
  ASSERT_EQ(expected, type);
}

TEST(GoopdateUtilsTest, GetAppClientsKey) {
  const TCHAR kAppGuid[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");

  EXPECT_STREQ(_T("HKCU\\Software\\Google\\Update\\Clients\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               goopdate_utils::GetAppClientsKey(false, kAppGuid));
  EXPECT_STREQ(_T("HKLM\\Software\\Google\\Update\\Clients\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               goopdate_utils::GetAppClientsKey(true, kAppGuid));
}

TEST(GoopdateUtilsTest, GetAppClientStateKey) {
  const TCHAR kAppGuid[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");

  EXPECT_STREQ(_T("HKCU\\Software\\Google\\Update\\ClientState\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               goopdate_utils::GetAppClientStateKey(false, kAppGuid));
  EXPECT_STREQ(_T("HKLM\\Software\\Google\\Update\\ClientState\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               goopdate_utils::GetAppClientStateKey(true, kAppGuid));
}

// This is an invalid case and causes an assert. Always returns HKLM path.
TEST(GoopdateUtilsTest, GetAppClientStateMediumKey_User) {
  const TCHAR kAppGuid[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");
  ExpectAsserts expect_asserts;
  EXPECT_STREQ(_T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               goopdate_utils::GetAppClientStateMediumKey(false, kAppGuid));
}

TEST(GoopdateUtilsTest, GetAppClientStateMediumKey_Machine) {
  const TCHAR kAppGuid[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");
  EXPECT_STREQ(_T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               goopdate_utils::GetAppClientStateMediumKey(true, kAppGuid));
}

// This is an invalid case and causes an assert.
TEST(GoopdateUtilsTest, GetAppClientStateMediumKey_UserAndMachineAreSame) {
  const TCHAR kAppGuid[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");
  ExpectAsserts expect_asserts;
  EXPECT_STREQ(goopdate_utils::GetAppClientStateMediumKey(true, kAppGuid),
               goopdate_utils::GetAppClientStateMediumKey(false, kAppGuid));
}

TEST(GoopdateUtilsTest, GetBrowserToRestart) {
  // case 1 - Stamp = IE, default = IE
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);

  // case 2 - Stamp = IE, default = FireFox
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_IE);

  // case 3 - Stamp = FireFox, default = IE
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, true, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, true, true,
                          BROWSER_FIREFOX);


  // case 4 - Stamp = FireFox, default = FireFox
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
}

TEST(GoopdateUtilsTest, ConvertStringToBrowserType) {
  BrowserType type = BROWSER_UNKNOWN;
  ASSERT_SUCCEEDED(goopdate_utils::ConvertStringToBrowserType(_T("0"), &type));
  ASSERT_EQ(BROWSER_UNKNOWN, type);

  ASSERT_SUCCEEDED(goopdate_utils::ConvertStringToBrowserType(_T("1"), &type));
  ASSERT_EQ(BROWSER_DEFAULT, type);

  ASSERT_SUCCEEDED(goopdate_utils::ConvertStringToBrowserType(_T("2"), &type));
  ASSERT_EQ(BROWSER_IE, type);

  ASSERT_SUCCEEDED(goopdate_utils::ConvertStringToBrowserType(_T("3"), &type));
  ASSERT_EQ(BROWSER_FIREFOX, type);

  ASSERT_FAILED(goopdate_utils::ConvertStringToBrowserType(_T("4"), &type));
  ASSERT_FAILED(goopdate_utils::ConvertStringToBrowserType(_T("asdf"), &type));
  ASSERT_FAILED(goopdate_utils::ConvertStringToBrowserType(_T("234"), &type));
  ASSERT_FAILED(goopdate_utils::ConvertStringToBrowserType(_T("-1"), &type));
}

TEST(GoopdateUtilsTest, RedirectHKCRTest) {
  RegKey key;
  Cleanup();

  if (vista_util::IsUserAdmin()) {
    // Only run this part of the test for Admins, because non-admins cannot
    // write to HKLM.

    // Without redirection, a HKCR write should write HKLM\Software\Classes,
    // assuming that the key does not already exist in HKCU.
    ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
    ASSERT_TRUE(RegKey::HasKey(hklm_key_name));
    ASSERT_FALSE(RegKey::HasKey(hkcu_key_name));

    Cleanup();

    ASSERT_SUCCEEDED(goopdate_utils::RedirectHKCR(true));

    // With HKLM redirection, a HKCR write should write HKLM\Software\Classes.
    ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
    ASSERT_TRUE(RegKey::HasKey(hklm_key_name));
    ASSERT_FALSE(RegKey::HasKey(hkcu_key_name));

    Cleanup();
  } else {
    std::wcout << _T("\tPart of this test did not run because the user ")
                  _T("is not an admin.") << std::endl;
  }

  ASSERT_SUCCEEDED(goopdate_utils::RedirectHKCR(false));

  // With HKCU redirection, a HKCR write should write HKCU\Software\Classes.
  ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
  ASSERT_FALSE(RegKey::HasKey(hklm_key_name));
  ASSERT_TRUE(RegKey::HasKey(hkcu_key_name));

  ASSERT_SUCCEEDED(goopdate_utils::RemoveRedirectHKCR());

  if (vista_util::IsUserAdmin()) {
    // Without redirection, the following HKCR writes should write
    // HKCU\Software\Classes.
    // This is because the key already exists in HKCU from the writes above.
    ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
    ASSERT_FALSE(RegKey::HasKey(hklm_key_name));
    ASSERT_TRUE(RegKey::HasKey(hkcu_key_name));
  } else {
    std::wcout << _T("\tPart of this test did not run because the user ")
                  _T("is not an admin.") << std::endl;
  }

  Cleanup();
}

TEST(GoopdateUtilsTest, GetImpersonationTokenNotInteractivePid0) {
  HANDLE token = NULL;
  ASSERT_SUCCEEDED(goopdate_utils::GetImpersonationToken(false, 0, &token));
  ASSERT_TRUE(token != NULL);
  ASSERT_TRUE(::CloseHandle(token));
  token = NULL;
}

TEST(GoopdateUtilsTest, GetImpersonationTokenNotInteractiveRealPid) {
  HANDLE token = NULL;
  DWORD pid = ::GetCurrentProcessId();
  ASSERT_SUCCEEDED(goopdate_utils::GetImpersonationToken(false, pid, &token));
  ASSERT_TRUE(token != NULL);
  ASSERT_TRUE(::CloseHandle(token));
  token = NULL;
}

TEST(GoopdateUtilsTest, GetImpersonationTokenInteractiveValidPid) {
  HANDLE token = NULL;
  DWORD pid = ::GetCurrentProcessId();
  ASSERT_SUCCEEDED(goopdate_utils::GetImpersonationToken(true, pid, &token));
  ASSERT_TRUE(token != NULL);
  ASSERT_TRUE(::CloseHandle(token));
}

TEST(GoopdateUtilsTest, GetImpersonationTokenInteractiveInvalidPid) {
  HANDLE token = NULL;
  ASSERT_EQ(E_INVALIDARG,
            goopdate_utils::GetImpersonationToken(true, 0, &token));
  ASSERT_TRUE(token == NULL);
}

TEST(GoopdateUtilsTest, ImpersonateUser) {
  DWORD pid = ::GetCurrentProcessId();

  if (vista_util::IsUserAdmin()) {
    ASSERT_SUCCEEDED(goopdate_utils::ImpersonateUser(false, 0));
    ASSERT_SUCCEEDED(goopdate_utils::UndoImpersonation(true));

    ASSERT_SUCCEEDED(goopdate_utils::ImpersonateUser(false, pid));
    ASSERT_SUCCEEDED(goopdate_utils::UndoImpersonation(true));

    ASSERT_SUCCEEDED(goopdate_utils::ImpersonateUser(true, pid));
    ASSERT_SUCCEEDED(goopdate_utils::UndoImpersonation(true));

    ASSERT_EQ(E_INVALIDARG, goopdate_utils::ImpersonateUser(true, 0));
  } else {
    std::wcout << _T("\tThis test did not run because the user ")
                  _T("is not an admin.") << std::endl;
  }
}

TEST(GoopdateUtilsTest, GoopdateTasks) {
  CString task_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                      _T("LongRunningSilent.exe"));
  // Install/uninstall.
  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path,
                                        vista_util::IsUserAdmin()));
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(vista_util::IsUserAdmin()));

  // Calling InstallGoopdateTask twice should succeed.
  for (int i = 0; i < 2; ++i) {
    EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path,
                                          vista_util::IsUserAdmin()));
  }

  // Start and stop.
  EXPECT_SUCCEEDED(StartGoopdateTaskCore(vista_util::IsUserAdmin()));
  HRESULT hr = E_FAIL;
  for (int tries = 0;
       tries < kMaxWaitForProcessIterations && SCHED_S_TASK_RUNNING != hr;
       ++tries) {
    ::Sleep(kWaitForProcessIntervalMs);
    hr = internal::GetScheduledTaskStatus(
        ConfigManager::GetCurrentTaskNameCore(vista_util::IsUserAdmin()));
  }
  EXPECT_EQ(SCHED_S_TASK_RUNNING, hr);

  // Without a wait in between the start and stop, stop failed intermittently.
  ::Sleep(500);

  EXPECT_SUCCEEDED(internal::StopScheduledTask(
      ConfigManager::GetCurrentTaskNameCore(vista_util::IsUserAdmin())));
  hr = SCHED_S_TASK_RUNNING;
  for (int tries = 0;
       tries < kMaxWaitForProcessIterations && SCHED_S_TASK_RUNNING == hr;
       ++tries) {
    ::Sleep(kWaitForProcessIntervalMs);
    hr = internal::GetScheduledTaskStatus(
        ConfigManager::GetCurrentTaskNameCore(vista_util::IsUserAdmin()));
  }
  EXPECT_NE(SCHED_S_TASK_RUNNING, hr);

  // Finally, uninstall.
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(vista_util::IsUserAdmin()));
}

TEST(GoopdateUtilsTest, GoopdateTaskInUseOverinstall) {
  CString task_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                      _T("LongRunningSilent.exe"));
  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path,
                                        vista_util::IsUserAdmin()));

  CString original_task_name(
      ConfigManager::GetCurrentTaskNameCore(vista_util::IsUserAdmin()));

  // Open the file underlying the current task in exclusive mode, so that
  // InstallGoopdateTasks() is forced to create a new task.
  CComPtr<ITaskScheduler> scheduler;
  EXPECT_SUCCEEDED(scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                              NULL,
                                              CLSCTX_INPROC_SERVER));
  CComPtr<ITask> task;
  EXPECT_SUCCEEDED(scheduler->Activate(original_task_name,
                                       __uuidof(ITask),
                                       reinterpret_cast<IUnknown**>(&task)));
  CComQIPtr<IPersistFile> persist(task);
  EXPECT_TRUE(persist);
  scoped_ptr_cotask<OLECHAR> job_file;
  EXPECT_SUCCEEDED(persist->GetCurFile(address(job_file)));
  persist.Release();

  File file;
  EXPECT_SUCCEEDED(file.OpenShareMode(job_file.get(), false, false, 0));

  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path,
                                        vista_util::IsUserAdmin()));
  CString new_task_name(
      ConfigManager::GetCurrentTaskNameCore(vista_util::IsUserAdmin()));
  EXPECT_STRNE(original_task_name, new_task_name);

  // Cleanup.
  file.Close();
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(vista_util::IsUserAdmin()));
}

TEST(GoopdateUtilsTest, GetOSInfo) {
  const TCHAR kXpOSVersion[] = _T("5.1");
  const TCHAR k2003OSVersion[] = _T("5.2");
  const TCHAR kVistaOSVersion[] = _T("6.0");
  const TCHAR kNoSp[] = _T("");
  const TCHAR kSp1[] = _T("Service Pack 1");
  const TCHAR kSp2[] = _T("Service Pack 2");
  const TCHAR kSp3[] = _T("Service Pack 3");

  CString os_version;
  CString service_pack;
  EXPECT_SUCCEEDED(goopdate_utils::GetOSInfo(&os_version, &service_pack));

  EXPECT_TRUE((kXpOSVersion == os_version && kSp2 == service_pack) ||
              (kXpOSVersion == os_version && kSp3 == service_pack) ||
              (k2003OSVersion == os_version && kSp1 == service_pack) ||
              (k2003OSVersion == os_version && kSp2 == service_pack) ||
              (kVistaOSVersion == os_version && kNoSp == service_pack) ||
              (kVistaOSVersion == os_version && kSp1 == service_pack));
}

class GoopdateUtilsRegistryProtectedTest : public testing::Test {
 protected:
  GoopdateUtilsRegistryProtectedTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  CString hive_override_key_name_;

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }
};

// Some methods used by goopdate_utils rely on registry entries that are
// overridden in the registry, so we need to write it.
class GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest
    : public GoopdateUtilsRegistryProtectedTest {
 protected:
  virtual void SetUp() {
    const TCHAR kWindowsCurrentVersionKeyPath[] =
        _T("HKLM\\Software\\Microsoft\\Windows\\CurrentVersion");
    const TCHAR kProgramFilesDirValueName[] = _T("ProgramFilesDir");
    const TCHAR kProgramFilesPath[] = _T("C:\\Program Files");

    GoopdateUtilsRegistryProtectedTest::SetUp();
    ASSERT_SUCCEEDED(RegKey::SetValue(kWindowsCurrentVersionKeyPath,
                                      kProgramFilesDirValueName,
                                      kProgramFilesPath));
  }
};

// Some methods used by goopdate_utils rely on registry entries that are
// overridden in the registry, so we need to write it.
class GoopdateUtilsRegistryProtectedWithUserFolderPathsTest
    : public GoopdateUtilsRegistryProtectedTest {
 protected:
  virtual void SetUp() {
  const TCHAR kUserShellKeyPath[] =
        _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\")
        _T("User Shell Folders");
    const TCHAR kLocalAppDataValueDirName[] = _T("Local AppData");
    const TCHAR kLocalAppDataPath[] =
        _T("%USERPROFILE%\\Local Settings\\Application Data");

    GoopdateUtilsRegistryProtectedTest::SetUp();
    ASSERT_SUCCEEDED(RegKey::SetValueExpandSZ(kUserShellKeyPath,
                                              kLocalAppDataValueDirName,
                                              kLocalAppDataPath));
  }
};

// pv should be ignored.
TEST_F(GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest,
       BuildGoogleUpdateExePath_MachineVersionFound) {
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  CString path = goopdate_utils::BuildGoogleUpdateExePath(true);
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  EXPECT_STREQ(program_files_path + _T("\\Google\\Update\\GoogleUpdate.exe"),
               path);
}

TEST_F(GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest,
       BuildGoogleUpdateExePath_MachineVersionNotFound) {
  // Test when the key doesn't exist.
  CString path = goopdate_utils::BuildGoogleUpdateExePath(true);
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  EXPECT_STREQ(program_files_path + _T("\\Google\\Update\\GoogleUpdate.exe"),
               path);

  // Test when the key exists but the value doesn't.
  ASSERT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_CLIENTS_GOOPDATE));
  path = goopdate_utils::BuildGoogleUpdateExePath(true);
  EXPECT_STREQ(program_files_path + _T("\\Google\\Update\\GoogleUpdate.exe"),
               path);
}

// pv should be ignored.
TEST_F(GoopdateUtilsRegistryProtectedWithUserFolderPathsTest,
       BuildGoogleUpdateExePath_UserVersionFound) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  CString path = goopdate_utils::BuildGoogleUpdateExePath(false);

  CString user_appdata;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_LOCAL_APPDATA, &user_appdata));
  CString expected_path;
  expected_path.Format(_T("%s\\Google\\Update\\GoogleUpdate.exe"),
                       user_appdata);
  EXPECT_STREQ(expected_path, path);
}

TEST_F(GoopdateUtilsRegistryProtectedWithUserFolderPathsTest,
       BuildGoogleUpdateExePath_UserVersionNotFound) {
  CString user_appdata;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_LOCAL_APPDATA, &user_appdata));
  CString expected_path;
  expected_path.Format(_T("%s\\Google\\Update\\GoogleUpdate.exe"),
                       user_appdata);

  // Test when the key doesn't exist.
  CString path = goopdate_utils::BuildGoogleUpdateExePath(false);
  EXPECT_STREQ(expected_path, path);

  // Test when the key exists but the value doesn't.
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENTS_GOOPDATE));
  path = goopdate_utils::BuildGoogleUpdateExePath(false);
  EXPECT_STREQ(expected_path, path);
}

// The version is no longer used by StartGoogleUpdateWithArgs, so the return
// value depends on whether program_files\Google\Update\GoogleUpdate.exe exists.
TEST_F(GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest,
       StartGoogleUpdateWithArgs_MachineVersionVersionDoesNotExist) {
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  const TCHAR* kArgs = _T("/foo");
  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(true, kArgs, NULL);
  EXPECT_TRUE(S_OK == hr || HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
}

// The version is no longer used by StartGoogleUpdateWithArgs, so the return
// value depends on whether <user_folder>\Google\Update\GoogleUpdate.exe exists.
// Also tests NULL args parameter
TEST_F(GoopdateUtilsRegistryProtectedWithUserFolderPathsTest,
       StartGoogleUpdateWithArgs_UserVersionVersionDoesNotExist) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(false, NULL, NULL);
  EXPECT_TRUE(S_OK == hr || HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetPersistentUserId_User) {
  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(USER_KEY_NAME,
                                                 kRegValueUserId,
                                                 &id));

  CString uid = goopdate_utils::GetPersistentUserId(USER_KEY_NAME);
  EXPECT_FALSE(uid.IsEmpty());

  EXPECT_SUCCEEDED(goopdate_utils::ReadPersistentId(USER_KEY_NAME,
                                                    kRegValueUserId,
                                                    &id));
  EXPECT_STREQ(uid, id);

  uid = goopdate_utils::GetPersistentUserId(USER_KEY_NAME);
  EXPECT_STREQ(uid, id);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetPersistentUserId_Machine) {
  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueUserId,
                                                 &id));

  CString uid = goopdate_utils::GetPersistentUserId(MACHINE_KEY_NAME);
  EXPECT_FALSE(uid.IsEmpty());

  EXPECT_SUCCEEDED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                    kRegValueUserId,
                                                    &id));
  EXPECT_STREQ(uid, id);

  uid = goopdate_utils::GetPersistentUserId(MACHINE_KEY_NAME);
  EXPECT_STREQ(uid, id);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetPersistentMachineId_Machine) {
  // Initially there is no machine id.
  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueMachineId,
                                                 &id));
  // Call the test method.
  CString mid = goopdate_utils::GetPersistentMachineId();
  EXPECT_FALSE(mid.IsEmpty());

  // Ensure that the value is written in the registry.
  EXPECT_SUCCEEDED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                    kRegValueMachineId,
                                                    &id));
  EXPECT_STREQ(mid, id);

  // Call the method again.
  mid = goopdate_utils::GetPersistentMachineId();

  // Ensure that the same id is returned.
  EXPECT_STREQ(mid, id);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetPersistentMachineId_User1) {
  // Initially there is no machine id.
  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueMachineId,
                                                 &id));
  SID* sid_val = NULL;
  ASSERT_TRUE(::ConvertStringSidToSid(_T("S-1-1-0"),
                                      reinterpret_cast<PSID*>(&sid_val)));
  ASSERT_TRUE(sid_val != NULL);

  // Create the update key.
  RegKey mid_key;
  EXPECT_SUCCEEDED(mid_key.Create(HKEY_LOCAL_MACHINE, GOOPDATE_MAIN_KEY));
  CDacl dacl;
  ASSERT_TRUE(AtlGetDacl(mid_key.Key(), SE_REGISTRY_KEY, &dacl));
  ASSERT_TRUE(dacl.AddDeniedAce(*sid_val, KEY_SET_VALUE));
  EXPECT_SUCCEEDED(mid_key.Close());

  // Now set the key permissions to not allow anyone to write.
  EXPECT_SUCCEEDED(mid_key.Open(HKEY_LOCAL_MACHINE, GOOPDATE_MAIN_KEY));
  ASSERT_TRUE(AtlSetDacl(mid_key.Key(), SE_REGISTRY_KEY, dacl, 0));

  // Call the test method.
  CString mid = goopdate_utils::GetPersistentMachineId();
  ASSERT_TRUE(mid.IsEmpty());

  // Ensure that the value Could not be written to the registry.
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueMachineId,
                                                 &id));
  ASSERT_TRUE(id.IsEmpty());

  ASSERT_TRUE(dacl.RemoveAces(*sid_val));
  ASSERT_TRUE(AtlSetDacl(mid_key.Key(), SE_REGISTRY_KEY, dacl, 0));
  EXPECT_SUCCEEDED(mid_key.Close());
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetPersistentMachineId_User2) {
  // Test simulates the situation where a machine goopdate is already running
  // and a user goopdate is run next.

  // Initially there is no machine id.
  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueMachineId,
                                                 &id));

  CString mid = goopdate_utils::GetPersistentMachineId();
  EXPECT_FALSE(mid.IsEmpty());

  // Ensure that the value Could not be written to the registry.
  EXPECT_SUCCEEDED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                kRegValueMachineId,
                                                &id));
  EXPECT_STREQ(id, mid);

  // Now set the permissions on the registry key such that we cannot write to
  // it. We should however be able to read from it.
  SID* sid_val = NULL;
  ASSERT_TRUE(::ConvertStringSidToSid(_T("S-1-1-0"),
                                      reinterpret_cast<PSID*>(&sid_val)));
  ASSERT_TRUE(sid_val != NULL);

  RegKey mid_key;
  EXPECT_SUCCEEDED(mid_key.Open(HKEY_LOCAL_MACHINE, GOOPDATE_MAIN_KEY));
  CDacl dacl;
  ASSERT_TRUE(AtlGetDacl(mid_key.Key(), SE_REGISTRY_KEY, &dacl));
  ASSERT_TRUE(dacl.AddDeniedAce(*sid_val, KEY_SET_VALUE));
  ASSERT_TRUE(AtlSetDacl(mid_key.Key(), SE_REGISTRY_KEY, dacl, 0));

  // Call the test method.
  mid = goopdate_utils::GetPersistentMachineId();
  EXPECT_FALSE(mid.IsEmpty());
  EXPECT_STREQ(mid, id);

  ASSERT_TRUE(dacl.RemoveAces(*sid_val));
  ASSERT_TRUE(AtlSetDacl(mid_key.Key(), SE_REGISTRY_KEY, dacl, 0));
  EXPECT_SUCCEEDED(mid_key.Close());
}

// New Unique ID is created and stored.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       GetPersistentUserId_OemInstalling_User) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));
  ASSERT_TRUE(ConfigManager::Instance()->IsOemInstalling(true));

  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(USER_KEY_NAME,
                                                 kRegValueUserId,
                                                 &id));

  CString uid = goopdate_utils::GetPersistentUserId(USER_KEY_NAME);
  EXPECT_FALSE(uid.IsEmpty());

  EXPECT_SUCCEEDED(goopdate_utils::ReadPersistentId(USER_KEY_NAME,
                                                    kRegValueUserId,
                                                    &id));
  EXPECT_STREQ(uid, id);

  uid = goopdate_utils::GetPersistentUserId(USER_KEY_NAME);
  EXPECT_STREQ(uid, id);
}

// Special value is returned but not stored.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       GetPersistentUserId_OemInstalling_Machine) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));
  ASSERT_TRUE(ConfigManager::Instance()->IsOemInstalling(true));

  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueUserId,
                                                 &id));

  CString uid = goopdate_utils::GetPersistentUserId(MACHINE_KEY_NAME);
  EXPECT_STREQ(_T("{00000000-03AA-03AA-03AA-000000000000}"), uid);

  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueUserId,
                                                 &id));

  uid = goopdate_utils::GetPersistentUserId(MACHINE_KEY_NAME);
  EXPECT_STREQ(_T("{00000000-03AA-03AA-03AA-000000000000}"), uid);
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueUserId));

  // Test the case where the value already exists.
  EXPECT_SUCCEEDED(RegKey::SetValue(
      MACHINE_REG_UPDATE,
      kRegValueUserId,
      _T("{12345678-1234-1234-1234-123456789abcd}")));
  uid = goopdate_utils::GetPersistentUserId(MACHINE_KEY_NAME);
  EXPECT_STREQ(_T("{00000000-03AA-03AA-03AA-000000000000}"), uid);
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueUserId));
}

// Special value is returned but not stored.
// There is no difference between user and machine instances.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       GetPersistentMachineId_OemInstalling_Machine) {
  const DWORD now_seconds = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now_seconds));
  ASSERT_TRUE(ConfigManager::Instance()->IsOemInstalling(true));

  CString id;
  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueMachineId,
                                                 &id));

  CString mid = goopdate_utils::GetPersistentMachineId();
  EXPECT_STREQ(_T("{00000000-03AA-03AA-03AA-000000000000}"), mid);

  EXPECT_FAILED(goopdate_utils::ReadPersistentId(MACHINE_KEY_NAME,
                                                 kRegValueMachineId,
                                                 &id));

  mid = goopdate_utils::GetPersistentMachineId();
  EXPECT_STREQ(_T("{00000000-03AA-03AA-03AA-000000000000}"), mid);
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueMachineId));

  // Test the case where the value already exists.
  EXPECT_SUCCEEDED(RegKey::SetValue(
      MACHINE_REG_UPDATE,
      kRegValueMachineId,
      _T("{12345678-1234-1234-1234-123456789abcd}")));
  mid = goopdate_utils::GetPersistentMachineId();
  EXPECT_STREQ(_T("{00000000-03AA-03AA-03AA-000000000000}"), mid);
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueMachineId));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_NoKey) {
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStateMediumPath));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
    IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that user values are not used.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium does not override ClientState.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_NoKey) {
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStateMediumPath));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that user values are not used.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium does not override ClientState.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium is not supported for user apps.

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_NoKey) {
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStateMediumPath));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that machine values are not used.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium is not used.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_NoKey) {
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_NoKey) {
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateZero_ClientStateMediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that user values are not affected.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_NoKey) {
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateZero_ClientStateMediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that machine values are not affected.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_NoKey) {
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

// Also tests that user values are not affected.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateNone_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_NoKey) {
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

// Also tests that machine values are not affected.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateNone_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(goopdate_utils::ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_NoKey) {
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStateMediumPath));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

// ClientStateMedium overrides ClientState.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
    AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

// ClientStateMedium overrides ClientState.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

// User does not affect machine.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_UserOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

// ClientStateMedium is not supported for user apps.

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_NoKey) {
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStateMediumPath));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

// ClientStateMedium is not used.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

// ClientStateMedium is not used.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

// Machine does not affect user.
TEST_F(GoopdateUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_MachineOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(goopdate_utils::AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetUsageStatsEnable_VerifyLegacyLocationNotSet) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  ASSERT_TRUE(RegKey::HasKey(MACHINE_REG_UPDATE));
  ASSERT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE,
                                kLegacyRegValueCollectUsageStats));
  ASSERT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetUsageStatsEnable_Machine_Off) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetUsageStatsEnable_User_Off) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(false, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppUserClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppUserClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppMachineClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetUsageStatsEnable_Machine_On) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetUsageStatsEnable_User_On) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(false, kAppGuid, TRISTATE_TRUE));

  ASSERT_TRUE(RegKey::HasKey(kAppUserClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppUserClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppMachineClientStatePath));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}


TEST_F(GoopdateUtilsRegistryProtectedTest, SetUsageStatsEnable_Machine_None) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));
  ASSERT_FALSE(RegKey::HasKey(MACHINE_REG_UPDATE));
  ASSERT_FALSE(RegKey::HasKey(USER_REG_UPDATE));

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(false, kAppGuid, TRISTATE_NONE));
  ASSERT_FALSE(RegKey::HasKey(USER_REG_UPDATE));
  ASSERT_FALSE(RegKey::HasKey(MACHINE_REG_UPDATE));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_Overwrite) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_NoneDoesNotOverwrite) {
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_ClientStateMediumCleared) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));

  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_NoneDoesNotClearClientStateMedium) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetUsageStatsEnable_User_ClientStateMediumNotCleared) {
  // User and machine values should not be cleared.
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));

  // True does not clear them.
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(false, kAppGuid, TRISTATE_TRUE));
  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
  enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  // False does not clear them.
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(
      goopdate_utils::SetUsageStatsEnable(false, kAppGuid, TRISTATE_FALSE));
  enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
  enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, ConvertLegacyUsageStats_NotPresent) {
  EXPECT_SUCCEEDED(goopdate_utils::ConvertLegacyUsageStats(true));
  ASSERT_FALSE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, ConvertLegacyUsageStats_Present) {
  DWORD val = 1;
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kLegacyRegValueCollectUsageStats,
                                    val));

  EXPECT_SUCCEEDED(goopdate_utils::ConvertLegacyUsageStats(true));

  ASSERT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE_GOOPDATE));
  ASSERT_TRUE(RegKey::HasValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                               _T("usagestats")));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);

  ASSERT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE,
                               kLegacyRegValueCollectUsageStats));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetClientsStringValueFromRegistry) {
  CString value;

  // Test IsMachine = true
  const TCHAR* dummy_machine_clients_key = MACHINE_REG_CLIENTS DUMMY_CLSID;

  ASSERT_FALSE(RegKey::HasKey(dummy_machine_clients_key));
  EXPECT_FAILED(goopdate_utils::GetClientsStringValueFromRegistry(
                                    true, DUMMY_CLSID, _T("name"), &value));
  ASSERT_SUCCEEDED(RegKey::DeleteKey(dummy_machine_clients_key));

  ASSERT_SUCCEEDED(RegKey::SetValue(dummy_machine_clients_key,
                                    _T("name"),
                                    _T("dummy")));
  EXPECT_SUCCEEDED(goopdate_utils::GetClientsStringValueFromRegistry(
                                      true, DUMMY_CLSID, _T("name"), &value));
  EXPECT_EQ(_T("dummy"), value);

  // Test IsMachine = false
  const TCHAR* dummy_user_clients_key = USER_REG_CLIENTS DUMMY_CLSID;

  ASSERT_FALSE(RegKey::HasKey(dummy_user_clients_key));
  EXPECT_FAILED(goopdate_utils::GetClientsStringValueFromRegistry(
                                   false, DUMMY_CLSID, _T("name"), &value));

  ASSERT_SUCCEEDED(RegKey::SetValue(dummy_user_clients_key,
                                    _T("name"),
                                    _T("dummy2")));
  EXPECT_SUCCEEDED(goopdate_utils::GetClientsStringValueFromRegistry(
                                      false, DUMMY_CLSID, _T("name"), &value));
  EXPECT_EQ(_T("dummy2"), value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetVerFromRegistry) {
  CString version;

  // Test IsMachine = true
  const TCHAR* dummy_machine_clients_key = MACHINE_REG_CLIENTS DUMMY_CLSID;

  ASSERT_FALSE(RegKey::HasKey(dummy_machine_clients_key));
  EXPECT_FAILED(goopdate_utils::GetVerFromRegistry(true,
                                                   DUMMY_CLSID,
                                                   &version));
  ASSERT_SUCCEEDED(RegKey::DeleteKey(dummy_machine_clients_key));

  ASSERT_SUCCEEDED(RegKey::SetValue(dummy_machine_clients_key,
                                    _T("pv"),
                                    _T("1.0.101.0")));
  EXPECT_SUCCEEDED(goopdate_utils::GetVerFromRegistry(true,
                                                      DUMMY_CLSID,
                                                      &version));
  EXPECT_EQ(_T("1.0.101.0"), version);

  // Test IsMachine = false
  const TCHAR* dummy_user_clients_key = USER_REG_CLIENTS DUMMY_CLSID;

  ASSERT_FALSE(RegKey::HasKey(dummy_user_clients_key));
  EXPECT_FAILED(goopdate_utils::GetVerFromRegistry(false,
                                                   DUMMY_CLSID,
                                                   &version));

  ASSERT_SUCCEEDED(RegKey::SetValue(dummy_user_clients_key,
                                    _T("pv"),
                                    _T("1.0.102.0")));
  EXPECT_SUCCEEDED(goopdate_utils::GetVerFromRegistry(false,
                                                      DUMMY_CLSID,
                                                      &version));
  EXPECT_EQ(_T("1.0.102.0"), version);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       BuildHttpGetString_MachineNoTestSource) {
  ASSERT_SUCCEEDED(RegKey::SetValue(
                       MACHINE_REG_UPDATE,
                       kRegValueMachineId,
                       _T("{39980C99-CDD5-43A0-93C7-69D90C124729}")));
  ASSERT_SUCCEEDED(RegKey::SetValue(
                       MACHINE_REG_UPDATE,
                       kRegValueUserId,
                       _T("{75521B9F-3EA4-49E8-BC61-E6BDCFEDF6F6}")));
  ASSERT_SUCCEEDED(RegKey::SetValue(
                       MACHINE_REG_CLIENT_STATE APP_GUID,
                       kRegValueInstallationId,
                       _T("{0F973A20-C484-462b-952C-5D9A459E3326}")));

  CString expected_str_before_os(
      _T("http://www.google.com/hello.py?code=123&")
      _T("hl=en&errorcode=0x0000000a&extracode1=0x00000016&extracode2=0&")
      _T("app=%7BB7BAF788-9D64-49c3-AFDC-B336AB12F332%7D&")
      _T("guver=1.0.51.0&ismachine=1&os="));
  CString expected_str_after_os(
      _T("&mid=%7B39980C99-CDD5-43A0-93C7-69D90C124729%7D")
      _T("&uid=%7B75521B9F-3EA4-49E8-BC61-E6BDCFEDF6F6%7D")
      _T("&iid=%7B0F973A20-C484-462b-952C-5D9A459E3326%7D&source=click"));
  bool expected_test_source = false;

#if defined(DEBUG) || !OFFICIAL_BUILD
  // TestSource is always set for these builds. It may be set for opt official
  // builds but this is not guaranteed.
  expected_str_after_os.Append(_T("&testsource="));
  expected_test_source = true;
#endif

  CString url_req;
  EXPECT_SUCCEEDED(goopdate_utils::BuildHttpGetString(
      _T("http://www.google.com/hello.py?code=123&"),
      10,
      22,
      0,
      APP_GUID,
      _T("1.0.51.0"),
      true,
      _T("en"),
      _T("click"),
      &url_req));
  EXPECT_LE(expected_str_before_os.GetLength(), url_req.GetLength());
  EXPECT_EQ(0, url_req.Find(expected_str_before_os));

  int os_fragment_len = 0;
  EXPECT_EQ(expected_str_before_os.GetLength(),
            VerifyOSInUrl(url_req, &os_fragment_len));

  EXPECT_EQ(expected_str_before_os.GetLength() + os_fragment_len,
            url_req.Find(expected_str_after_os));

  if (expected_test_source) {
    CString expected_testsource_str =
        ConfigManager::Instance()->GetTestSource();
    int expected_testsource_start = expected_str_before_os.GetLength() +
                                    os_fragment_len +
                                    expected_str_after_os.GetLength();
    EXPECT_EQ(expected_testsource_start, url_req.Find(expected_testsource_str));
    EXPECT_EQ(expected_testsource_start + expected_testsource_str.GetLength(),
              url_req.GetLength());
  } else {
    EXPECT_EQ(expected_str_before_os.GetLength() +
              os_fragment_len +
              expected_str_after_os.GetLength(),
              url_req.GetLength());

    EXPECT_EQ(-1, url_req.Find(_T("testsource")));
  }
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       BuildHttpGetString_UserWithTestSource) {
  #define APP_GUID _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}")

  ASSERT_SUCCEEDED(RegKey::SetValue(
                       MACHINE_REG_UPDATE,
                       kRegValueMachineId,
                       _T("{39980C99-CDD5-43A0-93C7-69D90C124729}")));
  ASSERT_SUCCEEDED(RegKey::SetValue(
                       USER_REG_UPDATE,
                       kRegValueUserId,
                       _T("{75521B9F-3EA4-49E8-BC61-E6BDCFEDF6F6}")));
  ASSERT_SUCCEEDED(RegKey::SetValue(
                       USER_REG_CLIENT_STATE APP_GUID,
                       kRegValueInstallationId,
                       _T("{0F973A20-C484-462b-952C-5D9A459E3326}")));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueTestSource,
                                    _T("dev")));

  const CString expected_str_before_os(
      _T("http://www.google.com/hello.py?")
      _T("hl=de&errorcode=0xffffffff&extracode1=0x00000000&extracode2=99&")
      _T("app=%7BB7BAF788-9D64-49c3-AFDC-B336AB12F332%7D&")
      _T("guver=foo%20bar&ismachine=0&os="));
  const CString expected_str_after_os(
      _T("&mid=%7B39980C99-CDD5-43A0-93C7-69D90C124729%7D")
      _T("&uid=%7B75521B9F-3EA4-49E8-BC61-E6BDCFEDF6F6%7D")
      _T("&iid=%7B0F973A20-C484-462b-952C-5D9A459E3326%7D&source=clack")
      _T("&testsource="));

  CString url_req;
  EXPECT_SUCCEEDED(goopdate_utils::BuildHttpGetString(
      _T("http://www.google.com/hello.py?"),
      0xffffffff,
      0,
      99,
      APP_GUID,
      _T("foo bar"),
      false,
      _T("de"),
      _T("clack"),
      &url_req));
  EXPECT_LE(expected_str_before_os.GetLength(), url_req.GetLength());
  EXPECT_EQ(0, url_req.Find(expected_str_before_os));

  int os_fragment_len = 0;
  EXPECT_EQ(expected_str_before_os.GetLength(),
            VerifyOSInUrl(url_req, &os_fragment_len));

  EXPECT_EQ(expected_str_before_os.GetLength() + os_fragment_len,
            url_req.Find(expected_str_after_os));

  const CString expected_testsource_str = _T("dev");

  int expected_testsource_start = expected_str_before_os.GetLength() +
                                  os_fragment_len +
                                  expected_str_after_os.GetLength();
  EXPECT_EQ(expected_testsource_start, url_req.Find(expected_testsource_str));
  EXPECT_EQ(expected_testsource_start + expected_testsource_str.GetLength(),
            url_req.GetLength());
}

// MID and UID automatically get generated when they are read, so it is not
// possible for them to be empty. IID is emtpy if not present.
TEST_F(GoopdateUtilsRegistryProtectedTest, BuildHttpGetString_NoMidUidIid) {
  CString url_req;
  EXPECT_SUCCEEDED(goopdate_utils::BuildHttpGetString(
      _T("http://www.google.com/hello.py?"),
      0xffffffff,
      0,
      99,
      _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}"),
      _T("foo bar"),
      true,
      _T("en"),
      _T("cluck"),
      &url_req));

  // Check for the GUID brackets and end of string as appropriate.
  EXPECT_NE(-1, url_req.Find(_T("&mid=%7B")));
  EXPECT_NE(-1, url_req.Find(_T("%7D&uid=%7B")));

  CString expected_test_src;
#if defined(DEBUG) || !OFFICIAL_BUILD
  expected_test_src = _T("&testsource=auto");
#endif
  const CString expected_iid_str(_T("%7D&iid=&source=cluck"));
  EXPECT_EQ(url_req.GetLength() -
            expected_iid_str.GetLength() -
            expected_test_src.GetLength(),
            url_req.Find(expected_iid_str));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetNumClients) {
  size_t num_clients(0);

  // Fails when no "Clients" key.
  EXPECT_HRESULT_FAILED(GetNumClients(true, &num_clients));
  EXPECT_HRESULT_FAILED(GetNumClients(false, &num_clients));

  // Tests no subkeys.
  const TCHAR* keys_to_create[] = { MACHINE_REG_CLIENTS, USER_REG_CLIENTS };
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKeys(keys_to_create,
                                              arraysize(keys_to_create)));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(true, &num_clients));
  EXPECT_EQ(0, num_clients);
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(false, &num_clients));
  EXPECT_EQ(0, num_clients);

  // Subkeys should be counted. Values should not be counted.
  RegKey machine_key;
  EXPECT_HRESULT_SUCCEEDED(machine_key.Open(HKEY_LOCAL_MACHINE,
                                            GOOPDATE_REG_RELATIVE_CLIENTS));
  EXPECT_HRESULT_SUCCEEDED(machine_key.SetValue(_T("name"), _T("value")));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(true, &num_clients));
  EXPECT_EQ(0, num_clients);

  const TCHAR* app_id = _T("{AA5523E3-40C0-4b85-B074-4BBA09559CCD}");
  EXPECT_HRESULT_SUCCEEDED(machine_key.Create(machine_key.Key(), app_id));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(true, &num_clients));
  EXPECT_EQ(1, num_clients);

  // Tests user scenario.
  RegKey user_key;
  EXPECT_HRESULT_SUCCEEDED(user_key.Open(HKEY_CURRENT_USER,
                                         GOOPDATE_REG_RELATIVE_CLIENTS));
  EXPECT_HRESULT_SUCCEEDED(user_key.SetValue(_T("name"), _T("value")));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(false, &num_clients));
  EXPECT_EQ(0, num_clients);

  EXPECT_HRESULT_SUCCEEDED(user_key.Create(user_key.Key(), app_id));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(false, &num_clients));
  EXPECT_EQ(1, num_clients);
}

TEST(GoopdateUtilsTest, BuildInstallDirectory_Machine) {
  const CPath dir = goopdate_utils::BuildInstallDirectory(true, _T("1.2.3.0"));
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  EXPECT_STREQ(program_files_path + _T("\\Google\\Update\\1.2.3.0"), dir);
}

TEST(GoopdateUtilsTest, BuildInstallDirectory_User) {
  CPath expected_path(GetGoogleUpdateUserPath());
  expected_path.Append(_T("4.5.6.7"));
  EXPECT_STREQ(expected_path,
               goopdate_utils::BuildInstallDirectory(false, _T("4.5.6.7")));
}

TEST(GoopdateUtilsTest, ConvertBrowserTypeToString) {
  for (int i = 0; i < BROWSER_MAX; ++i) {
    CString str_type = goopdate_utils::ConvertBrowserTypeToString(
        static_cast<BrowserType>(i));
    BrowserType type = BROWSER_UNKNOWN;
    ASSERT_HRESULT_SUCCEEDED(
        goopdate_utils::ConvertStringToBrowserType(str_type, &type));
    ASSERT_EQ(static_cast<int>(type), i);
  }
}

void CompareArgsAndUpdateResponseData(const UpdateResponseData& response_data,
                                      const CString& app_name,
                                      const CommandLineExtraArgs& args) {
  ASSERT_STREQ(GuidToString(response_data.guid()),
               GuidToString(args.apps[0].app_guid));
  ASSERT_STREQ(app_name, args.apps[0].app_name);
  ASSERT_EQ(response_data.needs_admin() == NEEDS_ADMIN_YES ? true : false,
            args.apps[0].needs_admin);
  ASSERT_STREQ(response_data.ap(), args.apps[0].ap);
  ASSERT_STREQ(response_data.tt_token(), args.apps[0].tt_token);

  ASSERT_EQ(GuidToString(response_data.installation_id()),
            GuidToString(args.installation_id));
  ASSERT_TRUE(args.brand_code.IsEmpty());
  ASSERT_TRUE(args.client_id.IsEmpty());
  ASSERT_TRUE(args.referral_id.IsEmpty());
  ASSERT_EQ(response_data.browser_type(), args.browser_type);
  ASSERT_EQ(TRISTATE_NONE, args.usage_stats_enable);
}

TEST(GoopdateUtilsTest, ConvertResponseDataToExtraArgsRequired) {
  // These unit tests create an update response and then call the test method,
  // to create the command line. Next the command line is parsed using the
  // command line class and the results are validated.
  UpdateResponseData input;
  input.set_guid(StringToGuid(_T("{8B59B82E-5543-4807-8590-84BF484AE2F6}")));

  CString unicode_name;
  ASSERT_TRUE(unicode_name.LoadString(IDS_ESCAPE_TEST));
  CString encoded_name;
  ASSERT_HRESULT_SUCCEEDED(WideStringToUtf8UrlEncodedString(unicode_name,
                                                            &encoded_name));
  input.set_app_name(encoded_name);
  input.set_needs_admin(NEEDS_ADMIN_YES);

  CString extra_args;
  ASSERT_HRESULT_SUCCEEDED(
      goopdate_utils::ConvertResponseDataToExtraArgs(input, &extra_args));

  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  ASSERT_HRESULT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CompareArgsAndUpdateResponseData(input, unicode_name, args);
}


TEST(GoopdateUtilsTest, ConvertResponseDataToExtraArgsAll) {
  UpdateResponseData input;
  input.set_guid(StringToGuid(_T("{8B59B82E-5543-4807-8590-84BF484AE2F6}")));

  CString unicode_name;
  ASSERT_TRUE(unicode_name.LoadString(IDS_ESCAPE_TEST));
  CString encoded_name;
  ASSERT_HRESULT_SUCCEEDED(WideStringToUtf8UrlEncodedString(unicode_name,
                                                            &encoded_name));
  input.set_app_name(encoded_name);
  input.set_needs_admin(NEEDS_ADMIN_YES);
  input.set_installation_id(
      StringToGuid(_T("{E314A405-FCC5-4ed1-BFA4-CBC22F1873BF}")));
  input.set_ap(_T("Test_ap"));
  input.set_tt_token(_T("Test_tt_token"));
  input.set_browser_type(BROWSER_IE);

  CString extra_args;
  ASSERT_HRESULT_SUCCEEDED(
      goopdate_utils::ConvertResponseDataToExtraArgs(input, &extra_args));

  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  ASSERT_HRESULT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CompareArgsAndUpdateResponseData(input, unicode_name, args);
}

TEST(GoopdateUtilsTest, UniqueEventInEnvironment_User) {
  const TCHAR* kEnvVarName = _T("SOME_ENV_VAR_FOR_TEST");
  scoped_event created_event;
  scoped_event opened_event;

  ASSERT_HRESULT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kEnvVarName,
      false,
      address(created_event)));
  ASSERT_TRUE(created_event);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(created_event), 0));

  TCHAR event_name[MAX_PATH] = {0};
  EXPECT_TRUE(
      ::GetEnvironmentVariable(kEnvVarName, event_name, arraysize(event_name)));

  ASSERT_HRESULT_SUCCEEDED(goopdate_utils::OpenUniqueEventFromEnvironment(
      kEnvVarName,
      false,
      address(opened_event)));
  ASSERT_TRUE(opened_event);

  EXPECT_TRUE(::SetEvent(get(opened_event)));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(created_event), 0));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}

TEST(GoopdateUtilsTest, UniqueEventInEnvironment_Machine) {
  const TCHAR* kEnvVarName = _T("OTHER_ENV_VAR_FOR_TEST");
  scoped_event created_event;
  scoped_event opened_event;
  TCHAR event_name[MAX_PATH] = {0};

  if (!vista_util::IsUserAdmin()) {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_OWNER),
              goopdate_utils::CreateUniqueEventInEnvironment(
                  kEnvVarName,
                  true,
                  address(created_event)));
    EXPECT_FALSE(created_event);

    EXPECT_FALSE(::GetEnvironmentVariable(kEnvVarName,
                                          event_name,
                                          arraysize(event_name)));
    return;
  }

  ASSERT_HRESULT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kEnvVarName,
      true,
      address(created_event)));
  ASSERT_TRUE(created_event);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(created_event), 0));

  EXPECT_TRUE(
      ::GetEnvironmentVariable(kEnvVarName, event_name, arraysize(event_name)));

  ASSERT_HRESULT_SUCCEEDED(goopdate_utils::OpenUniqueEventFromEnvironment(
      kEnvVarName,
      true,
      address(opened_event)));
  ASSERT_TRUE(opened_event);

  EXPECT_TRUE(::SetEvent(get(opened_event)));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(created_event), 0));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}

TEST(GoopdateUtilsTest, UniqueEventInEnvironment_UserMachineMismatch) {
  const TCHAR* kEnvVarName = _T("ENV_VAR_FOR_MIXED_TEST");
  scoped_event created_event;
  scoped_event opened_event;

  ASSERT_HRESULT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kEnvVarName,
      false,
      address(created_event)));
  ASSERT_TRUE(created_event);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(created_event), 0));

  TCHAR event_name[MAX_PATH] = {0};
  EXPECT_TRUE(
      ::GetEnvironmentVariable(kEnvVarName, event_name, arraysize(event_name)));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            goopdate_utils::OpenUniqueEventFromEnvironment(
                kEnvVarName,
                true,
                address(opened_event)));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}

TEST(GoopdateUtilsTest, OpenUniqueEventFromEnvironment_EnvVarDoesNotExist) {
  const TCHAR* kEnvVarName = _T("ANOTHER_ENV_VAR_FOR_TEST");
  scoped_event opened_event;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND),
            goopdate_utils::OpenUniqueEventFromEnvironment(
                kEnvVarName,
                false,
                address(opened_event)));
}

TEST(GoopdateUtilsTest, OpenUniqueEventFromEnvironment_EventDoesNotExist) {
  const TCHAR* kEnvVarName = _T("YET_ANOTHER_ENV_VAR_FOR_TEST");

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, _T("foo")));

  scoped_event opened_event;
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              goopdate_utils::OpenUniqueEventFromEnvironment(
                  kEnvVarName,
                  false,
                  address(opened_event)));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}


CString GetTempFile() {
  TCHAR temp_path[MAX_PATH] = {0};
  TCHAR temp_file[MAX_PATH] = {0};

  EXPECT_LT(::GetTempPath(arraysize(temp_path), temp_path),
            arraysize(temp_path));
  EXPECT_NE(0, ::GetTempFileName(temp_path, _T("ut_"), 0, temp_file));
  return CString(temp_file);
}

typedef std::map<CString, CString> StringMap;
typedef StringMap::const_iterator StringMapIter;

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_MissingFile) {
  CString temp_file = GetTempFile();
  ::DeleteFile(temp_file);

  ASSERT_FALSE(File::Exists(temp_file));

  StringMap pairs_read;
  ASSERT_FAILED(goopdate_utils::ReadNameValuePairsFromFile(temp_file,
                                                           _T("my_group"),
                                                           &pairs_read));
  ASSERT_EQ(0, pairs_read.size());
}

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_ReadEmpty) {
  CString temp_file = GetTempFile();
  File file_write;
  EXPECT_SUCCEEDED(file_write.Open(temp_file, true, false));
  file_write.Close();

  StringMap pairs_read;
  ASSERT_SUCCEEDED(goopdate_utils::ReadNameValuePairsFromFile(temp_file,
                                                              _T("my_group"),
                                                              &pairs_read));
  ASSERT_EQ(0, pairs_read.size());
}

void ValidateStringMapEquality(const StringMap& expected,
                               const StringMap& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  StringMapIter it_expected = expected.begin();
  for (; it_expected != expected.end(); ++it_expected) {
    StringMapIter it_actual = actual.find(it_expected->first);
    ASSERT_TRUE(it_actual != actual.end());
    ASSERT_STREQ(it_expected->second, it_actual->second);
  }
}

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_ReadOnePair) {
  CString group = _T("my_group");

  StringMap pairs_write;
  pairs_write[_T("some_name")] = _T("some_value");

  CString temp_file = GetTempFile();
  ASSERT_SUCCEEDED(goopdate_utils::WriteNameValuePairsToFile(temp_file,
                                                             group,
                                                             pairs_write));
  ASSERT_TRUE(File::Exists(temp_file));

  StringMap pairs_read;
  ASSERT_SUCCEEDED(goopdate_utils::ReadNameValuePairsFromFile(temp_file,
                                                              group,
                                                              &pairs_read));

  ValidateStringMapEquality(pairs_write, pairs_read);
}

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_ReadManyPairs) {
  CString group = _T("my_group");

  StringMap pairs_write;
  const int kCountPairs = 10;
  for (int i = 1; i <= kCountPairs; ++i) {
    CString name;
    name.Format(_T("name%d"), i);
    CString value;
    value.Format(_T("value%d"), i);
    pairs_write[name] = value;
  }

  CString temp_file = GetTempFile();
  ASSERT_SUCCEEDED(goopdate_utils::WriteNameValuePairsToFile(temp_file,
                                                             group,
                                                             pairs_write));
  ASSERT_TRUE(File::Exists(temp_file));

  StringMap pairs_read;
  ASSERT_SUCCEEDED(goopdate_utils::ReadNameValuePairsFromFile(temp_file,
                                                              group,
                                                              &pairs_read));

  ValidateStringMapEquality(pairs_write, pairs_read);
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetAppBranding_KeyDoesNotExist) {
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                           _T("ABCD"),
                                           _T("some_partner"),
                                           _T("referrer")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetAppBranding_AllEmpty) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetAppBranding_BrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCD"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetAppBranding_BrandCodeTooLong) {
  EXPECT_EQ(E_INVALIDARG, goopdate_utils::SetAppBranding(
                                              kAppMachineClientStatePath,
                                              _T("CHMGon.href)}"),
                                              _T(""),
                                              _T("")));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetAppBranding_ClientIdOnly) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, SetAppBranding_AllValid) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T("some_partner"),
                                                  _T("referrer")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCD"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(_T("referrer"), value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsAllEmpty) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsBrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_ExistingBrandTooLong) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("CHMG4CUTNt")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("CHMG"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsCliendIdOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsBothValid) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsAllEmtpy) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsBrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCE"),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCE"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsCliendIdOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsBothValid) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCD"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_AllAlreadyExistAllEmpty) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    __T("existingreferrerid")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(__T("existingreferrerid"), value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_AllAlreadyExistBrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    __T("existingreferrerid")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T(""),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(__T("existingreferrerid"), value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_BothAlreadyExistCliendIdOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    __T("existingreferrerid")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T(""),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(__T("existingreferrerid"), value);
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       SetAppBranding_BothAlreadyExistBothValid) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    _T("existingreferrerid")));

  EXPECT_SUCCEEDED(goopdate_utils::SetAppBranding(kAppMachineClientStatePath,
                                                  _T("ABCD"),
                                                  _T("some_partner"),
                                                  _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(_T("existingreferrerid"), value);
}

//
// IsMachineProcess tests.
//

class GoopdateUtilsIsMachineProcessTest : public testing::Test {
 protected:
  bool FromMachineDirHelper(CommandLineMode mode) {
    return goopdate_utils::IsMachineProcess(mode,
                                            true,
                                            false,
                                            false,
                                            TRISTATE_NONE);
  }

  bool IsLocalSystemHelper(CommandLineMode mode) {
    return goopdate_utils::IsMachineProcess(mode,
                                            false,
                                            true,
                                            false,
                                            TRISTATE_NONE);
  }

  bool MachineOverrideHelper(CommandLineMode mode) {
    return goopdate_utils::IsMachineProcess(mode,
                                            false,
                                            false,
                                            true,
                                            TRISTATE_NONE);
  }

  bool NeedsAdminFalseHelper(CommandLineMode mode) {
    return goopdate_utils::IsMachineProcess(mode,
                                            false,
                                            false,
                                            false,
                                            TRISTATE_FALSE);
  }

  bool NeedsAdminTrueHelper(CommandLineMode mode) {
    return goopdate_utils::IsMachineProcess(mode,
                                            false,
                                            false,
                                            false,
                                            TRISTATE_TRUE);
  }
};

TEST_F(GoopdateUtilsIsMachineProcessTest,
       IsMachineProcess_MachineDirOnly) {
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_NETDIAGS));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_CRASH));
  // TODO(omaha): Change to machine.
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_REPORTCRASH));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_INSTALL));
  }
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UPDATE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_IG));
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  }
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UG));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_WEBPLUGIN));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_LEGACYUI));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  }
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  }
  EXPECT_TRUE(FromMachineDirHelper(
      static_cast<CommandLineMode>(
          COMMANDLINE_MODE_CRASH_HANDLER + 1)));
}

TEST_F(GoopdateUtilsIsMachineProcessTest,
       IsMachineProcess_IsLocalSystemOnly) {
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_CORE));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_SERVICE));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_NETDIAGS));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_REPORTCRASH));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_INSTALL));
  }
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_UPDATE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_IG));
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  }
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_UG));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_WEBPLUGIN));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_LEGACYUI));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  }
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  }
  EXPECT_FALSE(IsLocalSystemHelper(
      static_cast<CommandLineMode>(
          COMMANDLINE_MODE_CRASH_HANDLER + 1)));
}

TEST_F(GoopdateUtilsIsMachineProcessTest,
       IsMachineProcess_MachineOverrideOnly) {
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_NETDIAGS));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_REPORTCRASH));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_INSTALL));
  }
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UPDATE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_IG));
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  }
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_UG));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UA));
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_WEBPLUGIN));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_LEGACYUI));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  }
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  }
  EXPECT_FALSE(MachineOverrideHelper(
      static_cast<CommandLineMode>(
          COMMANDLINE_MODE_CRASH_HANDLER + 1)));
}

TEST_F(GoopdateUtilsIsMachineProcessTest,
       IsMachineProcess_NeedsAdminFalseOnly) {
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_NETDIAGS));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_REPORTCRASH));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_INSTALL));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UPDATE));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_IG));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UG));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_WEBPLUGIN));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_LEGACYUI));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  EXPECT_FALSE(NeedsAdminFalseHelper(
      static_cast<CommandLineMode>(
          COMMANDLINE_MODE_CRASH_HANDLER + 1)));
}

TEST_F(GoopdateUtilsIsMachineProcessTest,
       IsMachineProcess_NeedsAdminTrueOnly) {
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_NETDIAGS));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_REPORTCRASH));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_INSTALL));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UPDATE));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_IG));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UG));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_WEBPLUGIN));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_LEGACYUI));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  EXPECT_FALSE(NeedsAdminTrueHelper(
      static_cast<CommandLineMode>(
          COMMANDLINE_MODE_CRASH_HANDLER + 1)));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_LegacyVersions) {
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.0.0.0")));
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.1.103.9")));
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.1.65535.65535")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_Omaha2AndLater) {
  EXPECT_TRUE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.2.0.0")));
  EXPECT_TRUE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.2.0111.2222")));
  EXPECT_TRUE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.3.456.7890")));
  EXPECT_TRUE(goopdate_utils::IsGoogleUpdate2OrLater(_T("2.0.0.0")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_VersionZero) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(
      goopdate_utils::IsGoogleUpdate2OrLater(_T("0.0.0.0")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_VersionUpperLimits) {
  EXPECT_TRUE(
      goopdate_utils::IsGoogleUpdate2OrLater(_T("65535.65535.65535.65535")));

  ExpectAsserts expect_asserts;
  EXPECT_FALSE(
      goopdate_utils::IsGoogleUpdate2OrLater(_T("65536.65536.65536.65536")));
  EXPECT_FALSE(
      goopdate_utils::IsGoogleUpdate2OrLater(_T("1.2.65536.65536")));
  EXPECT_FALSE(
      goopdate_utils::IsGoogleUpdate2OrLater(_T("1.1.65536.65536")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_TooFewElements) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.1.1")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_ExtraPeriod) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.1.2.3.")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_TooManyElements) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.1.2.3.4")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_Char) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.B.3.4")));
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.2.3.B")));
  EXPECT_FALSE(goopdate_utils::IsGoogleUpdate2OrLater(_T("1.2.3.9B")));
}

TEST(GoopdateUtilsTest, FormatMessageForNetworkError) {
  const TCHAR* const kTestAppName = _T("Test App");
  CString message;
  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NO_NETWORK,
                                               kTestAppName,
                                               &message));
  EXPECT_STREQ(
      _T("Installation failed. Ensure that your computer is connected to the ")
      _T("Internet and that your firewall allows GoogleUpdate.exe to connect ")
      _T("and then try again. Error code = 0x80040801."),
      message);

  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NETWORK_UNAUTHORIZED,
                                               kTestAppName,
                                               &message));
  EXPECT_STREQ(
      _T("The Test App installer could not connect to the Internet because of ")
      _T("an HTTP 401 Unauthorized response. This is likely a proxy ")
      _T("configuration issue.  Please configure the proxy server to allow ")
      _T("network access and try again or contact your network administrator. ")
      _T("Error code = 0x80042191"),
      message);

  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NETWORK_FORBIDDEN,
                                               kTestAppName,
                                               &message));
  EXPECT_STREQ(
      _T("The Test App installer could not connect to the Internet because of ")
      _T("an HTTP 403 Forbidden response. This is likely a proxy ")
      _T("configuration issue.  Please configure the proxy server to allow ")
      _T("network access and try again or contact your network administrator. ")
      _T("Error code = 0x80042193"),
      message);

  EXPECT_EQ(true,
            FormatMessageForNetworkError(GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED,
                                         kTestAppName,
                                         &message));
  EXPECT_STREQ(
      _T("The Test App installer could not connect to the Internet because a ")
      _T("proxy server required user authentication. Please configure the ")
      _T("proxy server to allow network access and try again or contact your ")
      _T("network administrator. Error code = 0x80042197"),
      message);

  EXPECT_EQ(false, FormatMessageForNetworkError(E_FAIL,
                                                kTestAppName,
                                                &message));
  EXPECT_STREQ(
      _T("Installation failed. Ensure that your computer is connected to the ")
      _T("Internet and that your firewall allows GoogleUpdate.exe to connect ")
      _T("and then try again. Error code = 0x80004005."),
      message);
}

TEST(GoopdateUtilsTest, WriteInstallerDataToTempFile) {
  CStringA utf8_bom;
  utf8_bom.Format("%c%c%c", 0xEF, 0xBB, 0xBF);

  std::vector<CString> list_installer_data;

  list_installer_data.push_back(_T(""));
  list_installer_data.push_back(_T("hello\n"));
  list_installer_data.push_back(_T("good bye"));
  list_installer_data.push_back(_T("  there  you\n     go "));
  list_installer_data.push_back(_T("\"http://foo.bar.org/?q=stuff&h=other\""));
  list_installer_data.push_back(_T("foo\r\nbar\n"));
  list_installer_data.push_back(_T("foo\n\rbar"));    // LFCR is not recognized.

  std::vector<CStringA> expected_installer_data;
  expected_installer_data.push_back("");
  expected_installer_data.push_back("hello\n");
  expected_installer_data.push_back("good bye");
  expected_installer_data.push_back("  there  you\n     go ");
  expected_installer_data.push_back("\"http://foo.bar.org/?q=stuff&h=other\"");
  expected_installer_data.push_back("foo\r\nbar\n");
  expected_installer_data.push_back("foo\n\rbar");

  ASSERT_EQ(expected_installer_data.size(), list_installer_data.size());

  for (size_t i = 0; i < list_installer_data.size(); ++i) {
    CString installer_data = list_installer_data[i];
    SCOPED_TRACE(installer_data);

    CString file_path;
    HRESULT hr = goopdate_utils::WriteInstallerDataToTempFile(
        installer_data,
        &file_path);
    EXPECT_SUCCEEDED(hr);

    // TODO(omaha): consider eliminating the special case.
    // WriteInstallerDataToTempFile() will return S_FALSE with "" data.
    if (S_OK == hr) {
      File file;
      const int kBufferLen = 1000;
      std::vector<byte> data_line(kBufferLen);
      EXPECT_SUCCEEDED(file.Open(file_path, false, false));
      uint32 bytes_read(0);
      EXPECT_SUCCEEDED(file.Read(data_line.size(),
                                 &data_line.front(),
                                 &bytes_read));
      data_line.resize(bytes_read);
      data_line.push_back(0);
      EXPECT_STREQ(utf8_bom + expected_installer_data[i],
                   reinterpret_cast<const char*>(&data_line.front()));
      EXPECT_SUCCEEDED(file.Close());
    } else {
      EXPECT_TRUE(installer_data.IsEmpty());
    }
  }
}

TEST(GoopdateUtilsTest, GetDefaultGoopdateTaskName_Core_Machine) {
  CString expected_task_name(kScheduledTaskNameMachinePrefix);
  expected_task_name += kScheduledTaskNameCoreSuffix;

  EXPECT_STREQ(
      expected_task_name,
      goopdate_utils::GetDefaultGoopdateTaskName(true, COMMANDLINE_MODE_CORE));
}

TEST(GoopdateUtilsTest, GetDefaultGoopdateTaskName_Core_User) {
  CString expected_task_name_user = kScheduledTaskNameUserPrefix;
  CString user_sid;
  EXPECT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &user_sid));
  expected_task_name_user += user_sid;
  expected_task_name_user += kScheduledTaskNameCoreSuffix;
  EXPECT_STREQ(
      expected_task_name_user,
      goopdate_utils::GetDefaultGoopdateTaskName(false, COMMANDLINE_MODE_CORE));
}

TEST(GoopdateUtilsTest, GetDefaultGoopdateTaskName_UA_Machine) {
  CString expected_task_name(kScheduledTaskNameMachinePrefix);
  expected_task_name += kScheduledTaskNameUASuffix;

  EXPECT_STREQ(
      expected_task_name,
      goopdate_utils::GetDefaultGoopdateTaskName(true, COMMANDLINE_MODE_UA));
}

TEST(GoopdateUtilsTest, GetDefaultGoopdateTaskName_UA_User) {
  CString expected_task_name_user = kScheduledTaskNameUserPrefix;
  CString user_sid;
  EXPECT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &user_sid));
  expected_task_name_user += user_sid;
  expected_task_name_user += kScheduledTaskNameUASuffix;
  EXPECT_STREQ(
      expected_task_name_user,
      goopdate_utils::GetDefaultGoopdateTaskName(false, COMMANDLINE_MODE_UA));
}

}  // namespace goopdate_utils

}  // namespace omaha

