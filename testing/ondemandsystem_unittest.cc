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

//
// System level tests for On Demand.  Unlike omaha_unittest.cpp, this test
// should be run one test at a time view --gtest_filter.  The tests all assume
// that there is an update available for the app (specified via guid as the
// first argument).

#include "base/basictypes.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/gtest/include/gtest/gtest.h"
#include "omaha/tools/performondemand/performondemand.h"
#include "omaha/base/utils.h"
#include <windows.h>
#include <atltime.h>

namespace omaha {

// Lazily define the guid and is_machine (passed in at the command line) global.
CString guid;
bool is_machine;
const int UPDATE_TIMEOUT = 60;

class OnDemandTest : public testing::Test {
 protected:

  virtual void SetUp() {
    wprintf(_T("Initializing\n"));
    HRESULT hr = CComObject<JobObserver>::CreateInstance(&job_observer);
    ASSERT_EQ(S_OK, hr);
    job_holder = job_observer;
    ASSERT_EQ(Reconnect(), S_OK);
  }

  virtual HRESULT Reconnect() {
    on_demand = NULL;
    if (is_machine) {
      return on_demand.CoCreateInstance(
          L"GoogleUpdate.OnDemandCOMClassMachine");
    } else {
      return on_demand.CoCreateInstance(
          L"GoogleUpdate.OnDemandCOMClassUser");
    }
  }

  virtual void TearDown() {
    job_holder = NULL;
    on_demand = NULL;
  }

  void WaitForUpdateCompletion(int timeout) {
    MSG msg;
    SYSTEMTIME start_system_time = {0};
    SYSTEMTIME current_system_time = {0};
    ::GetSystemTime(&start_system_time);
    CTime start_time(start_system_time);
    CTimeSpan timeout_period(0, 0, 0, timeout);

    while (::GetMessage(&msg, NULL, 0, 0))
    {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      ::GetSystemTime(&current_system_time);
      CTime current_time(current_system_time);
      CTimeSpan elapsed_time = current_time - start_time;
      if (timeout_period < elapsed_time) {
        wprintf(_T("Timed out.\n"));
        break;
      }
    }

    PrintSummary();
  }

  void PrintSummary() {
    wprintf(_T("Observed: [0x%x]\n"), job_observer->observed);
  }

  void ExpectSuccessful() {
    // Make sure we got a succesfful install.
    char* err_msg = "Did not observe a complete, successful install!";
    EXPECT_TRUE(job_observer->observed & (
        // Complete codes we don't expect are intentially commented out.
        ON_COMPLETE_SUCCESS |
        //ON_COMPLETE_SUCCESS_CLOSE_UI |
        //ON_COMPLETE_RESTART_ALL_BROWSERS |
        //ON_COMPLETE_REBOOT |
        //ON_COMPLETE_RESTART_BROWSER |
        ON_COMPLETE_RESTART_ALL_BROWSERS_NOTICE_ONLY
        //ON_COMPLETE_REBOOT_NOTICE_ONLY |
        //ON_COMPLETE_RESTART_BROWSER_NOTICE_ONLY |
        //ON_COMPLETE_RUN_COMMAND
        )) << err_msg;
  }

