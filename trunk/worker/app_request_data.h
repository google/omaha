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
// app_request_data.h:  The app or component level data required to generate the
// XML that asks the server about that app or component.  Also used for pings in
// addition to install/update requests.  These are contained within AppRequest
// objects to represent the full product hierarchy.

#ifndef OMAHA_WORKER_APP_REQUEST_DATA_H__
#define OMAHA_WORKER_APP_REQUEST_DATA_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/ping_event.h"

namespace omaha {

class AppRequestData {
 public:
  AppRequestData() {}
  explicit AppRequestData(const AppData& app_data) {
    app_data_ = app_data;
  }

  void set_app_data(const AppData& app_data) {
    app_data_ = app_data;
  }
  const AppData& app_data() const { return app_data_; }

  void AddPingEvent(const PingEvent& ping_event) {
    ping_events_.push_back(ping_event);
  }

  PingEventVector::const_iterator ping_events_begin() const {
    return ping_events_.begin();
  }

  PingEventVector::const_iterator ping_events_end() const {
    return ping_events_.end();
  }

  size_t num_ping_events() const { return ping_events_.size(); }

 private:
  AppData app_data_;
  PingEventVector ping_events_;
};

typedef std::vector<AppRequestData> AppRequestDataVector;

}  // namespace omaha.

#endif  // OMAHA_WORKER_APP_REQUEST_DATA_H__

