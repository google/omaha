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

#include "omaha/tools/omahacompatibility/console_writer.h"
#include <Windows.h>
#include <tchar.h>
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"
#include "omaha/tools/omahacompatibility/common/ping_observer.h"

namespace omaha {

CString ConsoleWriter::PingTypeToString(PingEvent::Types type)  {
  switch (type) {
    case PingEvent::EVENT_UNKNOWN:
      return _T("EVENT_UNKNOWN");
    case PingEvent::EVENT_INSTALL_DOWNLOAD_FINISH:
      return _T("EVENT_INSTALL_DOWNLOAD_FINISH");
    case PingEvent::EVENT_INSTALL_COMPLETE:
      return _T("EVENT_INSTALL_COMPLETE");
    case PingEvent::EVENT_UPDATE_COMPLETE:
      return _T("EVENT_UPDATE_COMPLETE");
    case PingEvent::EVENT_UNINSTALL:
      return _T("EVENT_UNINSTALL");
    case PingEvent::EVENT_INSTALL_DOWNLOAD_START:
      return _T("EVENT_INSTALL_DOWNLOAD_START");
    case PingEvent::EVENT_INSTALL_INSTALLER_START:
      return _T("EVENT_INSTALL_INSTALLER_START");
    case PingEvent::EVENT_INSTALL_APPLICATION_BEGIN:
      return _T("EVENT_INSTALL_APPLICATION_BEGIN");

    // Install Setup events.
    case PingEvent::EVENT_SETUP_INSTALL_BEGIN:
      return _T("EVENT_SETUP_INSTALL_BEGIN");
    case PingEvent::EVENT_SETUP_INSTALL_COMPLETE:
      return _T("EVENT_SETUP_INSTALL_COMPLETE");

    // Register Product Events.
    case PingEvent::EVENT_REGISTER_PRODUCT_COMPLETE:
      return _T("EVENT_REGISTER_PRODUCT_COMPLETE");

    // Update Events.
    case PingEvent::EVENT_UPDATE_APPLICATION_BEGIN:
      return _T("EVENT_UPDATE_APPLICATION_BEGIN");
    case PingEvent::EVENT_UPDATE_DOWNLOAD_START:
      return _T("EVENT_UPDATE_DOWNLOAD_START");
    case PingEvent::EVENT_UPDATE_DOWNLOAD_FINISH:
      return _T("EVENT_UPDATE_DOWNLOAD_FINISH");
    case PingEvent::EVENT_UPDATE_INSTALLER_START:
      return _T("EVENT_UPDATE_INSTALLER_START");

    // Self-update Setup events.
    case PingEvent::EVENT_SETUP_UPDATE_BEGIN:
      return _T("EVENT_SETUP_UPDATE_BEGIN");
    case PingEvent::EVENT_SETUP_UPDATE_COMPLETE:
      return _T("EVENT_SETUP_UPDATE_COMPLETE");

    // Other events.
    case PingEvent::EVENT_INSTALL_OEM_FIRST_CHECK:
      return _T("EVENT_INSTALL_OEM_FIRST_CHECK");

    // Failure report events - not part of the normal flow.
    case PingEvent::EVENT_SETUP_INSTALL_FAILURE:
      return _T("EVENT_SETUP_INSTALL_FAILURE");
    case PingEvent::EVENT_SETUP_COM_SERVER_FAILURE:
      return _T("EVENT_SETUP_COM_SERVER_FAILURE");
    case PingEvent::EVENT_SETUP_UPDATE_FAILURE:
      return _T("EVENT_SETUP_UPDATE_FAILURE");
    default:
      return _T("Unknown");
  }
}

CString ConsoleWriter::PingResultToString(PingEvent::Results result)  {
  switch (result) {
    case PingEvent::EVENT_RESULT_ERROR:
      return _T("EVENT_RESULT_ERROR");
    case PingEvent::EVENT_RESULT_SUCCESS:
      return _T("EVENT_RESULT_SUCCESS");
    case PingEvent::EVENT_RESULT_SUCCESS_REBOOT:
      return _T("EVENT_RESULT_SUCCESS_REBOOT");
    case PingEvent::EVENT_RESULT_CANCELLED:
      return _T("EVENT_RESULT_CANCELLED");
    case PingEvent::EVENT_RESULT_INSTALLER_ERROR_MSI:
      return _T("EVENT_RESULT_INSTALLER_ERROR_MSI");
    case PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER:
      return _T("EVENT_RESULT_INSTALLER_ERROR_OTHER");
    case PingEvent::EVENT_RESULT_NOUPDATE:
      return _T("EVENT_RESULT_NOUPDATE");
    case PingEvent::EVENT_RESULT_INSTALLER_ERROR_SYSTEM:
      return _T("EVENT_RESULT_INSTALLER_ERROR_SYSTEM");
    case PingEvent::EVENT_RESULT_UPDATE_DEFERRED:
      return _T("EVENT_RESULT_UPDATE_DEFERRED");
    default:
      return _T("unknown result");
  }
}

bool ConsoleWriter::IsUpdateCompletedEvent(const CString& app_guid,
                                           const PingEvent& ping) {
  return app_guid == app_guid_ &&
         ping.event_type() == PingEvent::EVENT_UPDATE_COMPLETE;
}

bool ConsoleWriter::IsInstallCompletedEvent(const CString& app_guid,
                                            const PingEvent& ping) {
  return app_guid == app_guid_ &&
         ping.event_type() == PingEvent::EVENT_INSTALL_COMPLETE;
}

void ConsoleWriter::Observe(const AppRequestData& data) {
  PingEventVector::const_iterator iter = data.ping_events_begin();
  for (; iter != data.ping_events_end(); ++iter) {
    PingEvent ping = *iter;
    CString msg;
    msg.Format(_T("\nPing App = %s, Type = %s, Result = %s, Error = %d\n"),
               GuidToString(data.app_data().app_guid()),
               PingTypeToString(ping.event_type()),
               PingResultToString(ping.event_result()),
               ping.error_code());
    printf("%S", msg);
    CORE_LOG(L1, (msg));

    if (IsInstallCompletedEvent(GuidToString(data.app_data().app_guid()),
                                ping)) {
      __mutexScope(lock_);
      install_result_ = ping.event_result();
      install_completed_ = true;
    } else if (IsUpdateCompletedEvent(GuidToString(data.app_data().app_guid()),
                                      ping)) {
      __mutexScope(lock_);
      update_result_ = ping.event_result();
      update_completed_ = true;
    }
  }
}

}  // namespace omaha
