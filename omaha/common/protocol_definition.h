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

#include <cstring>
#include <vector>
#include "omaha/common/const_goopdate.h"
#include "omaha/common/install_manifest.h"
#include "omaha/common/ping_event.h"

namespace omaha {

typedef std::pair<CString, CString> StringPair;

namespace xml {

namespace request {

// Defines the structure of the Omaha protocol update request.
// The structure of the request is:
//
// Request | --- Hw
//         | --- OS
//         | -<- App | --- UpdateCheck
//                   | -<- Data
//                   | --- Ping
//                   | -<- PingEvent
//
// In the diagram above, --- and -<- mean 1:1 and 1:many respectively.
//
// The xml parser traverses this data structure in order to serialize it. The
// names of the members of structures closely match the names of the elements
// and attributes in the xml document.

struct Hw {
  uint32 physmemory;  // Physical memory rounded down to the closest GB.

  // Instruction set capabilities for the CPU.
  bool has_sse;
  bool has_sse2;
  bool has_sse3;
  bool has_ssse3;
  bool has_sse41;
  bool has_sse42;
  bool has_avx;
};

struct OS {
  CString platform;       // "win".
  CString version;        // major.minor.
  CString service_pack;
  CString arch;  // "x86", "x64", "ARM64", etc, or "unknown".
};

struct UpdateCheck {
  UpdateCheck()
      : is_valid(false),
        is_update_disabled(false),
        is_rollback_allowed(false) {}

  // TODO(omaha): this member is not serialized. Use pointers to indicate
  // optional elements instead of is_valid.
  bool is_valid;

  bool is_update_disabled;

  CString tt_token;

  bool is_rollback_allowed;

  CString target_version_prefix;

  CString target_channel;
};


struct Data {
  CString name;                 // It could be either "install" or "untrusted".
  CString install_data_index;
  CString untrusted_data;
};

// didrun element. The element is named "ping" for legacy reasons.
struct Ping {
  Ping() : active(ACTIVE_UNKNOWN),
           days_since_last_active_ping(0),
           days_since_last_roll_call(0),
           day_of_last_activity(0),
           day_of_last_roll_call(0) {}

  ActiveStates active;
  int days_since_last_active_ping;
  int days_since_last_roll_call;
  int day_of_last_activity;
  int day_of_last_roll_call;
  CString ping_freshness;
};

struct App {
  App() : install_time_diff_sec(0), day_of_install(0) {}

  CString app_id;

  CString version;

  CString next_version;

  std::vector<StringPair> app_defined_attributes;

  CString ap;

  CString lang;

  CString iid;

  CString brand_code;

  CString client_id;

  CString experiments;

  int install_time_diff_sec;

  int day_of_install;

  CString cohort;       // Opaque string.
  CString cohort_hint;  // Server may use to move the app to a new cohort.
  CString cohort_name;  // Human-readable interpretation of the cohort.

  // Optional update check.
  UpdateCheck update_check;

  // Optional data.
  std::vector<Data> data;

  // Optional 'did run' ping.
  Ping ping;

  // Progress/result pings.
  PingEventVector ping_events;
};

struct Request {
  Request() : is_machine(false), check_period_sec(-1), domain_joined(false) {
    memset(&hw, 0, sizeof(hw));
  }

  bool is_machine;

  CString uid;

  CString protocol_version;

  CString omaha_version;

  CString omaha_shell_version;

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

  // Provides a hint for what download urls should be returned by server.
  // This data member is controlled by a group policy settings.
  // The only group policy value supported so far is "cacheable".
  CString dlpref;

  // True if this machine is part of a managed enterprise domain.
  bool domain_joined;

  Hw hw;

  OS os;

  std::vector<App> apps;
};

}  // namespace request

namespace response {

// Status strings returned by the server.
const TCHAR* const kStatusOkValue = _T("ok");
const TCHAR* const kStatusNoUpdate = _T("noupdate");
const TCHAR* const kStatusRestrictedExportCountry = _T("restricted");
const TCHAR* const kStatusHwNotSupported = _T("error-hwnotsupported");
const TCHAR* const kStatusOsNotSupported = _T("error-osnotsupported");
const TCHAR* const kStatusUnKnownApplication = _T("error-UnKnownApplication");
const TCHAR* const kStatusInternalError = _T("error-internal");
const TCHAR* const kStatusHashError = _T("error-hash");
const TCHAR* const kStatusUnsupportedProtocol = _T("error-unsupportedprotocol");
const TCHAR* const kStatusNoData = _T("error-nodata");
const TCHAR* const kStatusInvalidArgs = _T("error-invalidargs");

// Defines an Omaha protocol update response. The structure of the response is:
//
// Response | --- DayStart
//          | --- SystemRequirements
//          | -<- App | --- UpdateCheck | --- Urls
//                                      | --- InstallManifest | --- Packages
//                                                            | --- Actions
//                    | -<- Data
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

  CString error_url;         // URL describing error. Ignored in Omaha 3.

  std::vector<CString> urls;

  InstallManifest install_manifest;
};

struct Data {
  CString status;
  CString name;
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

  CString cohort;       // Opaque string.
  CString cohort_hint;  // Server may use to move the app to a new cohort.
  CString cohort_name;  // Human-readable interpretation of the cohort.

  UpdateCheck update_check;

  std::vector<Data> data;

  Ping ping;

  std::vector<Event> events;
};

struct DayStart {
  DayStart() : elapsed_seconds(0), elapsed_days(0) {}

  int elapsed_seconds;  // Number of seconds since mid-night.
  int elapsed_days;     // Number of days elapsed since a chosen datum.
};

struct SystemRequirements {
  CString platform;        // "win".

  // Expected host processor architecture that the app is compatible with.
  // `arch` can be a single entry, or multiple entries separated with `,`.
  // Entries prefixed with a `-` (negative entries) indicate non-compatible
  // hosts.
  //
  // Examples:
  // * `arch` == "x86".
  // * `arch` == "x64".
  // * `arch` == "x86,x64,-arm64": the app will fail installation if the
  // underlying host is arm64.
  CString arch;

  CString min_os_version;  // major.minor.
};

struct Response {
  CString protocol;
  DayStart day_start;
  SystemRequirements sys_req;
  std::vector<App> apps;
};

}  // namespace response

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_PROTOCOL_DEFINITION_H_
