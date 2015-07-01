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

// TODO(omaha): why so many dependencies for this unit test?

#include <windows.h>
#include <atlstr.h>
#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/signatures.h"
#include "omaha/base/thread_pool.h"
#include "omaha/base/timer.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/goopdate/app_state_checking_for_update.h"
#include "omaha/goopdate/app_state_waiting_to_download.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/goopdate/download_manager.h"
#include "omaha/goopdate/file_hash.h"
#include "omaha/testing/unit_test.h"

using ::testing::_;
using ::testing::Return;

namespace omaha {

namespace {

const FileHash kUpdateBinHashSha1 = {
    _T("YF2z/br/S6E3KTca0MT7qziJN44="), _T("")
};
const FileHash kUpdateBinBothHashes = {
    _T("YF2z/br/S6E3KTca0MT7qziJN44="),
    _T("e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad")
};
const FileHash kUpdateBin1HashSha1 = {
    _T("tbYInfmArVRUD62Ex292vN4LtGQ="), _T("")
};
const FileHash kUpdateBin1BothHashes = {
    _T("tbYInfmArVRUD62Ex292vN4LtGQ="),
    _T("f955bdcb6611c4e3033cf5104e01c732001da4a79e23f7771fc6f0216195bd6e")
};
const FileHash kUpdateBin1HashSha256 = {
    _T(""),
    _T("f955bdcb6611c4e3033cf5104e01c732001da4a79e23f7771fc6f0216195bd6e")
};

const TCHAR kAppGuid1[] = _T("{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}");
const TCHAR kAppGuid2[] = _T("{C7F2B395-A01C-4806-AA07-9163F66AFC48}");


class DownloadAppWorkItem : public UserWorkItem {
 public:
  DownloadAppWorkItem(DownloadManager* download_manager, App* app)
      : download_manager_(download_manager), app_(app) {}

 private:
  virtual void DoProcess() {
    download_manager_->DownloadApp(app_);
  }

  DownloadManager* download_manager_;
  App* app_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadAppWorkItem);
};

bool FileHashesEqual(const FileHash& hash1, const FileHash& hash2) {
  return hash1.sha256 == hash2.sha256 && hash1.sha1 == hash2.sha1;
}

}  // namespace

class DownloadManagerTest : public AppTestBase {
 public:
  static HRESULT BuildUniqueFileName(const CString& filename,
                                     CString* unique_filename) {
    return DownloadManager::BuildUniqueFileName(filename,
                                                unique_filename);
  }

 protected:
  explicit DownloadManagerTest(bool is_machine)
      : AppTestBase(is_machine, true) {}

  virtual void SetUp() {
    AppTestBase::SetUp();

    CleanupFiles();

    download_manager_.reset(new DownloadManager(is_machine_));
    EXPECT_SUCCEEDED(download_manager_->Initialize());
  }

  virtual void TearDown() {
    download_manager_.reset();
    CleanupFiles();

    AppTestBase::TearDown();
  }

  virtual void CleanupFiles() = 0;

  static void SetAppStateCheckingForUpdate(App* app) {
    SetAppStateForUnitTest(app, new fsm::AppStateCheckingForUpdate);
  }

  static void SetAppStateWaitingToDownload(App* app) {
    SetAppStateForUnitTest(app, new fsm::AppStateWaitingToDownload);
  }

  const CString cache_path_;
  scoped_ptr<DownloadManager> download_manager_;
};


class DownloadManagerMachineTest : public DownloadManagerTest {
 protected:
  DownloadManagerMachineTest() : DownloadManagerTest(true) {}

  virtual void CleanupFiles() {
    ConfigManager* cm(ConfigManager::Instance());
    DeleteDirectory(cm->GetMachineInstallWorkingDir());
    DeleteDirectory(cm->GetMachineSecureDownloadStorageDir());
  }
};

class DownloadManagerUserTest : public DownloadManagerTest {
 protected:
  DownloadManagerUserTest() : DownloadManagerTest(false) {}

  virtual void CleanupFiles() {
    ConfigManager* cm(ConfigManager::Instance());
    DeleteDirectory(cm->GetUserInstallWorkingDir());
    DeleteDirectory(cm->GetUserDownloadStorageDir());
  }
};

