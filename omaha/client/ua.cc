// Copyright 2007-2010 Google Inc.
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

// TODO(omaha): Dig out the RefHolder in scope_guard.h so we can use const
// references instead pointers. This TODO was added for some code that no longer
// exists, but it is still a good idea.

#include "omaha/client/ua.h"
#include "omaha/client/ua_internal.h"
#include <windows.h>
#include <atlstr.h>
#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/program_instance.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/utils.h"
#include "omaha/client/install_apps.h"
#include "omaha/client/install_self.h"
#include "omaha/client/client_metrics.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/event_logger.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/ping.h"

// Design Notes:
// Following are the mutexes that are taken by the worker
// 1. SingleUpdateWorker. Only taken by the update worker.
// 2. SingleInstallWorker. This is application specific. Only taken by the
//    install worker and for the specific application.
// 3. Before install, the install manager takes the global install lock.
// 4. A key thing to add to this code is after taking the install lock,
//    to validate that the version of the applicaion that is present in the
//    registry is the same as that we queried for. The reason to do this
//    is to ensure that there are no races between update and install workers.
// 5. Termination of the worker happens because of four reasons:
//    a. Shutdown event - Only applicable to the update worker. When this event
//       is signalled, the main thread comes out of the wait. It then tries to
//       destroy the contained thread pool, which causes a timed wait for the
//       worker thread. The worker thread is notified by setting a
//       cancelled flag on the worker.
//    b. Install completes, user closes UI - Only applicable for the
//       interactive installs. In this case the main thread comes out of
//       the message loop and deletes the thread pool. The delete happens
//       immediately, since the worker is doing nothing.
//    c. User cancels install - Only applicable in case if interactive installs.
//       The main thread sets the cancelled flag on the workerjob and comes out
//       of the message loop. It then tries to delete the thread pool, causing
//       a timed wait. The worker job queries the cancelled flag periodically
//       and quits as soon as possible.
//    d. The update worker completes - In this case we do not run on a thread
//       pool.

namespace omaha {

namespace {

void WriteUpdateAppsStartEvent(bool is_machine) {
  GoogleUpdateLogEvent update_event(EVENTLOG_INFORMATION_TYPE,
                                    kWorkerStartEventId,
                                    is_machine);
  update_event.set_event_desc(_T("Update Apps start"));

  ConfigManager& cm = *ConfigManager::Instance();

  int au_check_period_ms = cm.GetAutoUpdateTimerIntervalMs();
  int time_since_last_checked_sec = cm.GetTimeSinceLastCheckedSec(is_machine);
  bool is_period_overridden = false;
  int last_check_period_ms = cm.GetLastCheckPeriodSec(&is_period_overridden);

  CString event_text;
  SafeCStringFormat(&event_text,
      _T("AuCheckPeriodMs=%d, TimeSinceLastCheckedSec=%d, ")
      _T("LastCheckedPeriodSec=%d"),
      au_check_period_ms, time_since_last_checked_sec, last_check_period_ms);

  update_event.set_event_text(event_text);
  update_event.WriteEvent();
}

}  // namespace

namespace internal {

// Ensures there is only one instance of /ua per session per Omaha instance.
bool EnsureSingleUAProcess(bool is_machine, ProgramInstance** instance) {
  ASSERT1(instance);
  ASSERT1(!*instance);
  NamedObjectAttributes single_ua_process_attr;
  GetNamedObjectAttributes(kUpdateAppsSingleInstance,
                           is_machine,
                           &single_ua_process_attr);

  *instance = new ProgramInstance(single_ua_process_attr.name);
  return !(*instance)->EnsureSingleInstance();
}

}  // namespace internal

// Always checks whether it should uninstall.
// Checks for updates of all apps if the required period has elapsed, it is
// being run on-demand, or an uninstall seems necessary. It will also send a
// self-update failure ping in these cases if necessary.
//
// Calls UpdateAllApps(), which will call IAppBundle::updateAllApps(), even in
// cases where an uninstall seems necessary. This allows an uninstall ping to
// be sent for any uninstalled apps. Because the COM server does not know about
// uninstall, the COM server will also do a final update check for the remaining
// app - should be Omaha. It's possible that this update check could result in
// a self-update, in which case the uninstall may fail and be delayed an hour.
// See http://b/2814535.
// Since all pings are sent by the AppBundle destructor, if the bundle has
// normal or uninstall pings need, the network request could delay the exiting
// of the COM server beyond the point where this client releases the IAppBundle.
// and launches /uninstall. This could cause uninstall to fail if the ping takes
// a long time.
//
// TODO(omaha): Test this method as it is very important.
HRESULT UpdateApps(bool is_machine,
                   bool is_interactive,
                   bool is_on_demand,
                   const CString& install_source,
                   const CString& display_language,
                   bool* has_ui_been_displayed) {
  CORE_LOG(L1, (_T("[UpdateApps]")));
  ASSERT1(has_ui_been_displayed);

  WriteUpdateAppsStartEvent(is_machine);

  scoped_ptr<ProgramInstance> single_ua_process;

  if (internal::EnsureSingleUAProcess(is_machine, address(single_ua_process))) {
    OPT_LOG(L1, (_T("[Another worker is already running. Exiting.]")));
    ++metric_client_another_update_in_progress;
    return GOOPDATE_E_UA_ALREADY_RUNNING;
  }

  if (ConfigManager::Instance()->CanUseNetwork(is_machine)) {
    VERIFY1(SUCCEEDED(Ping::SendPersistedPings(is_machine)));
  }

  // Generate a session ID for network accesses.
  CString session_id;
  VERIFY1(SUCCEEDED(GetGuid(&session_id)));

  // A tentative uninstall check is done here. There are stronger checks,
  // protected by locks, which are done by Setup.
  size_t num_clients(0);
  const bool is_uninstall =
      FAILED(app_registry_utils::GetNumClients(is_machine, &num_clients)) ||
      num_clients <= 1;
  CORE_LOG(L4, (_T("[UpdateApps][registered apps: %u]"), num_clients));

  const bool should_check_for_updates =
      goopdate_utils::ShouldCheckForUpdates(is_machine);

  if (is_uninstall) {
    // TODO(omaha3): The interactive /ua process will not exit without user
    // interaction. This could cause the uninstall to fail.
    CORE_LOG(L1, (_T("[/ua launching /uninstall]")));
    return goopdate_utils::LaunchUninstallProcess(is_machine);
  }

  if (!(is_on_demand || should_check_for_updates)) {
    OPT_LOG(L1, (_T("[Update check not needed at this time]")));
    return S_OK;
  }

  HRESULT hr = UpdateAllApps(is_machine,
                             is_interactive,
                             install_source,
                             display_language,
                             session_id,
                             has_ui_been_displayed);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[UpdateAllApps failed][0x%08x]"), hr));
  }
  return hr;
}

}  // namespace omaha
