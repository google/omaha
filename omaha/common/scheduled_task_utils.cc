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

#include "omaha/common/scheduled_task_utils.h"
#include "omaha/common/scheduled_task_utils_internal.h"
#include <lmcons.h>
#include <lmsname.h>
#include <mstask.h>
#include <atlsecurity.h>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/scoped_ptr_cotask.h"
#include "omaha/base/service_utils.h"
#include "omaha/base/string.h"
#include "omaha/base/system_info.h"
#include "omaha/base/time.h"
#include "omaha/base/timer.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/client/resource.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

namespace scheduled_task_utils {

namespace internal {

CString GetCurrentTaskNameCore(bool is_machine) {
  UTIL_LOG(L3, (_T("[GetCurrentTaskNameCore][%d]"), is_machine));

  CString default_name(GetDefaultGoopdateTaskName(is_machine,
                                                  COMMANDLINE_MODE_CORE));
  return goopdate_utils::GetCurrentVersionedName(is_machine,
                                                 kRegValueTaskNameC,
                                                 default_name);
}

HRESULT CreateAndSetVersionedTaskNameCoreInRegistry(
    bool is_machine) {
  UTIL_LOG(L3, (_T("[CreateAndSetVersionedTaskNameCoreInRegistry][%d]"),
                is_machine));

  CString default_name(GetDefaultGoopdateTaskName(is_machine,
                                                  COMMANDLINE_MODE_CORE));
  return goopdate_utils::CreateAndSetVersionedNameInRegistry(
             is_machine,
             default_name,
             kRegValueTaskNameC);
}

CString GetCurrentTaskNameUA(bool is_machine) {
  UTIL_LOG(L3, (_T("[GetCurrentTaskNameUA][%d]"), is_machine));

  CString default_name(GetDefaultGoopdateTaskName(is_machine,
                                                  COMMANDLINE_MODE_UA));
  return goopdate_utils::GetCurrentVersionedName(is_machine,
                                                 kRegValueTaskNameUA,
                                                 default_name);
}

HRESULT CreateAndSetVersionedTaskNameUAInRegistry(bool machine) {
  UTIL_LOG(L3, (_T("[CreateAndSetVersionedTaskNameUAInRegistry][%d]"),
                machine));

  CString default_name(GetDefaultGoopdateTaskName(machine,
                                                  COMMANDLINE_MODE_UA));
  return goopdate_utils::CreateAndSetVersionedNameInRegistry(
             machine,
             default_name,
             kRegValueTaskNameUA);
}

bool IsInstalledScheduledTask(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return false;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  UTIL_LOG(L3, (_T("[IsInstalledScheduledTask returned][0x%x]"), hr));
  return hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

DWORD GetScheduledTaskPriority(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return 0;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetScheduledTaskPriority][Activate failed][0x%x]"), hr));
    return 0;
  }

  DWORD priority = 0;
  hr = task->GetPriority(&priority);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.GetMostRecentRunTime failed][0x%x]"), hr));
    return 0;
  }

  ASSERT1(priority);
  return priority;
}

bool HasScheduledTaskEverRun(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return false;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[HasScheduledTaskEverRun][Activate failed][0x%x]"), hr));
    return false;
  }

  SYSTEMTIME recent_run_time = {0};
  hr = task->GetMostRecentRunTime(&recent_run_time);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.GetMostRecentRunTime failed][0x%x]"), hr));
    return false;
  }

  // hr == SCHED_S_TASK_HAS_NOT_RUN if the task has never run.
  return hr == S_OK;
}

HRESULT GetScheduledTaskStatus(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetScheduledTaskStatus: Activate failed][0x%x]"), hr));
    return hr;
  }

  HRESULT task_status(S_OK);
  hr = task->GetStatus(&task_status);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.GetStatus failed][0x%x]"), hr));
    return hr;
  }

  return task_status;
}

