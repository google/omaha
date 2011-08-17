// Copyright 2010 Google Inc.
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

// Definitions for the request and response data structures that are part of
// the xml protocol. Each attribute or element that occurs in the xml protocol
// has a definition here. The request and response xml artifacts are defined
// inside their corresponding namespaces.

#ifndef OMAHA_COMMON_PROTOCOL_DEFINITION_H_
#define OMAHA_COMMON_PROTOCOL_DEFINITION_H_

#include <vector>
#include "omaha/common/const_goopdate.h"
#include "omaha/common/install_manifest.h"
#include "omaha/common/ping_event.h"

namespace omaha {

namespace xml {

namespace request {

// Defines the structure of the Omaha protocol update request.
// The structure of the request is:
//
// Request | --- OS
//         | -<- App | --- UpdateCheck
//                   | --- Data
//                   | --- Ping
//                   | -<- PingEvent
//
// The xml parser traverses this data structure in order to serialize it. The
// names of the members of structures closely match the names of the elements
// and attributes in the xml document.
//
// TODO(omaha): briefly document the members.

struct OS {
  CString platform;
  CString version;
  CString service_pack;
  CString arch;
};

struct UpdateCheck {
  UpdateCheck() : is_valid(false), is_update_disabled(false) {}

  // TODO(omaha): this member is not serialized. Use pointers to indicate
  // optional elements instead of is_valid.
  bool is_valid;

  bool is_update_disabled;

  CString tt_token;
};

// For now, only a single "install" data is supported.
struct Data {
  CString install_data_index;
};

// didrun element. The element is named "ping" for legacy reasons.
struct Ping {
  Ping() : active(ACTIVE_UNKNOWN),
           days_since_last_active_ping(0),
           days_since_last_roll_call(0) {}

  ActiveStates active;
  int days_since_last_active_ping;
  int days_since_last_roll_call;
};

struct App {
    App() : install_time_diff_sec(0) {}

    CString app_id;

    CString version;

    CString next_version;

    CString ap;

    CString lang;

    CString iid;

    CString brand_code;

    CString client_id;

    CString experiments;

    int install_time_diff_sec;

    // Optional update check.
    UpdateCheck update_check;

    // Optional data.
    Data data;

    // Optional 'did run' ping.
    Ping ping;

    // Progress/result pings.
    PingEventVector ping_events;
  };

struct Request {
  Request() : is_machine(false), check_period_sec(-1) {}

  bool is_machine;

  CString uid;

  CString protocol_version;

  CString omaha_version;

  CString install_source;

  CString origin_url;

  // Identifies the source of the request as a test/production prober system.
  CString test_source;

  // Unique identifier for this request, used to associate the same request
  // received multiple times on the server.
  CString request_id;

  // Unique identifier for this session, used to correlate multiple requests
  // associated with a single operation by the Omaha client.
  CString session_id;

  // Time between update checks in seconds.
  // TODO(omaha): see if we can enforce by convention that -1 means it is
  // using the default value.
  int check_period_sec;

  OS os;

  std::vector<App> apps;
};

}  // namespace request

namespace response {

// Defines an Omaha protocol update response. The structure of the response is:
//
// Response | --- DayStart
//          | -<- App | --- UpdateCheck | --- Urls
//                                      | --- InstallManifest | --- Packages
//                                                            | --- Actions
//                    | --- Data
//                    | --- Ping
//                    | -<- Event
//
// The xml parser traverses the xml dom and populates the data members of
// the response. The names of the members of structures closely match the names
// of the elements and attributes in the xml document.
//
// TODO(omaha): briefly document the members.

struct UpdateCheck {
  CString status;

  CString tt_token;

  CString error_url;         // URL describing error.

  std::vector<CString> urls;

  InstallManifest install_manifest;
};

// For now, only a single "install" data is supported.
struct Data {
  CString status;

  CString install_data_index;
  CString install_data;
};

struct Ping {
  CString status;
};

struct Event {
  CString status;
};

struct App {
  CString status;

  // TODO(omaha): rename to app_id.
  CString appid;

  CString experiments;

  UpdateCheck update_check;

  std::vector<Data> data;

  Ping ping;

  std::vector<Event> events;
};

struct DayStart {
  DayStart() : elapsed_seconds(0) {}

  int elapsed_seconds;
};

struct Response {
  CString protocol;
  DayStart day_start;
  std::vector<App> apps;
};

}  // namespace response

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_PROTOCOL_DEFINITION_H_

