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
#include <objbase.h>
#include "base/scoped_ptr.h"
#include "omaha/common/error.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/utils.h"
#include "omaha/common/thread_pool.h"
#include "omaha/common/timer.h"
#include "omaha/goopdate/request.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/ping.h"

namespace omaha {

// Sends a ping when run in a thread pool.
class PingJob : public UserWorkItem {
 public:
  PingJob(Ping* ping, Request* request, HRESULT* result)
      : ping_(ping),
        request_(request),
        result_(result) {}

 private:
  virtual void DoProcess() {
    scoped_co_init init_com_apt(COINIT_MULTITHREADED);
    *result_ = ping_->SendPing(request_);
  }

  Ping* ping_;
  Request* request_;
  HRESULT* result_;

  DISALLOW_EVIL_CONSTRUCTORS(PingJob);
};

class PingTest : public testing::Test {
 public:
  PingTest() : ping_result_(E_FAIL) {}

 protected:
  virtual void SetUp() {
    ping_result_ = E_FAIL;
    thread_pool_.reset(new ThreadPool);
    EXPECT_HRESULT_SUCCEEDED(thread_pool_->Initialize(kShutdownDelayMs));

    ping_.reset(new Ping);

    AppData data(StringToGuid(_T("{C6E96DF3-BFEA-4403-BAA7-8920CE0B494A}")),
                              true);
    CreateTestAppData(&data);
    request_.reset(CreateRequest(true,
                                 data,
                                 PingEvent::EVENT_INSTALL_DOWNLOAD_FINISH,
                                 PingEvent::EVENT_RESULT_SUCCESS,
                                 0));
    ASSERT_TRUE(request_.get());
  }

  virtual void TearDown() {
    // ThreadPool destructor blocks waiting for the work items to complete.
    thread_pool_.reset();
    request_.reset();
    ping_.reset();
  }

  void CreateTestAppData(AppData* expected_app) {
    ASSERT_TRUE(expected_app != NULL);
    expected_app->set_version(_T("1.1.1.3"));
    expected_app->set_previous_version(_T("1.0.0.0"));
    expected_app->set_language(_T("abc"));
    expected_app->set_did_run(AppData::ACTIVE_RUN);
    expected_app->set_ap(_T("Test ap"));
    expected_app->set_tt_token(_T("Test TT Token"));
    expected_app->set_iid(
        StringToGuid(_T("{F723495F-8ACF-4746-8240-643741C797B5}")));
    expected_app->set_brand_code(_T("GOOG"));
    expected_app->set_client_id(_T("someclient"));
  }

  Request* CreateRequest(bool is_machine,
                         const AppData& app,
                         PingEvent::Types type,
                         PingEvent::Results result,
                         int error_code) {
    scoped_ptr<Request> req(new Request(is_machine));

    AppRequestData app_request_data(app);
    PingEvent ping_event(type,
                         result,
                         error_code,
                         8675309,
                         app.previous_version());
    app_request_data.AddPingEvent(ping_event);
    AppRequest app_request(app_request_data);
    req->AddAppRequest(app_request);
    return req.release();
  }

  HRESULT ping_result_;
  scoped_ptr<ThreadPool> thread_pool_;
  scoped_ptr<Ping> ping_;
  scoped_ptr<Request> request_;

  static const int kShutdownDelayMs = 60 * 1000;  // 60 seconds.
};

TEST_F(PingTest, SendPing) {
  // The same Ping instance can send multiple pings.
  EXPECT_HRESULT_SUCCEEDED(ping_->SendPing(request_.get()));
  EXPECT_HRESULT_SUCCEEDED(ping_->SendPing(request_.get()));
}

// Runs a ping instance in a thread pool and attempts to cancel it.
// Either the ping succeeds or it is canceled.
TEST_F(PingTest, CancelPing) {
  scoped_ptr<UserWorkItem> work_item(new PingJob(ping_.get(),
                                                 request_.get(),
                                                 &ping_result_));
  EXPECT_HRESULT_SUCCEEDED(thread_pool_->QueueUserWorkItem(work_item.get(),
                           WT_EXECUTEDEFAULT));
  work_item.release();

  // Sleep for a while to give the ping some time to run.
  ::Sleep(100);
  EXPECT_HRESULT_SUCCEEDED(ping_->Cancel());

  // Wait for the ping work item to complete.
  LowResTimer timer(true);
  while (thread_pool_->HasWorkItems() && timer.GetSeconds() <= 60) {
    ::Sleep(20);
  }

  EXPECT_TRUE(SUCCEEDED(ping_result_) ||
              ping_result_ == OMAHA_NET_E_REQUEST_CANCELLED);
}

TEST_F(PingTest, SendEmptyPing) {
  Request req(false);
  req.AddAppRequest(AppRequest(AppRequestData(AppData())));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_NO_DATA), ping_->SendPing(&req));
}

TEST_F(PingTest, SendPing_GoogleUpdateEulaNotAccepted) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_EQ(GOOPDATE_E_CANNOT_USE_NETWORK, ping_->SendPing(request_.get()));

  RestoreRegistryHives();
  RegKey::DeleteKey(kRegistryHiveOverrideRoot);
}

}  // namespace omaha