HRESULT GetScheduledTaskExitCode(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.Activate failed][0x%x]"), hr));
    return hr;
  }

  DWORD exit_code(0);
  hr = task->GetExitCode(&exit_code);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.GetExitCode failed][0x%x]"), hr));
    return hr;
  }

  return hr == SCHED_S_TASK_HAS_NOT_RUN ? hr : exit_code;
}

HRESULT StartScheduledTask(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  if (v2::IsTaskScheduler2APIAvailable()) {
    return v2::StartScheduledTask(task_name);
  }

  if (GetScheduledTaskStatus(task_name) == SCHED_S_TASK_RUNNING) {
    return S_OK;
  }

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.Activate failed][0x%x]"), hr));
    return hr;
  }

  hr = task->Run();
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.Run failed][0x%x]"), hr));
    return hr;
  }

  return hr;
}

HRESULT StopScheduledTask(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  if (v2::IsTaskScheduler2APIAvailable()) {
    return v2::StopScheduledTask(task_name);
  }

  if (GetScheduledTaskStatus(task_name) != SCHED_S_TASK_RUNNING) {
    return S_OK;
  }

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.Activate failed][0x%x]"), hr));
    return hr;
  }

  hr = task->Terminate();
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.Run failed][0x%x]"), hr));
    return hr;
  }

  return hr;
}

