// Copyright 2013 Google Inc.
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

#include "omaha/goopdate/app_command_ping_delegate.h"

#include <vector>
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/ping.h"
#include "omaha/common/ping_event.h"

namespace omaha {

AppCommandPingDelegate::AppCommandPingDelegate(const CString& app_guid,
                                               bool is_machine,
                                               const CString& session_id,
                                               int reporting_id)
    : app_guid_(app_guid),
      is_machine_(is_machine),
      session_id_(session_id),
      reporting_id_(reporting_id) {
}

void AppCommandPingDelegate::OnLaunchResult(HRESULT result) {
  SendPing(PingEvent::EVENT_APP_COMMAND_BEGIN,
           SUCCEEDED(result) ? PingEvent::EVENT_RESULT_SUCCESS :
           PingEvent::EVENT_RESULT_ERROR,
           result);
}

void AppCommandPingDelegate::OnObservationFailure(HRESULT result) {
  SendPing(PingEvent::EVENT_APP_COMMAND_COMPLETE,
           PingEvent::EVENT_RESULT_ERROR,
           result);
}

void AppCommandPingDelegate::OnCommandCompletion(DWORD exit_code) {
  PingEvent::Results result = PingEvent::EVENT_RESULT_SUCCESS;
  int error_code = 0;

  switch (exit_code) {
    case ERROR_SUCCESS_REBOOT_REQUIRED:
      result = PingEvent::EVENT_RESULT_SUCCESS_REBOOT;
      break;
    case ERROR_SUCCESS:
      result = PingEvent::EVENT_RESULT_SUCCESS;
      break;
    default:
      result = PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER;
      error_code = exit_code;
      break;
  }

  SendPing(PingEvent::EVENT_APP_COMMAND_COMPLETE, result, error_code);
}

void AppCommandPingDelegate::SendPing(PingEvent::Types type,
                                      PingEvent::Results result,
                                      int error_code) {
  PingEventPtr ping_event(
      new PingEvent(type, result, error_code, reporting_id_));

  Ping ping(is_machine_, session_id_, CString());
  std::vector<CString> apps;
  apps.push_back(app_guid_);
  ping.LoadAppDataFromRegistry(apps);
  ping.BuildAppsPing(ping_event);
  SendReliablePing(&ping, true);  // true == is_fire_and_forget
}

}  // namespace omaha