TEST_F(DownloadManagerUserTest, DownloadApp_MultiplePackagesInOneApp) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // One app, two packages.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash_sha256=\"e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad\" "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
            "<package "
              "hash=\"tbYInfmArVRUD62Ex292vN4LtGQ=\" "
              "name=\"UpdateData1.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  // Tests the first package.
  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinBothHashes, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
  EXPECT_LT(0, app->GetDownloadTimeMs());

  // Tests the second package.
  package = app->next_version()->GetPackage(1);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData1.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1HashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
  EXPECT_LT(0, app->GetDownloadTimeMs());

  // Sanity check the pings, including the two download metrics pings.
  CString actual_pings;
  const PingEventVector& pings(app->ping_events());
  for (size_t i = 0; i != pings.size(); ++i) {
    SafeCStringAppendFormat(&actual_pings, _T("%s; "), pings[i]->ToString());
  }

  EXPECT_NE(-1, actual_pings.Find(
      _T("eventtype=9, eventresult=1, errorcode=0, extracode1=0; ")));

  EXPECT_NE(-1, actual_pings.Find(
      _T("eventtype=5, eventresult=1, errorcode=0, extracode1=0; ")));

  EXPECT_NE(-1, actual_pings.Find(
      _T("eventtype=1, eventresult=1, errorcode=0, extracode1=0, ")
      _T("url=http://dl.google.com/update2/UpdateData.bin, ")
      _T("downloader=bits, error=0x0, downloaded_bytes=2048, ")
      _T("total_bytes=2048, download_time=")));

  EXPECT_NE(-1, actual_pings.Find(
      _T("eventtype=1, eventresult=1, errorcode=0, extracode1=0, ")
      _T("url=http://dl.google.com/update2/UpdateData1.bin, ")
      _T("downloader=bits, error=0x0, downloaded_bytes=2048, ")
      _T("total_bytes=2048, download_time=")));

  EXPECT_NE(-1, actual_pings.Find(
      _T("eventtype=1, eventresult=1, errorcode=0, extracode1=0; ")));
}

// Downloads multiple apps serially.
TEST_F(DownloadManagerUserTest, DownloadApp_MultipleApps) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid2), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App2"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Two apps, one package each.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash_sha256=\"e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad\" "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
    "<app appid=\"{C7F2B395-A01C-4806-AA07-9163F66AFC48}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"2.0\">"
          "<packages>"
            "<package "
              "hash=\"tbYInfmArVRUD62Ex292vN4LtGQ=\" "
              "name=\"UpdateData1.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));

  // Tests the first app.
  app = app_bundle_->GetApp(0);
  ASSERT_TRUE(app);
  SetAppStateWaitingToDownload(app);
  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinBothHashes, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
  EXPECT_LT(0, app->GetDownloadTimeMs());

  // Tests the second app.
  app = app_bundle_->GetApp(1);
  ASSERT_TRUE(app);
  SetAppStateWaitingToDownload(app);
  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData1.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1HashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
  EXPECT_LT(0, app->GetDownloadTimeMs());
}