HRESULT CreateLogonTrigger(ITask* task) {
  ASSERT1(task);

  CComPtr<ITaskTrigger> trigger;
  WORD index = 0;

  // Create a trigger to run on every user logon.
  HRESULT hr = task->CreateTrigger(&index, &trigger);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.CreateTrigger failed][0x%x]"), hr));
    return hr;
  }

  TASK_TRIGGER trigger_config = {0};
  trigger_config.cbTriggerSize = sizeof(trigger_config);
  // These are required parameters. A past start date is good.
  trigger_config.wBeginDay = 1;
  trigger_config.wBeginMonth = 1;
  trigger_config.wBeginYear = 1999;

  // Run on every user logon.
  trigger_config.TriggerType = TASK_EVENT_TRIGGER_AT_LOGON;

  hr = trigger->SetTrigger(&trigger_config);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskTrigger.SetTrigger failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT CreatePeriodicTrigger(ITask* task, bool create_hourly_trigger) {
  ASSERT1(task);

  CComPtr<ITaskTrigger> trigger;
  WORD index = 0;

  // Create a trigger to run every day.
  HRESULT hr = task->CreateTrigger(&index, &trigger);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.CreateTrigger failed][0x%x]"), hr));
    return hr;
  }

  // Start time set to 5 minutes from the current time.
  time64 start_time = GetCurrent100NSTime() + kScheduledTaskDelayStartNs;
  SYSTEMTIME sys_time = Time64ToSystemTime(start_time);
  SYSTEMTIME locale_time = {0};
  if (!SystemTimeToTzSpecificLocalTime(NULL, &sys_time, &locale_time)) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[SystemTimeToTzSpecificLocalTime failed][0x%x]"), hr));
    return hr;
  }

  TASK_TRIGGER trigger_config = {0};
  trigger_config.cbTriggerSize = sizeof(trigger_config);
  trigger_config.wBeginYear = locale_time.wYear;
  trigger_config.wBeginMonth = locale_time.wMonth;
  trigger_config.wBeginDay = locale_time.wDay;
  trigger_config.wStartHour = locale_time.wHour;
  trigger_config.wStartMinute = locale_time.wMinute;

  trigger_config.TriggerType = TASK_TIME_TRIGGER_DAILY;
  trigger_config.Type.Daily.DaysInterval = kScheduledTaskIntervalDays;

  if (create_hourly_trigger) {
    // The task will be run daily at kScheduledTaskDurationMinutes intervals.
    // And the task will be repeated every au_timer_interval_minutes within a
    // single kScheduledTaskDurationMinutes interval.
    int au_timer_interval_minutes =
        ConfigManager::Instance()->GetAutoUpdateTimerIntervalMs() / (60 * 1000);
    ASSERT1(au_timer_interval_minutes > 0 &&
            au_timer_interval_minutes < kScheduledTaskDurationMinutes);

    trigger_config.MinutesDuration = kScheduledTaskDurationMinutes;
    trigger_config.MinutesInterval = au_timer_interval_minutes;
  }

  hr = trigger->SetTrigger(&trigger_config);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskTrigger.SetTrigger failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT CreateScheduledTask(ITask* task,
                            const TCHAR* task_path,
                            const TCHAR* task_parameters,
                            const TCHAR* task_comment,
                            bool is_machine,
                            bool create_logon_trigger,
                            bool create_daily_trigger,
                            bool create_hourly_trigger) {
  ASSERT1(task);
  ASSERT1(task_path && *task_path);
  ASSERT1(task_parameters);
  ASSERT1(task_comment && *task_comment);
  ASSERT1(create_logon_trigger || create_daily_trigger);
  ASSERT1(!create_logon_trigger || (create_logon_trigger && is_machine));
  ASSERT1(!create_hourly_trigger ||
          (create_hourly_trigger && create_daily_trigger));

  UTIL_LOG(L3, (_T("[CreateScheduledTask][%s][%s][%d]"),
                task_path, task_parameters, is_machine));

  HRESULT hr = task->SetApplicationName(task_path);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.SetApplicationName failed][0x%x]"), hr));
    return hr;
  }

  hr = task->SetParameters(task_parameters);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.SetParameters failed][0x%x]"), hr));
    return hr;
  }

  hr = task->SetComment(task_comment);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.SetComment failed][0x%x]"), hr));
    return hr;
  }

  if (is_machine) {
    // Run using SYSTEM credentials, by passing in an empty username string.
    hr = task->SetAccountInformation(_T(""), NULL);
  } else {
    // Run as current user.
    // For the user task, we set TASK_FLAG_RUN_ONLY_IF_LOGGED_ON, so that we do
    // not need the user password for task creation.
    hr = task->SetFlags(TASK_FLAG_RUN_ONLY_IF_LOGGED_ON);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[ITask.SetFlags failed][0x%x]"), hr));
      return hr;
    }

    CString user_name;
    DWORD buffer_size = UNLEN + 1;
    if (!::GetUserName(CStrBuf(user_name, buffer_size), &buffer_size)) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LE, (_T("[::GetUserName failed][0x%x]"), hr));
      return hr;
    }
    hr = task->SetAccountInformation(user_name, NULL);
  }

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.SetAccountInformation failed][0x%x]"), hr));
    return hr;
  }

  // The default is to run for a finite number of days. We want to run
  // indefinitely.
  // Due to a bug introduced in Vista, and propogated to Windows 7, setting the
  // MaxRunTime to INFINITE results in the task only running for 72 hours. For
  // these operating systems, setting the RunTime to "INFINITE - 1" gets the
  // desired behavior of allowing an "infinite" run of the task.
  DWORD max_time = INFINITE - (SystemInfo::IsRunningOnVistaOrLater() ? 1 : 0);
  hr = task->SetMaxRunTime(max_time);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITask.SetMaxRunTime failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITaskTrigger> trigger;
  WORD index = 0;

  if (create_logon_trigger && is_machine) {
    // Create a trigger to run on every user logon. Non-admin users are not able
    // to create logon triggers, so we create only for machine.
    hr = CreateLogonTrigger(task);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (create_daily_trigger) {
    hr = CreatePeriodicTrigger(task, create_hourly_trigger);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Save task.
  CComQIPtr<IPersistFile> persist(task);
  if (!persist) {
    hr = E_NOINTERFACE;
    UTIL_LOG(LE, (_T("[ITask.QueryInterface IPersistFile failed][0x%x]"), hr));
    return hr;
  }

  hr = persist->Save(NULL, TRUE);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IPersistFile.Save failed][0x%x]"), hr));
    return hr;
  }

  if (is_machine) {
    return S_OK;
  }

  // Adjust privileges to explicitly allow the current user to be able to
  // manipulate this task. User applications, and consequently, Omaha, can be
  // installed in an elevated mode. This can happen, for instance, if the user
  // installs on XP, then upgrades to Vista. Or chooses "Run as Administrator"
  // when running the meta-installer on Vista. Subsequently, Omaha running at
  // medium integrity needs to be able to manipulate the installed task.
  scoped_ptr_cotask<OLECHAR> job_file;
  hr = persist->GetCurFile(address(job_file));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IPersistFile.GetCurFile failed][0x%x]"), hr));
    return hr;
  }

  persist.Release();

  CAccessToken token;
  CSid current_sid;
  if (!token.GetEffectiveToken(TOKEN_QUERY) || !token.GetUser(&current_sid)) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[Failed to get current user sid][0x%x]"), hr));
    return hr;
  }

  hr = AddAllowedAce(job_file.get(),
                     SE_FILE_OBJECT,
                     current_sid,
                     FILE_ALL_ACCESS,
                     0);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Could not adjust DACL][%s][0x%x]"), job_file.get(), hr));
    return hr;
  }

  return S_OK;
}