  CComObject<JobObserver>* job_observer;
  CComPtr<IJobObserver> job_holder;
  CComPtr<IGoogleUpdate> on_demand;
};

TEST_F(OnDemandTest, BasicUpdate) {
  wprintf(_T("Starting Update\n"));
  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  char* err_msg = "Did not observe a complete, successful install!";
  EXPECT_TRUE(job_observer->observed & ON_INSTALLING) << err_msg;
}

TEST_F(OnDemandTest, OnDemandDuringAutoUpdate) {
  wprintf(_T("Starting Update\n"));
  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  char* err_msg = "Did not observe a complete, successful install!";
  EXPECT_TRUE(job_observer->observed & ON_INSTALLING) << err_msg;
}


TEST_F(OnDemandTest, UpdateThenNoUpdate) {
  wprintf(_T("Starting Update\n"));
  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  char* err_msg = "Did not observe a complete, successful install!";
  EXPECT_TRUE(job_observer->observed & ON_INSTALLING) << err_msg;


  wprintf(_T("Starting Second Update Request\n"));
  // Reset the memory of observed actions.
  job_observer->Reset();
  hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  EXPECT_FALSE(job_observer->observed & ON_INSTALLING) << err_msg;
}


TEST_F(OnDemandTest, CloseDuringCheckingForUpdates) {
  wprintf(_T("Starting Update\n"));

  job_observer->AddCloseMode(ON_CHECKING_FOR_UPDATES);

  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  char* err_msg = "Observed a complete install when should have closed!";
  EXPECT_TRUE(job_observer->observed & ON_COMPLETE_ERROR) << err_msg;
  EXPECT_FALSE(job_observer->observed & ON_INSTALLING) << err_msg;
}


TEST_F(OnDemandTest, CloseDuringDownload) {
  wprintf(_T("Starting Update\n"));

  job_observer->AddCloseMode(ON_DOWNLOADING);

  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  char* err_msg = "Observed an install when should have closed!";
  EXPECT_FALSE(job_observer->observed & ON_INSTALLING) << err_msg;
}


TEST_F(OnDemandTest, CloseDuringDownloadAndTryAgain) {
  wprintf(_T("Starting Update\n"));

  job_observer->AddCloseMode(ON_DOWNLOADING);

  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  char* err_msg = "Observed a complete, install when should have closed!";
  EXPECT_FALSE(job_observer->observed & ON_INSTALLING) << err_msg;

  wprintf(_T("Requesting update.\n"));
  // Try a second time, but this time don't interfere.  An update should ensue.
  job_observer->Reset();
  hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  EXPECT_TRUE(job_observer->observed & ON_INSTALLING) << err_msg;
}


TEST_F(OnDemandTest, UpdateWithOmahaUpdateAvailable) {
  wprintf(_T("Starting Update\n"));
  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);
  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  char* err_msg = "Did not observe a complete, successful install!";
  EXPECT_TRUE(job_observer->observed & ON_INSTALLING) << err_msg;
}


TEST_F(OnDemandTest, UpdateAfterOmahaUpdate) {
  // Test shutdown code by first connecting the the com instance, waiting around
  // for Omaha to update (done by OnDemandTestFactory.py), and then updating.
  wprintf(_T("Waiting 60 seconds for omaha to update itself.\n"));
  ::SleepEx(60000, true);

  wprintf(_T("Attempting update on shut-down server.\n"));
  HRESULT hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(hr, HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE)) <<
      "Goopdate should have shutdown.";

  wprintf(_T("Reconnecting to COM\n"));
  ASSERT_EQ(Reconnect(), S_OK);

  wprintf(_T("Starting Update\n"));
  hr = on_demand->Update(guid, job_observer);
  ASSERT_EQ(S_OK, hr);

  WaitForUpdateCompletion(UPDATE_TIMEOUT);

  ExpectSuccessful();
  char* err_msg = "Did not observe a complete, successful install!";
  EXPECT_TRUE(job_observer->observed & ON_INSTALLING) << err_msg;
}



bool ParseParams(int argc, TCHAR* argv[], CString* guid, bool* is_machine) {
  ASSERT1(argv);
  ASSERT1(guid);
  if (argc < 3) {
    return false;
  }
  *guid = argv[1];
  // NOTE(cnygaard): I tried static casting from int to bool but it gave me a
  // nasty warning about losing efficiency.
  if (0 == _ttoi(argv[2])) {
    *is_machine = false;
  } else {
    *is_machine = true;
  }

  // Verify that the guid is valid.
  GUID parsed = StringToGuid(*guid);
  if (parsed == GUID_NULL) {
    return false;
  }

  argc -= 2;
  testing::InitGoogleTest(&argc, argv+2);
  return true;
}

}  // namespace omaha

int _tmain(int argc, TCHAR** argv) {

  if (!omaha::ParseParams(argc, argv, &omaha::guid, &omaha::is_machine)) {
    wprintf(_T("Usage: ondemandsystem_unittest.exe \n"));
    wprintf(_T("  [{GUID}] [is_machine (0|1)] --gtest_filter=<testname>\n"));
    return 0;
  }
  omaha::FailOnAssert fail_on_assert;
  CComModule module;
  scoped_co_init com_apt;

  int result = RUN_ALL_TESTS();
  return result;
}