// Downloads multiple apps concurrently. The test builds a bundle of two
// apps, creates two thread pool work items to download the apps, waits
// for the downloads to complete, and then checks the results of each download.
// This is essentialy the same unit test as DownloadApp_MultipleApps done
// concurrently instead of serially.
TEST_F(DownloadManagerUserTest, DownloadApp_Concurrent) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid2), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App2"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Two apps, one package each.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
    "<app appid=\"{C7F2B395-A01C-4806-AA07-9163F66AFC48}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"2.0\">"
          "<packages>"
            "<package "
              "hash_sha256=\"f955bdcb6611c4e3033cf5104e01c732001da4a79e23f7771fc6f0216195bd6e\" "
              "hash=\"tbYInfmArVRUD62Ex292vN4LtGQ=\" "
              "name=\"UpdateData1.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));

  // The thread pool waits up to 1 minute for the work items to complete when
  // the thread pool object is destroyed.
  const int kShutdownDelayMs = 60000;

  ThreadPool thread_pool;
  ASSERT_HRESULT_SUCCEEDED(thread_pool.Initialize(kShutdownDelayMs));

  const int kNumApps = 2;

  for (int i = 0; i != kNumApps; ++i) {
    app = app_bundle_->GetApp(i);
    SetAppStateWaitingToDownload(app);

    scoped_ptr<DownloadAppWorkItem> work_item(
        new DownloadAppWorkItem(download_manager_.get(), app));

    // WT_EXECUTELONGFUNCTION causes the thread pool to use multiple threads.
    ASSERT_HRESULT_SUCCEEDED(thread_pool.QueueUserWorkItem(
                                 work_item.release(),
                                 COINIT_MULTITHREADED,
                                 WT_EXECUTELONGFUNCTION));
  }

  // Poll the state of the download manager and wait up to 1 minute for the
  // downloads to complete.
  const int kTimeToWaitForDownloadsMs = 60000;
  const int kTimeToSleepWhenPollingMs   = 10;

  // Wait some time for the download manager to pick up the work items and
  // become busy.
  Timer timer(true);
  while (timer.GetMilliseconds() < kTimeToWaitForDownloadsMs) {
    if (download_manager_->IsBusy()) {
      break;
    }
  }
  timer.Reset();

  // Wait for the download manager to exit its busy state.
  timer.Start();
  while (download_manager_->IsBusy() &&
         timer.GetMilliseconds() < kTimeToWaitForDownloadsMs) {
    ::Sleep(kTimeToSleepWhenPollingMs);
  }

  // Expect that downloads have completed in a reasonable time.
  EXPECT_FALSE(download_manager_->IsBusy());

  // Test the outcome of the two downloads.
  app = app_bundle_->GetApp(0);
  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));

  app = app_bundle_->GetApp(1);
  package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData1.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1BothHashes, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));

  // Try to cancel the downloads if they could not complete in time and the
  // download manager is still busy. The thread pool waits a while for the work
  // items to complete after they have been canceled.
  if (download_manager_->IsBusy()) {
    for (int i = 0; i != kNumApps; ++i) {
      download_manager_->Cancel(app_bundle_->GetApp(i));
    }
    return;
  }
}