HRESULT UpgradeScheduledTask(const TCHAR* task_name,
                             const TCHAR* task_path,
                             const TCHAR* task_parameters,
                             const TCHAR* task_comment,
                             bool is_machine,
                             bool create_logon_trigger,
                             bool create_daily_trigger,
                             bool create_hourly_trigger) {
  ASSERT1(task_name && *task_name);
  ASSERT1(IsInstalledScheduledTask(task_name));

  UTIL_LOG(L3, (_T("[UpgradeScheduledTask][%s][%s][%s][%d]"),
                task_name, task_path, task_parameters, is_machine));

  // TODO(Omaha): Perhaps pass the ITaskScheduler around where possible.
  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(task_name,
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[UpgradeScheduledTask][Activate failed][0x%x]"), hr));
    return hr;
  }

  // Delete existing triggers. CreateScheduledTask() will recreate them anew.
  WORD trigger_count(0);
  hr = task->GetTriggerCount(&trigger_count);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.GetTriggerCount failed][0x%x]"), hr));
    return hr;
  }

  for (int i = 0; i < trigger_count; ++i) {
    hr = task->DeleteTrigger(0);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[ITaskScheduler.DeleteTrigger failed][0x%x]"), hr));
      return hr;
    }
  }

  return CreateScheduledTask(task,
                             task_path,
                             task_parameters,
                             task_comment,
                             is_machine,
                             create_logon_trigger,
                             create_daily_trigger,
                             create_hourly_trigger);
}

// TODO(Omaha): Change the apis to avoid specifying hourly and daily triggers.
HRESULT InstallScheduledTask(const TCHAR* task_name,
                             const TCHAR* task_path,
                             const TCHAR* task_parameters,
                             const TCHAR* task_comment,
                             bool is_machine,
                             bool create_logon_trigger,
                             bool create_daily_trigger,
                             bool create_hourly_trigger) {
  if (IsInstalledScheduledTask(task_name)) {
    UninstallScheduledTask(task_name);
  }

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITask> task;
  hr = scheduler->NewWorkItem(task_name,
                              CLSID_CTask,
                              __uuidof(ITask),
                              reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.NewWorkItem failed][0x%x]"), hr));
    return hr;
  }

  return CreateScheduledTask(task,
                             task_path,
                             task_parameters,
                             task_comment,
                             is_machine,
                             create_logon_trigger,
                             create_daily_trigger,
                             create_hourly_trigger);
}

