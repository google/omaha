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

#include "omaha/worker/worker_event_logger.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/event_logger.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/worker/job.h"

namespace omaha {

const TCHAR* const kUpdateCheckEventDesc = _T("Update check. Status = 0x%08x");
const TCHAR* const kUpdateEventDesc           = _T("Application update");
const TCHAR* const kInstallEventDesc          = _T("Application install");
const TCHAR* const kUpdateAppsWorkerEventDesc = _T("Update worker start");

void WriteUpdateCheckEvent(bool is_machine, HRESULT hr, const CString& text) {
  const int event_type = SUCCEEDED(hr) ? EVENTLOG_INFORMATION_TYPE :
                                         EVENTLOG_WARNING_TYPE;
  const int event_id   = kUpdateCheckEventId;

  CString event_description, event_text;
  event_description.Format(kUpdateCheckEventDesc, hr);

  CString url;
  VERIFY1(SUCCEEDED(ConfigManager::Instance()->GetUpdateCheckUrl(&url)));
  event_text.Format(_T("url=%s\n%s"), url, text);

  GoogleUpdateLogEvent update_check_event(event_type, event_id, is_machine);
  update_check_event.set_event_desc(event_description);
  update_check_event.set_event_text(event_text);
  update_check_event.WriteEvent();
}

void WriteJobCompletedEvent(bool is_machine, const Job& job) {
  int type = IsCompletionSuccess(job.info()) ? EVENTLOG_INFORMATION_TYPE :
                                               EVENTLOG_WARNING_TYPE;

  GoogleUpdateLogEvent update_event(type, kUpdateEventId, is_machine);
  CString desc = job.is_update() ? kUpdateEventDesc :
                                   kInstallEventDesc;
  update_event.set_event_desc(desc);
  CString event_text;
  event_text.AppendFormat(_T("App=%s, Ver=%s, PrevVer=%s, Status=0x%08x"),
                          GuidToString(job.app_data().app_guid()),
                          job.app_data().version(),
                          job.app_data().previous_version(),
                          job.info().error_code);
  update_event.set_event_text(event_text);
  update_event.WriteEvent();
}

void WriteUpdateAppsWorkerStartEvent(bool is_machine) {
  GoogleUpdateLogEvent update_event(EVENTLOG_INFORMATION_TYPE,
                                    kWorkerStartEventId,
                                    is_machine);
  update_event.set_event_desc(kUpdateAppsWorkerEventDesc);

  ConfigManager& cm = *ConfigManager::Instance();

  int au_check_period_ms = cm.GetAutoUpdateTimerIntervalMs();
  int time_since_last_checked_sec = cm.GetTimeSinceLastCheckedSec(is_machine);
  bool is_period_overridden = false;
  int last_check_period_ms = cm.GetLastCheckPeriodSec(&is_period_overridden);

  CString event_text;
  event_text.Format(
    _T("AuCheckPeriodMs=%d, TimeSinceLastCheckedSec=%d, ")
    _T("LastCheckedPeriodSec=%d"),
    au_check_period_ms, time_since_last_checked_sec, last_check_period_ms);

  update_event.set_event_text(event_text);
  update_event.WriteEvent();
}

}  // namespace omaha