// Downloads multiple apps concurrently and cancels the downloads while they
// are in progress. The test builds a bundle of two apps with one file each,
// creates two thread pool work items to download the apps, waits for the
// downloads to begin, and then cancels them.
// TODO(omaha): Fix the intermittent failures.
TEST_F(DownloadManagerUserTest, DISABLED_DownloadApp_Cancel) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid2), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App2"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Two apps, one package each.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/dl/edgedl/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"jCBqGodZn1Ms5oZ1U28LFUaQDXo=\" "
              "name=\"UpdateData_10M.bin\" "
              "required=\"true\" "
              "size=\"10485760\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
    "<app appid=\"{C7F2B395-A01C-4806-AA07-9163F66AFC48}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/dl/edgedl/update2/\"/>"
        "</urls>"
        "<manifest version=\"2.0\">"
          "<packages>"
            "<package "
              "hash=\"jCBqGodZn1Ms5oZ1U28LFUaQDXo=\" "
              "name=\"UpdateData_10M.bin\" "
              "required=\"true\" "
              "size=\"10485760\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));

  // The thread pool waits up to 1 minute for the work items to complete when
  // the thread pool object is destroyed.
  const int kShutdownDelayMs = 60000;

  ThreadPool thread_pool;
  ASSERT_HRESULT_SUCCEEDED(thread_pool.Initialize(kShutdownDelayMs));

  const int kNumApps = 2;

  for (int i = 0; i != kNumApps; ++i) {
    app = app_bundle_->GetApp(i);
    SetAppStateWaitingToDownload(app);

    scoped_ptr<DownloadAppWorkItem> work_item(
        new DownloadAppWorkItem(download_manager_.get(), app));

    ASSERT_HRESULT_SUCCEEDED(thread_pool.QueueUserWorkItem(
                                 work_item.release(),
                                 COINIT_MULTITHREADED,
                                 WT_EXECUTELONGFUNCTION));
  }

  for (int i = 0; i != kNumApps; ++i) {
     app = app_bundle_->GetApp(i);
     EXPECT_NE(STATE_ERROR, app->state());
  }

  // Poll the state of the download manager and wait up to 1 minute for the
  // downloads to complete.
  const int kTimeToWaitForDownloadsMs = 60000;
  const int kTimeToSleepWhenPollingMs   = 10;

  // Cancel the downloads as soon as all apps are downloading and
  // wait until all apps have transitioned in the error state.
  Timer timer(true);
  bool is_done = false;
  while (!is_done && timer.GetMilliseconds() < kTimeToWaitForDownloadsMs) {
    int num_apps_downloading = 0;

    for (int i = 0; i != kNumApps; ++i) {
      app = app_bundle_->GetApp(i);
      if (app->state() == STATE_DOWNLOADING) {
        const Package* package = app->next_version()->GetPackage(0);
        if (package->bytes_downloaded()) {
          ++num_apps_downloading;
        }
      }
    }

    is_done = (num_apps_downloading == kNumApps);

    ::Sleep(kTimeToSleepWhenPollingMs);
  }

  for (int i = 0; i != kNumApps; ++i) {
    download_manager_->Cancel(app_bundle_->GetApp(i));
  }

  is_done = false;
  while (!is_done && timer.GetMilliseconds() < kTimeToWaitForDownloadsMs) {
    int num_apps_cancelled = 0;
    for (int i = 0; i != kNumApps; ++i) {
      app = app_bundle_->GetApp(i);
      if (app->state() == STATE_ERROR) {
        ++num_apps_cancelled;
      }
    }

    is_done = (num_apps_cancelled == kNumApps);

    ::Sleep(kTimeToSleepWhenPollingMs);
  }

  for (int i = 0; i != kNumApps; ++i) {
    // Check the state of the app and the package after the cancel call.
    app = app_bundle_->GetApp(i);
    EXPECT_EQ(STATE_ERROR, app->state());

    const Package* package = app->next_version()->GetPackage(0);
    ASSERT_TRUE(package);
    EXPECT_LT(0, package->bytes_downloaded());
    VARIANT_BOOL is_available(false);
    EXPECT_HRESULT_SUCCEEDED(package->get_isAvailable(&is_available));
    EXPECT_FALSE(is_available);

    // Check CurrentAppState members.
    CComPtr<IDispatch> current_state_disp;
    EXPECT_HRESULT_SUCCEEDED(app->get_currentState(&current_state_disp));
    CComPtr<ICurrentState> current_state;
    EXPECT_HRESULT_SUCCEEDED(
        current_state_disp->QueryInterface(&current_state));

    LONG state_value = 0;
    EXPECT_HRESULT_SUCCEEDED(current_state->get_stateValue(&state_value));
    EXPECT_EQ(STATE_ERROR, static_cast<CurrentState>(state_value));

    LONG error_code = 0;
    EXPECT_HRESULT_SUCCEEDED(current_state->get_errorCode(&error_code));
    EXPECT_EQ(GOOPDATE_E_CANCELLED, error_code);

    LONG extra_code1 = 0;
    EXPECT_HRESULT_SUCCEEDED(current_state->get_errorCode(&extra_code1));
    EXPECT_EQ(0, extra_code1);
  }
}

// Common packages of different apps are not cached by the package cache and
// will be redownloaded until the network cache is implemented.
// TODO(omaha): fix unit test as soon as the network cache is implemented.
TEST_F(DownloadManagerUserTest, DownloadApp_MultipleAppsCommonPackage) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid2), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App2"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Two apps, same package each.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
    "<app appid=\"{C7F2B395-A01C-4806-AA07-9163F66AFC48}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"2.0\">"
          "<packages>"
          "<package "
            "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
            "name=\"UpdateData.bin\" "
            "required=\"true\" "
            "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));

  // Tests the first app.
  app = app_bundle_->GetApp(0);
  ASSERT_TRUE(app);
  SetAppStateWaitingToDownload(app);
  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));

  // Tests the second app. The package is redownloaded.
  app = app_bundle_->GetApp(1);
  ASSERT_TRUE(app);
  SetAppStateWaitingToDownload(app);
  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
}

// Creates two bundles with the same app. The package corresponding to the
// app in the second bundle must come from the cache.
TEST_F(DownloadManagerUserTest, DownloadApp_FileAlreadyInCache) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  // Tests the package and the bytes downloaded.
  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
  EXPECT_LT(0, app->GetDownloadTimeMs());

  // Create the second app bundle.
  shared_ptr<AppBundle> app_bundle2(model_->CreateAppBundle(false));
  EXPECT_SUCCEEDED(app_bundle2->put_displayName(CComBSTR(_T("My Bundle"))));
  EXPECT_SUCCEEDED(app_bundle2->put_displayLanguage(CComBSTR(_T("en"))));
  EXPECT_SUCCEEDED(app_bundle2->initialize());

  app = NULL;
  ASSERT_SUCCEEDED(app_bundle2->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  // Since the package is cached, it does not matter if the EULA is accepted.
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_FALSE));

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle2.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));

  // Tests the package and the bytes downloaded.
  package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));

  // No bytes are downloaded if the package has been cached already.
  EXPECT_EQ(0, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
  EXPECT_EQ(0, app->GetDownloadTimeMs());
}