HRESULT UninstallScheduledTask(const TCHAR* task_name) {
  ASSERT1(task_name && *task_name);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  // Stop the task before deleting it. Ignore return value.
  VERIFY1(SUCCEEDED(StopScheduledTask(task_name)));

  // delete the task.
  hr = scheduler->Delete(task_name);
  if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
    UTIL_LOG(LE, (_T("[GetScheduledTaskStatus][Delete failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT UninstallScheduledTasks(const TCHAR* task_prefix) {
  ASSERT1(task_prefix && *task_prefix);

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IEnumWorkItems> enum_items;
  hr = scheduler->Enum(&enum_items);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.Enum failed][0x%x]"), hr));
    return hr;
  }

  TCHAR** task_names = NULL;
  DWORD task_count = 0;
  while (enum_items->Next(1, &task_names, &task_count) == S_OK) {
    ASSERT1(task_count == 1);
    scoped_co_task_ptr task_names_guard(task_names);
    scoped_co_task_ptr task_name_guard(task_names[0]);

    if (String_StartsWith(task_names[0], task_prefix, true)) {
      UninstallScheduledTask(task_names[0]);
    }
  }

  return S_OK;
}

// Returns the task name Omaha used to install in Omaha 1.2.x.
CString GetOmaha1LegacyTaskName(bool is_machine) {
  const TCHAR* const kLegacyOmaha1TaskNameMachine = _T("GoogleUpdateTask");
  const TCHAR* const kLegacyOmaha1TaskNameUser = _T("GoogleUpdateTaskUser");
  return is_machine ? kLegacyOmaha1TaskNameMachine : kLegacyOmaha1TaskNameUser;
}

// Returns the task name Omaha used to install in Omaha 2 before the
// "GoogleUpdate.exe does not run all the time" refactoring.
CString GetOmaha2LegacyTaskName(bool is_machine) {
  const TCHAR* kLegacyOmaha2TaskNameUserPrefix = _T("GoogleUpdateTaskUser");
  const TCHAR* kLegacyOmaha2TaskNameMachine = _T("GoogleUpdateTaskMachine");
  if (is_machine) {
    return kLegacyOmaha2TaskNameMachine;
  }

  CString task_name_user = kLegacyOmaha2TaskNameUserPrefix;
  CString user_sid;
  VERIFY1(SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid)));
  task_name_user += user_sid;
  return task_name_user;
}

HRESULT WaitForTaskStatus(const TCHAR* task_name, HRESULT status, int time_ms) {
  int kSleepBetweenRetriesMs = 100;
  Timer timer(true);
  while (timer.GetMilliseconds() < time_ms) {
    if (GetScheduledTaskStatus(task_name) == status) {
      return status;
    }
    ::Sleep(kSleepBetweenRetriesMs);
  }
  return GetScheduledTaskStatus(task_name);
}

namespace v2 {

bool IsTaskScheduler2APIAvailable() {
  CComPtr<ITaskService> task_service;
  return SUCCEEDED(task_service.CoCreateInstance(CLSID_TaskScheduler,
                                                 NULL,
                                                 CLSCTX_INPROC_SERVER));
}

HRESULT GetRegisteredTask(const TCHAR* task_name, IRegisteredTask** task) {
  ASSERT1(IsTaskScheduler2APIAvailable());
  ASSERT1(task_name && *task_name);
  ASSERT1(task);

  CComPtr<ITaskService> task_service;
  HRESULT hr = task_service.CoCreateInstance(CLSID_TaskScheduler,
                                             NULL,
                                             CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskService.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  hr = task_service->Connect(CComVariant(), CComVariant(),
                             CComVariant(), CComVariant());
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskService::Connect failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<ITaskFolder> task_folder;
  hr = task_service->GetFolder(CComBSTR(_T("\\")) , &task_folder);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Cannot get Root Folder pointer][0x%x]"), hr));
    return hr;
  }

  CComPtr<IRegisteredTask> registered_task;
  hr = task_folder->GetTask(CComBSTR(task_name), &registered_task);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Cannot get the registered task][0x%x]"), hr));
    return hr;
  }

  *task = registered_task.Detach();
  return S_OK;
}

bool IsScheduledTaskRunning(const TCHAR* task_name) {
  ASSERT1(IsTaskScheduler2APIAvailable());
  ASSERT1(task_name && *task_name);

  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);
  if (FAILED(hr)) {
    return false;
  }

  CComPtr<IRunningTaskCollection> running_task_collection;
  hr = registered_task->GetInstances(0, &running_task_collection);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRegisteredTask.GetInstances failed][0x%x]"), hr));
    return false;
  }

  long count = 0;
  hr = running_task_collection->get_Count(&count);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRunningTaskCollection.get_Count failed][0x%x]"), hr));
    return false;
  }

  return count > 0;
}

