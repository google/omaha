// Copyright 2012 Google Inc.
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

#ifndef OMAHA_COMMON_PING_EVENT_H_
#define OMAHA_COMMON_PING_EVENT_H_

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/debug.h"
#include "third_party/bar/shared_ptr.h"

namespace omaha {

class PingEvent {
 public:
  // The extra code represents the file order as defined by the setup.
  static const int kSetupFilesExtraCodeMask = 0x00000100;

  // The extra code represents a state of the app state machine.
  static const int kAppStateExtraCodeMask   = 0x10000000;

  // List of sources that could generate EVENT_DEBUG pings.
  enum DebugMessageSource {
    DEBUG_SOURCE_CUP_FAILURE  = 0,
    DEBUG_SOURCE_FILE_VERIFICATION = 1,
    DEBUG_SOURCE_LONG_TAIL = 2,
    DEBUG_SOURCE_CRICKET_PROBE = 3,
  };

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
    // No longer used - EVENT_SETUP_INSTALL_BEGIN = 10,
    // No longer used - EVENT_SETUP_INSTALL_COMPLETE = 11,

    // Update Events.
    // The Update Event = 3 above is used for update completion.
    EVENT_UPDATE_APPLICATION_BEGIN = 12,
    EVENT_UPDATE_DOWNLOAD_START = 13,
    EVENT_UPDATE_DOWNLOAD_FINISH = 14,
    EVENT_UPDATE_INSTALLER_START = 15,

    // Self-update Setup events.
    // No longer used - EVENT_SETUP_UPDATE_BEGIN = 16,
    // No longer used - EVENT_SETUP_UPDATE_COMPLETE = 17,

    // Ping when installed via /registerproduct.
    EVENT_REGISTER_PRODUCT_COMPLETE = 20,

    // Ping when an end user first boots a new system with an OEM-installed app.
    EVENT_INSTALL_OEM_FIRST_CHECK = 30,

    // App Command Events
    EVENT_APP_COMMAND_BEGIN = 40,
    EVENT_APP_COMMAND_COMPLETE = 41,

    // Failure report events - not part of the normal flow.
    // No longer used - EVENT_SETUP_INSTALL_FAILURE = 100,
    // No longer used - EVENT_GOOPDATE_DLL_FAILURE = 101,
    // No longer used - EVENT_SETUP_COM_SERVER_FAILURE = 102,
    // No longer used - EVENT_SETUP_UPDATE_FAILURE = 103,

    EVENT_DEBUG = 50,
    EVENT_CHROME_RECOVERY_COMPONENT = 53
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
    //  EVENT_RESULT_NOUPDATE = 7,
    EVENT_RESULT_INSTALLER_ERROR_SYSTEM = 8,
    EVENT_RESULT_UPDATE_DEFERRED = 9,
    EVENT_RESULT_HANDOFF_ERROR = 10,
  };

  PingEvent(Types type,
            Results result,
            int error_code,
            int extra_code1);

  PingEvent(Types type,
            Results result,
            int error_code,
            int extra_code1,
            int source_url_index,
            int update_check_time_ms,
            int download_time_ms,
            uint64 num_bytes_downloaded,
            uint64 app_size,
            int install_time_ms);

  virtual ~PingEvent() {}

  virtual HRESULT ToXml(IXMLDOMNode* parent_node) const;
  virtual CString ToString() const;

 private:
  const Types event_type_;
  const Results event_result_;
  const int error_code_;
  const int extra_code1_;

  const int source_url_index_;
  const int update_check_time_ms_;
  const int download_time_ms_;
  const uint64 num_bytes_downloaded_;
  const uint64 app_size_;
  const int install_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(PingEvent);
};

typedef shared_ptr<const PingEvent> PingEventPtr;
typedef std::vector<PingEventPtr> PingEventVector;

}  // namespace omaha

#endif  // OMAHA_COMMON_PING_EVENT_H_

