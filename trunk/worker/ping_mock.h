// Copyright 2009-2010 Google Inc.
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

#ifndef OMAHA_WORKER_PING_MOCK_H__
#define OMAHA_WORKER_PING_MOCK_H__

#include <windows.h>
#include <vector>
#include "omaha/goopdate/request.h"
#include "omaha/worker/ping.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class PingMock : public Ping {
 public:
  PingMock() {}

  virtual ~PingMock() {
    for (size_t i = 0; i < ping_requests_.size(); ++i) {
      delete ping_requests_[i];
    }
  }

  // Creates a copy of req and stores it in the ping_requests_ vector.
  // The implementation must be kept in sync with Request's members.
  virtual HRESULT SendPing(Request* req) {
    ASSERT1(req);

    Request* request = new Request(req->is_machine());
    request->version_ = req->version();
    request->os_version_ = req->os_version();
    request->os_service_pack_ = req->os_service_pack();
    request->test_source_ = req->test_source();
    request->request_id_ = req->request_id();
    request->app_requests_ = req->app_requests_;

    ping_requests_.push_back(request);
    return S_OK;
  }

  const std::vector<Request*>& ping_requests() const { return ping_requests_; }

 private:
  std::vector<Request*> ping_requests_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(PingMock);
};

// Returns the event from an AppRequest that contains a single event.
inline const PingEvent& GetSingleEventFromAppRequest(
    const AppRequest& app_request,
    const GUID& expected_app_guid,
    bool expected_is_machine) {
  const AppRequestData& app_request_data = app_request.request_data();

  const AppData& app_data = app_request_data.app_data();
  EXPECT_TRUE(::IsEqualGUID(expected_app_guid, app_data.app_guid()));
  EXPECT_TRUE(::IsEqualGUID(GUID_NULL, app_data.parent_app_guid()));
  EXPECT_EQ(expected_is_machine, app_data.is_machine_app());
  EXPECT_TRUE(!app_data.version().IsEmpty());
  EXPECT_TRUE(!app_data.previous_version().IsEmpty());

  EXPECT_EQ(1, app_request_data.num_ping_events());
  return *app_request_data.ping_events_begin();
}

inline const PingEvent& GetSingleEventFromRequest(const Request& request,
                                                  const GUID& expected_app_guid,
                                                  bool expected_is_machine) {
  EXPECT_EQ(expected_is_machine, request.is_machine());
  EXPECT_TRUE(!request.version().IsEmpty());
  EXPECT_TRUE(!request.os_version().IsEmpty());
  // Skip checking request.os_service_pack() as it can be empty for RTM.
#if defined(DEBUG) || !OFFICIAL_BUILD
  EXPECT_TRUE(!request.test_source().IsEmpty());
#else
  EXPECT_TRUE(request.test_source().IsEmpty());
#endif
  EXPECT_TRUE(!request.request_id().IsEmpty());
  EXPECT_EQ(1, request.get_request_count());
  return GetSingleEventFromAppRequest(*request.app_requests_begin(),
                                      expected_app_guid,
                                      expected_is_machine);
}

}  // namespace omaha

#endif  // OMAHA_WORKER_PING_MOCK_H__