HRESULT StartScheduledTask(const TCHAR* task_name) {
  ASSERT1(IsTaskScheduler2APIAvailable());
  ASSERT1(task_name && *task_name);

  if (IsScheduledTaskRunning(task_name)) {
    return S_OK;
  }

  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);
  if (FAILED(hr)) {
    return hr;
  }

  hr = registered_task->Run(CComVariant(), NULL);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRegisteredTask.Run failed][0x%x]"), hr));
    return hr;
  }

  return hr;
}

HRESULT StopScheduledTask(const TCHAR* task_name) {
  ASSERT1(IsTaskScheduler2APIAvailable());
  ASSERT1(task_name && *task_name);

  if (!IsScheduledTaskRunning(task_name)) {
    return S_OK;
  }

  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IRunningTaskCollection> running_task_collection;
  hr = registered_task->GetInstances(0, &running_task_collection);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRegisteredTask.GetInstances failed][0x%x]"), hr));
    return hr;
  }

  long count = 0;
  hr = running_task_collection->get_Count(&count);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRunningTaskCollection.get_Count failed][0x%x]"), hr));
    return hr;
  }

  if (count <= 0) {
    return S_OK;
  }

  for (long i = 0; i < count; ++i) {
    CComPtr<IRunningTask> running_task;
    hr = running_task_collection->get_Item(CComVariant(i+1), &running_task);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[IRunningTaskCollection.get_Item][%d][0x%x]"), i, hr));
      return hr;
    }

    hr = running_task->Stop();
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[IRunningTask.Stop failed][%d][0x%x]"), i, hr));
      return hr;
    }
  }

  return S_OK;
}

}  // namespace v2

}  // namespace internal

CString GetDefaultGoopdateTaskName(bool is_machine, CommandLineMode mode) {
  ASSERT1(mode == COMMANDLINE_MODE_CORE || mode == COMMANDLINE_MODE_UA);

  CString task_name;
  if (is_machine) {
    task_name = kScheduledTaskNameMachinePrefix;
  } else {
    task_name = kScheduledTaskNameUserPrefix;
    CString user_sid;
    VERIFY1(SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid)));
    task_name += user_sid;
  }

  task_name += (mode == COMMANDLINE_MODE_CORE) ? kScheduledTaskNameCoreSuffix :
                                                 kScheduledTaskNameUASuffix;
  return task_name;
}

HRESULT InstallGoopdateTaskForMode(const TCHAR* task_path,
                                   bool is_machine,
                                   CommandLineMode mode) {
  ASSERT1(mode == COMMANDLINE_MODE_CORE || mode == COMMANDLINE_MODE_UA);

  CommandLineBuilder builder(mode);
  if (mode == COMMANDLINE_MODE_UA) {
    builder.set_install_source(kCmdLineInstallSource_Scheduler);
  }
  const CString task_parameters = builder.GetCommandLineArgs();

  CString company_name;
  VERIFY1(company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));
  CString task_description;
  task_description.FormatMessage(IDS_SCHEDULED_TASK_DESCRIPTION, company_name);

  CString task_name(mode == COMMANDLINE_MODE_CORE ?
                    internal::GetCurrentTaskNameCore(is_machine) :
                    internal::GetCurrentTaskNameUA(is_machine));
  HRESULT hr = internal::InstallScheduledTask(task_name,
                                              task_path,
                                              task_parameters,
                                              task_description,
                                              is_machine,
                                              mode == COMMANDLINE_MODE_CORE &&
                                              is_machine,
                                              true,
                                              mode == COMMANDLINE_MODE_UA);

  if (SUCCEEDED(hr)) {
    return hr;
  }

  // Create a new task name and fall through to install that.
  if (mode == COMMANDLINE_MODE_CORE) {
    VERIFY1(SUCCEEDED(
    internal::CreateAndSetVersionedTaskNameCoreInRegistry(is_machine)));
    task_name = internal::GetCurrentTaskNameCore(is_machine);
  } else {
    VERIFY1(SUCCEEDED(
    internal::CreateAndSetVersionedTaskNameUAInRegistry(is_machine)));
    task_name = internal::GetCurrentTaskNameUA(is_machine);
  }
  ASSERT1(!internal::IsInstalledScheduledTask(task_name));

  return internal::InstallScheduledTask(task_name,
                                        task_path,
                                        task_parameters,
                                        task_description,
                                        is_machine,
                                        mode == COMMANDLINE_MODE_CORE &&
                                        is_machine,
                                        true,
                                        mode == COMMANDLINE_MODE_UA);
}