TEST_F(DownloadManagerUserTest, DISABLED_DownloadApp_404) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("404 Test"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"NoSuchFile-OmahaTest.exe\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_EQ(GOOPDATE_E_NETWORK_FIRST + 404,
            download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("NoSuchFile-OmahaTest.exe"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(0, package->bytes_downloaded());
  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));
}

TEST_F(DownloadManagerUserTest, DownloadApp_Sha1HashFailure) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("Hash Fail"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Provides the wrong hash for the package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"tbYInfmArVRUD62Ex292vN4LtGQ=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE, download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1HashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));

  // All bytes were downloaded even if the validation of the file has failed.
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_LT(0, app->GetDownloadTimeMs());
}

TEST_F(DownloadManagerUserTest, DownloadApp_Sha256HashFailure) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("Hash Fail"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Provides the wrong hash for the package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash_sha256=\"f955bdcb6611c4e3033cf5104e01c732001da4a79e23f7771fc6f0216195bd6e\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE, download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1HashSha256, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));

  // All bytes were downloaded even if the validation of the file has failed.
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_LT(0, app->GetDownloadTimeMs());
}

TEST_F(DownloadManagerUserTest, DownloadApp_HashFailure_ActualSmaller) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("Hash Fail"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Provides the wrong hash for the package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"tbYInfmArVRUD62Ex292vN4LtGQ=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048000\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_EQ(GOOPDATEDOWNLOAD_E_FILE_SIZE_SMALLER,
            download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048000, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1HashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));

  // Actual bytes were downloaded even if the validation of the file has failed.
  EXPECT_EQ(2048, package->bytes_downloaded());
}

TEST_F(DownloadManagerUserTest, DownloadApp_HashFailure_ActualLarger) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("Hash Fail"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Provides the wrong hash for the package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"tbYInfmArVRUD62Ex292vN4LtGQ=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"20\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_EQ(GOOPDATEDOWNLOAD_E_FILE_SIZE_LARGER,
            download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(20, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBin1HashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));

  // Actual bytes were downloaded even if the validation of the file has failed
  // or expected a smaller file.
  EXPECT_EQ(2048, package->bytes_downloaded());
}

TEST_F(DownloadManagerUserTest, DownloadApp_BaseUrlFallback) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("Hash Fail"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // Provides the wrong hash for the package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/TEST_NOT_EXIST/\"/>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));
  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
}

TEST_F(DownloadManagerUserTest, DownloadApp_FallbackToNextUrlIfCachingFails) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(
      app->put_displayName(CComBSTR(_T("Hash Fails For First Url"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // First URL points to a corrupted file.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/test/fakedata/\"/>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));
  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(2048, package->bytes_downloaded());
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));
}

TEST_F(DownloadManagerUserTest, DownloadApp_EulaNotAccepted) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));

  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_FALSE));

  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  ExpectAsserts expect_asserts;  // Eula not accepted causes asserts.

  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            download_manager_->DownloadApp(app));

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);
  EXPECT_STREQ(_T("UpdateData.bin"), package->filename());
  EXPECT_EQ(2048, package->expected_size());
  EXPECT_TRUE(FileHashesEqual(kUpdateBinHashSha1, package->expected_hash()));
  EXPECT_EQ(0, package->bytes_downloaded());
  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));
  EXPECT_EQ(0, app->GetDownloadTimeMs());
}

TEST_F(DownloadManagerUserTest, GetPackage) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));
  EXPECT_SUCCEEDED(app->put_isEulaAccepted(VARIANT_TRUE));  // Allow download.

  // One app, one package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);

  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));
  EXPECT_SUCCEEDED(download_manager_->DownloadApp(app));
  EXPECT_TRUE(download_manager_->IsPackageAvailable(package));

  // Get a unique temp dir name. The directory is not created.
  CString dir(GetUniqueTempDirectoryName());

  // The call fails if the package destination directory does not exist.
  EXPECT_FAILED(download_manager_->GetPackage(package, dir));

  EXPECT_SUCCEEDED(CreateDir(dir, NULL));
  EXPECT_SUCCEEDED(download_manager_->GetPackage(package, dir));

  CString filename(ConcatenatePath(dir, package->filename()));
  std::vector<CString> files;
  files.push_back(filename);
  EXPECT_SUCCEEDED(VerifyFileHash(files, kUpdateBinHashSha1.sha1));

  // Getting the package the second time overwrites the destination file
  // and succeeds.
  EXPECT_SUCCEEDED(download_manager_->GetPackage(package, dir));

  EXPECT_SUCCEEDED(DeleteDirectory(dir));
}

