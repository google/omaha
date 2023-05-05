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

#include "omaha/client/ua.h"

#include <windows.h>
#include <atlstr.h>
#include <stdlib.h>

#include "base/rand_util.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/program_instance.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/utils.h"
#include "omaha/base/time.h"
#include "omaha/client/install_apps.h"
#include "omaha/client/install_self.h"
#include "omaha/client/client_metrics.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line_builder.h"
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
// 6. There is a random delay before triggering the actual update check.
//    This delay avoids the situation where many update checks could happen at
//    the same time, for instance at the minute mark, when the client computers
//    have their clocks synchronized.

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

// Ensures there is only one instance of /ua per session per Omaha instance.
bool EnsureSingleUAProcess(bool is_machine,
                           std::unique_ptr<ProgramInstance>* instance) {
  ASSERT1(instance);
  NamedObjectAttributes single_ua_process_attr;
  GetNamedObjectAttributes(kUpdateAppsSingleInstance,
                           is_machine,
                           &single_ua_process_attr);

  instance->reset(new ProgramInstance(single_ua_process_attr.name));
  return !instance->get()->EnsureSingleInstance();
}

bool IsUpdateAppsHourlyJitterDisabled() {
  DWORD value = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueDisableUpdateAppsHourlyJitter,
                                 &value))) {
    return value != 0;
  } else {
    return false;
  }
}

}  // namespace

// Returns false if "RetryAfter" in the registry is set to a time greater than
// the current time. Otherwise, returns true if the absolute difference between
// time moments is greater than the interval between update checks.
// Deals with clocks rolling backwards, in scenarios where the clock indicates
// some time in the future, for example next year, last_checked_ is updated to
// reflect that time, and then the clock is adjusted back to present.
// In the case where the update check period is not overriden, the function
// introduces an hourly jitter for 10% of the function calls when the time
// interval falls in the range: [LastCheckPeriodSec, LastCheckPeriodSec + 1hr).
// No update check is made if the "updates suppressed" period is in effect.
bool ShouldCheckForUpdates(bool is_machine) {
  ConfigManager* cm = ConfigManager::Instance();

  if (!cm->CanRetryNow(is_machine)) {
    return false;
  }

  bool is_period_overridden = false;
  const int update_interval = cm->GetLastCheckPeriodSec(&is_period_overridden);
  if (0 == update_interval) {
    ASSERT1(is_period_overridden);
    OPT_LOG(L1, (_T("[ShouldCheckForUpdates returned 0][checks disabled]")));
    return false;
  }

  const int time_since_last_check = cm->GetTimeSinceLastCheckedSec(is_machine);

  bool should_check_for_updates = false;

  if (ConfigManager::Instance()->AreUpdatesSuppressedNow()) {
    should_check_for_updates = false;
  } else if (time_since_last_check < update_interval) {
    // Too soon.
    should_check_for_updates = false;
  } else if (update_interval <= time_since_last_check &&
             time_since_last_check < update_interval + kSecondsPerHour) {
    // Defer some checks if not overridden or if the feature is not disabled.
    // Do not skip checks when errors happen in the RNG.
    if (!is_period_overridden && !IsUpdateAppsHourlyJitterDisabled()) {
      const int kPercentageToSkip = 10;    // skip 10% of the checks.
      unsigned int random_value = 0;
      if (!RandUint32(&random_value)) {
        random_value = 0;
      }
      should_check_for_updates = random_value % 100 < 100 - kPercentageToSkip;
    } else {
      should_check_for_updates = true;
    }
  } else {
    ASSERT1(time_since_last_check >= update_interval + kSecondsPerHour);
    should_check_for_updates = true;
  }

  OPT_LOG(L3, (_T("[ShouldCheckForUpdates returned %d][%u]"),
               should_check_for_updates, is_period_overridden));
  return should_check_for_updates;
}

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

  std::unique_ptr<ProgramInstance> single_ua_process;

  if (EnsureSingleUAProcess(is_machine, &single_ua_process)) {
    OPT_LOG(L1, (_T("[Another worker is already running. Exiting.]")));
    ++metric_client_another_update_in_progress;
    return GOOPDATE_E_UA_ALREADY_RUNNING;
  }

  VERIFY_SUCCEEDED(ConfigManager::Instance()->SetLastStartedAU(is_machine));

  if (ConfigManager::Instance()->CanUseNetwork(is_machine)) {
    VERIFY_SUCCEEDED(Ping::SendPersistedPings(is_machine));
  }

  // Generate a session ID for network accesses.
  CString session_id;
  VERIFY_SUCCEEDED(GetGuid(&session_id));

  // A tentative uninstall check is done here. There are stronger checks,
  // protected by locks, which are done by Setup.
  size_t num_clients(0);
  const bool is_uninstall =
      FAILED(app_registry_utils::GetNumClients(is_machine, &num_clients)) ||
      num_clients <= 1;
  CORE_LOG(L4, (_T("[UpdateApps][registered apps: %u]"), num_clients));

  if (is_uninstall) {
    // TODO(omaha3): The interactive /ua process will not exit without user
    // interaction. This could cause the uninstall to fail.
    CORE_LOG(L1, (_T("[/ua launching /uninstall]")));
    return goopdate_utils::LaunchUninstallProcess(is_machine);
  }

  // We first install any apps that need to be force-installed according to
  // policy set by a domain administrator.
  HRESULT hr = InstallForceInstallApps(is_machine,
                                       is_interactive,
                                       install_source,
                                       display_language,
                                       session_id,
                                       has_ui_been_displayed);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[InstallForceInstallApps failed][%#x]"), hr));
  }

  // InstallForceInstallApps creates a BundleAtlModule instance on the stack, so
  // we reset the _pAtlModule to allow for a fresh ATL module for UpdateAllApps.
  _pAtlModule = NULL;

  // Generate a new session ID for UpdateAllApps.
  VERIFY_SUCCEEDED(GetGuid(&session_id));

  const bool should_check_for_updates = ShouldCheckForUpdates(is_machine);
  if (!(is_on_demand || should_check_for_updates)) {
    OPT_LOG(L1, (_T("[Update check not needed at this time]")));
    return S_OK;
  }

  // Waits a while before starting checking for updates. Usually, the wait
  // is a random value in the range [0, 60000) miliseconds (up to one minute).
  const int au_jitter_ms(ConfigManager::Instance()->GetAutoUpdateJitterMs());
  if (au_jitter_ms > 0) {
    OPT_LOG(L1, (_T("[Applying update check jitter][%d]"), au_jitter_ms));
    ::Sleep(au_jitter_ms);
  }

  hr = UpdateAllApps(is_machine,
                     is_interactive,
                     install_source,
                     display_language,
                     session_id,
                     has_ui_been_displayed);
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[UpdateAllApps failed][0x%08x]"), hr));
  }
  return hr;
}

}  // namespace omaha