HRESULT InstallGoopdateTasks(const TCHAR* task_path, bool is_machine) {
  HRESULT hr = InstallGoopdateTaskForMode(task_path,
                                          is_machine,
                                          COMMANDLINE_MODE_CORE);
  if (FAILED(hr)) {
    return hr;
  }

  return InstallGoopdateTaskForMode(task_path, is_machine, COMMANDLINE_MODE_UA);
}

HRESULT UninstallGoopdateTasks(bool is_machine) {
  VERIFY1(SUCCEEDED(internal::UninstallScheduledTask(
      internal::GetCurrentTaskNameCore(is_machine))));
  VERIFY1(SUCCEEDED(internal::UninstallScheduledTask(
      internal::GetCurrentTaskNameUA(is_machine))));

  // Try to uninstall any tasks that we failed to update during a previous
  // overinstall. It is possible that we fail to uninstall these again here.
  VERIFY1(SUCCEEDED(internal::UninstallScheduledTasks(
      scheduled_task_utils::GetDefaultGoopdateTaskName(is_machine,
                                                 COMMANDLINE_MODE_CORE))));
  VERIFY1(SUCCEEDED(internal::UninstallScheduledTasks(
      scheduled_task_utils::GetDefaultGoopdateTaskName(is_machine,
                                                 COMMANDLINE_MODE_UA))));
  return S_OK;
}

HRESULT UninstallLegacyGoopdateTasks(bool is_machine) {
  const CString& legacy_omaha1_task =
      internal::GetOmaha1LegacyTaskName(is_machine);
  VERIFY1(SUCCEEDED(internal::UninstallScheduledTask(legacy_omaha1_task)));

  const CString& legacy_omaha2_task =
      internal::GetOmaha2LegacyTaskName(is_machine);
  VERIFY1(SUCCEEDED(internal::UninstallScheduledTask(legacy_omaha2_task)));

  return S_OK;
}

HRESULT StartGoopdateTaskCore(bool is_machine) {
  return internal::StartScheduledTask(
             internal::GetCurrentTaskNameCore(is_machine));
}

bool IsInstalledGoopdateTaskUA(bool is_machine) {
  return internal::IsInstalledScheduledTask(
                             internal::GetCurrentTaskNameUA(is_machine));
}

bool IsDisabledGoopdateTaskUA(bool is_machine) {
  const CString& task_name(internal::GetCurrentTaskNameUA(is_machine));
  return internal::GetScheduledTaskStatus(task_name) == SCHED_S_TASK_DISABLED;
}

HRESULT GetExitCodeGoopdateTaskUA(bool is_machine) {
  const CString& task_name(internal::GetCurrentTaskNameUA(is_machine));
  return internal::GetScheduledTaskExitCode(task_name);
}

bool IsUATaskHealthy(bool is_machine) {
  if (!ServiceUtils::IsServiceRunning(SERVICE_SCHEDULE)) {
    UTIL_LOG(LW, (_T("[Task Scheduler Service is not running]")));
    return false;
  }

  if (!IsInstalledGoopdateTaskUA(is_machine)) {
    UTIL_LOG(LW, (_T("[UA Task not installed]")));
    return false;
  }

  if (IsDisabledGoopdateTaskUA(is_machine)) {
    UTIL_LOG(LW, (_T("[UA Task disabled]")));
    return false;
  }

  return true;
}

}  // namespace scheduled_task_utils

}  // namespace omaha