TEST_F(DownloadManagerUserTest, GetPackage_NotPresent) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  EXPECT_SUCCEEDED(app->put_displayName(CComBSTR(_T("App1"))));

  // One app, one package.
  CStringA buffer_string =

  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<response protocol=\"3.0\">"
    "<app appid=\"{0B35E146-D9CB-4145-8A91-43FDCAEBCD1E}\" status=\"ok\">"
      "<updatecheck status=\"ok\">"
        "<urls>"
          "<url codebase=\"http://dl.google.com/update2/\"/>"
        "</urls>"
        "<manifest version=\"1.0\">"
          "<packages>"
            "<package "
              "hash=\"YF2z/br/S6E3KTca0MT7qziJN44=\" "
              "name=\"UpdateData.bin\" "
              "required=\"true\" "
              "size=\"2048\"/>"
          "</packages>"
        "</manifest>"
      "</updatecheck>"
    "</app>"
  "</response>";

  EXPECT_HRESULT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(), buffer_string));
  SetAppStateWaitingToDownload(app);

  const Package* package = app->next_version()->GetPackage(0);
  ASSERT_TRUE(package);

  EXPECT_FALSE(download_manager_->IsPackageAvailable(package));

  // Get a unique temp dir name. The directory is not created.
  CString dir(GetUniqueTempDirectoryName());

  EXPECT_SUCCEEDED(CreateDir(dir, NULL));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            download_manager_->GetPackage(package, dir));

  EXPECT_SUCCEEDED(DeleteDirectory(dir));
}

TEST(DownloadManagerTest, BuildUniqueFileName) {
  CString file1, file2;
  EXPECT_SUCCEEDED(DownloadManagerTest::BuildUniqueFileName(_T("a"), &file1));
  EXPECT_SUCCEEDED(DownloadManagerTest::BuildUniqueFileName(_T("a"), &file2));
  EXPECT_STRNE(file1, file2);
}

TEST(DownloadManagerTest, GetMessageForError) {
  const TCHAR* kEnglish = _T("en");
  EXPECT_SUCCEEDED(ResourceManager::Create(
      true, app_util::GetCurrentModuleDirectory(), kEnglish));

  EXPECT_STREQ(
      _T("Unable to connect to the Internet. If you use a firewall, please ")
      _T("whitelist GoogleUpdate.exe."),
      DownloadManager::GetMessageForError(
          ErrorContext(GOOPDATE_E_NO_NETWORK), kEnglish));

  EXPECT_STREQ(
      _T("Unable to connect to the Internet. HTTP 401 Unauthorized. Please ")
      _T("check your proxy configuration."),
      DownloadManager::GetMessageForError(
          ErrorContext(GOOPDATE_E_NETWORK_UNAUTHORIZED), kEnglish));

  EXPECT_STREQ(
      _T("Unable to connect to the Internet. HTTP 403 Forbidden. Please check ")
      _T("your proxy configuration."),
      DownloadManager::GetMessageForError(
          ErrorContext(GOOPDATE_E_NETWORK_FORBIDDEN), kEnglish));

  EXPECT_STREQ(
      _T("Unable to connect to the Internet. Proxy server requires ")
      _T("authentication."),
      DownloadManager::GetMessageForError(
          ErrorContext(GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED),
          kEnglish));

  EXPECT_STREQ(
      _T("The download failed."),
      DownloadManager::GetMessageForError(ErrorContext(E_FAIL), kEnglish));

  EXPECT_STREQ(
      _T("Failed to cache the downloaded installer. Error: 0x80070005."),
      DownloadManager::GetMessageForError(
          ErrorContext(GOOPDATEDOWNLOAD_E_CACHING_FAILED, 0x80070005),
          kEnglish));

  ResourceManager::Delete();
}

}  // namespace omaha
