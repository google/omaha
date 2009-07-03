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
// ping_event.h: Encapsulates events to attach to AppRequests for pings.

#ifndef OMAHA_WORKER_PING_EVENT_H__
#define OMAHA_WORKER_PING_EVENT_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/debug.h"

namespace omaha {

class PingEvent {
 public:
  // When updating this enum, also update the protocol file on the server.
  // These values get reported to the server, so do not change existing ones.
  //
  // Checkpoints:
  //  "EVENT_INSTALL_*" events report the progress of initial installs.
  //  "EVENT_UPDATE_*" events report the progress of silent updates.
  //  These checkpoints represent the "START" or "FINISH" of a phase.
  // Actions:
  //  "EVENT_*_BEGIN" events report the start of a specific action (i.e. job).
  //  "EVENT_*_COMPLETE" events represent the end of such actions and report
  // successful completion or the error that occurred during the action.
  enum Types {
    EVENT_UNKNOWN = 0,
    EVENT_INSTALL_DOWNLOAD_FINISH = 1,
    EVENT_INSTALL_COMPLETE = 2,
    EVENT_UPDATE_COMPLETE = 3,
    EVENT_UNINSTALL = 4,
    EVENT_INSTALL_DOWNLOAD_START = 5,
    EVENT_INSTALL_INSTALLER_START = 6,
    // Never used = 7
    // No longer used - EVENT_INSTALLED_GOOPDATE_STARTED = 8,
    EVENT_INSTALL_APPLICATION_BEGIN = 9,

    // Install Setup events.
    EVENT_SETUP_INSTALL_BEGIN = 10,
    EVENT_SETUP_INSTALL_COMPLETE = 11,

    // Update Events.
    // The Update Event = 3 above is used for update completion.
    EVENT_UPDATE_APPLICATION_BEGIN = 12,
    EVENT_UPDATE_DOWNLOAD_START = 13,
    EVENT_UPDATE_DOWNLOAD_FINISH = 14,
    EVENT_UPDATE_INSTALLER_START = 15,

    // Self-update Setup events.
    EVENT_SETUP_UPDATE_BEGIN = 16,
    EVENT_SETUP_UPDATE_COMPLETE = 17,

    // Ping when installed via /registerproduct.
    EVENT_REGISTER_PRODUCT_COMPLETE = 20,

    // Ping when an end user first boots a new system with an OEM-installed app.
    EVENT_INSTALL_OEM_FIRST_CHECK = 30,

    // Failure report events - not part of the normal flow.
    EVENT_SETUP_INSTALL_FAILURE = 100,
    // No longer used - EVENT_GOOPDATE_DLL_FAILURE = 101,
    EVENT_SETUP_COM_SERVER_FAILURE = 102,
    EVENT_SETUP_UPDATE_FAILURE = 103,
  };

  // When updating this enum, also update the identical one in
  // omaha_extensions.proto.
  // These values get reported to the server, so do not change existing ones.
  enum Results {
    EVENT_RESULT_ERROR = 0,
    EVENT_RESULT_SUCCESS = 1,
    EVENT_RESULT_SUCCESS_REBOOT = 2,
    //  EVENT_RESULT_SUCCESS_RESTART_BROWSER = 3,
    EVENT_RESULT_CANCELLED = 4,
    EVENT_RESULT_INSTALLER_ERROR_MSI = 5,
    EVENT_RESULT_INSTALLER_ERROR_OTHER = 6,
    EVENT_RESULT_NOUPDATE = 7,
    EVENT_RESULT_INSTALLER_ERROR_SYSTEM = 8,
    EVENT_RESULT_UPDATE_DEFERRED = 9,
  };

  // TODO(omaha): consider making previous_version part of the app element
  // instead of the event element.
  PingEvent(Types type,
            Results result,
            int error_code,
            int extra_code1,
            const CString& previous_version)
    :  event_type_(type),
       event_result_(result),
       error_code_(error_code),
       extra_code1_(extra_code1),
       previous_version_(previous_version) {
    ASSERT1(EVENT_UNKNOWN != event_type_);
  }

  Types event_type() const { return event_type_; }
  Results event_result() const { return event_result_; }
  int error_code() const { return error_code_; }
  int extra_code1() const { return extra_code1_; }
  CString previous_version() const { return previous_version_; }

 private:
  Types event_type_;
  Results event_result_;
  int error_code_;
  int extra_code1_;
  CString previous_version_;
};

typedef std::vector<PingEvent> PingEventVector;

}  // namespace omaha.

#endif  // OMAHA_WORKER_PING_EVENT_H__

