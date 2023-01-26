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

#include <atlsecurity.h>
#include <atltime.h>
#include <lmcons.h>
#include <lmsname.h>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
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

namespace {

CString GenerateRandName(const TCHAR* name_prefix) {
  CString guid;
  if (FAILED(GetGuid(&guid))) {
    return CString();
  }

  CString rand_name;
  SafeCStringFormat(&rand_name, _T("%s%s"), name_prefix, guid);
  return rand_name;
}

}  // namespace

V1ScheduledTasks::V1ScheduledTasks() {
  CORE_LOG(L1, (_T("[V1ScheduledTasks::V1ScheduledTasks]")));
}

V1ScheduledTasks::~V1ScheduledTasks() {
  CORE_LOG(L1, (_T("[V1ScheduledTasks::~V1ScheduledTasks]")));
}

bool V1ScheduledTasks::IsInstalledScheduledTask(const CString& task_name) {
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

bool V1ScheduledTasks::IsDisabledScheduledTask(const CString& task_name) {
  return GetScheduledTaskStatus(task_name) == SCHED_S_TASK_DISABLED;
}

bool V1ScheduledTasks::HasScheduledTaskEverRun(const CString& task_name) {
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
  return hr != SCHED_S_TASK_HAS_NOT_RUN;
}

HRESULT V1ScheduledTasks::GetScheduledTaskStatus(const CString& task_name) {
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

HRESULT V1ScheduledTasks::GetScheduledTaskExitCode(const CString& task_name) {
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

HRESULT V1ScheduledTasks::StartScheduledTask(const CString& task_name) {
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

HRESULT V1ScheduledTasks::StopScheduledTask(const CString& task_name) {
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

HRESULT V1ScheduledTasks::CreateLogonTrigger(ITask* task) {
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

HRESULT V1ScheduledTasks::CreatePeriodicTrigger(ITask* task,
                                                bool create_hourly_trigger) {
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

HRESULT V1ScheduledTasks::CreateScheduledTask(ITask* task,
                                              const CString& task_path,
                                              const CString& task_parameters,
                                              const CString& task_comment,
                                              bool is_machine,
                                              bool create_logon_trigger,
                                              bool create_hourly_trigger) {
  ASSERT1(task);
  ASSERT1(!create_logon_trigger || (create_logon_trigger && is_machine));

  UTIL_LOG(L3, (_T("[CreateScheduledTask][%s][%s][%d]"),
                task_path, task_parameters, is_machine));

  CString quoted_task_path(task_path);
  EnclosePath(&quoted_task_path);

  HRESULT hr = task->SetApplicationName(quoted_task_path);
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

  hr = CreatePeriodicTrigger(task, create_hourly_trigger);
  if (FAILED(hr)) {
    return hr;
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

HRESULT V1ScheduledTasks::InstallScheduledTask(const CString& task_name,
                                               const CString& task_path,
                                               const CString& task_parameters,
                                               const CString& task_comment,
                                               bool is_machine,
                                               bool create_logon_trigger,
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
                             create_hourly_trigger);
}

HRESULT V1ScheduledTasks::UninstallScheduledTask(const CString& task_name) {
  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ITaskScheduler.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }

  // Stop the task before deleting it. Ignore return value.
  VERIFY_SUCCEEDED(StopScheduledTask(task_name));

  // delete the task.
  hr = scheduler->Delete(task_name);
  if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
    UTIL_LOG(LE, (_T("[GetScheduledTaskStatus][Delete failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT V1ScheduledTasks::UninstallScheduledTasks(const CString& task_prefix) {
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

V2ScheduledTasks::V2ScheduledTasks() {
  CORE_LOG(L1, (_T("[V2ScheduledTasks::V2ScheduledTasks]")));
}

V2ScheduledTasks::~V2ScheduledTasks() {
  CORE_LOG(L1, (_T("[V2ScheduledTasks::~V2ScheduledTasks]")));
}

HRESULT V2ScheduledTasks::GetTaskFolder(ITaskFolder** task_folder) {
  ASSERT1(task_folder);

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

  CComPtr<ITaskFolder> folder;
  hr = task_service->GetFolder(CComBSTR(_T("\\")) , &folder);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Cannot get Root Folder pointer][0x%x]"), hr));
    return hr;
  }

  *task_folder = folder.Detach();
  return S_OK;
}

HRESULT V2ScheduledTasks::GetRegisteredTask(const CString& task_name,
                                            IRegisteredTask** task) {
  ASSERT1(task);

  CComPtr<ITaskFolder> task_folder;
  HRESULT hr = GetTaskFolder(&task_folder);
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

bool V2ScheduledTasks::IsScheduledTaskRunning(const CString& task_name) {
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

  long count = 0;  // NOLINT
  hr = running_task_collection->get_Count(&count);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRunningTaskCollection.get_Count failed][0x%x]"), hr));
    return false;
  }

  return count > 0;
}

HRESULT V2ScheduledTasks::StartScheduledTask(const CString& task_name) {
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

HRESULT V2ScheduledTasks::StopScheduledTask(const CString& task_name) {
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

  long count = 0;  // NOLINT
  hr = running_task_collection->get_Count(&count);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IRunningTaskCollection.get_Count failed][0x%x]"), hr));
    return hr;
  }

  if (count <= 0) {
    return S_OK;
  }

  for (long i = 0; i < count; ++i) {  // NOLINT
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

HRESULT V2ScheduledTasks::CreateScheduledTaskXml(
                              const CString& task_path,
                              const CString& task_parameters,
                              const CString& task_description,
                              const CString& start_time,
                              bool is_machine,
                              bool create_logon_trigger,
                              bool create_hourly_trigger,
                              CString* scheduled_task_xml) {
  ASSERT1(!create_logon_trigger || (create_logon_trigger && is_machine));
  ASSERT1(scheduled_task_xml);

  UTIL_LOG(L3, (_T("[CreateScheduledTaskXml][%s][%s][%d]"),
                task_path, task_parameters, is_machine));

  CString logon_trigger;
  if (create_logon_trigger) {
    logon_trigger =
        _T("  <LogonTrigger>\n")
        _T("    <Enabled>true</Enabled>\n")
        _T("  </LogonTrigger>\n");
  }

  CString hourly_trigger;
  if (create_hourly_trigger) {
    hourly_trigger =
        _T("    <Repetition>\n")
        _T("      <Interval>PT1H</Interval>\n")
        _T("      <Duration>P1D</Duration>\n")
        _T("    </Repetition>\n");
  }
  CString user_id;
  CString principal_attributes;
  if (is_machine) {
    user_id = Sids::System().Sid();
    principal_attributes = _T("<RunLevel>HighestAvailable</RunLevel>\n");
  } else {
    CAccessToken access_token;
    CSid current_user_sid;
    VERIFY1(access_token.GetProcessToken(TOKEN_READ | TOKEN_QUERY));
    VERIFY1(access_token.GetUser(&current_user_sid));
    user_id = current_user_sid.Sid();
    principal_attributes = _T("<LogonType>InteractiveToken</LogonType>\n");
  }

  CString quoted_task_path(task_path);
  EnclosePath(&quoted_task_path);

  CString task_xml;
  SafeCStringFormat(
      &task_xml,
      _T("<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n")
      _T("<Task version=\"1.2\"\n")
      _T("  xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n")
      _T("  <RegistrationInfo>\n")
      _T("    <Version>%s</Version>\n")
      _T("    <Description>%s</Description>\n")
      _T("  </RegistrationInfo>\n")
      _T("  <Triggers>\n")
      _T("    %s\n")
      _T("    <CalendarTrigger>\n")
      _T("      <StartBoundary>%s</StartBoundary>\n")
      _T("      %s\n")
      _T("      <ScheduleByDay>\n")
      _T("        <DaysInterval>1</DaysInterval>\n")
      _T("      </ScheduleByDay>\n")
      _T("    </CalendarTrigger>\n")
      _T("  </Triggers>\n")
      _T("  <Principals>\n")
      _T("    <Principal>\n")
      _T("      <UserId>%s</UserId>\n")
      _T("      %s\n")
      _T("    </Principal>\n")
      _T("  </Principals>\n")
      _T("  <Settings>\n")
      _T("    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n")
      _T("    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n")
      _T("    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n")
      _T("    <StartWhenAvailable>true</StartWhenAvailable>\n")
      _T("    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\n")
      _T("    <IdleSettings>\n")
      _T("      <StopOnIdleEnd>false</StopOnIdleEnd>\n")
      _T("    </IdleSettings>\n")
      _T("    <Enabled>true</Enabled>\n")
      _T("    <RunOnlyIfIdle>false</RunOnlyIfIdle>\n")
      _T("    <WakeToRun>false</WakeToRun>\n")
      _T("    <ExecutionTimeLimit>PT72H</ExecutionTimeLimit>\n")
      _T("  </Settings>\n")
      _T("  <Actions>\n")
      _T("    <Exec>\n")
      _T("      <Command>%s</Command>\n")
      _T("      <Arguments>%s</Arguments>\n")
      _T("    </Exec>\n")
      _T("  </Actions>\n")
      _T("</Task>\n"),
      omaha::GetVersionString(),
      task_description,
      logon_trigger,
      start_time,
      hourly_trigger,
      user_id,
      principal_attributes,
      quoted_task_path,
      task_parameters);

  *scheduled_task_xml = task_xml;
  UTIL_LOG(L6, (_T("[CreateScheduledTaskXml][%s]"), task_xml));
  return S_OK;
}

HRESULT V2ScheduledTasks::InstallScheduledTask(const CString& task_name,
                                               const CString& task_path,
                                               const CString& task_parameters,
                                               const CString& task_description,
                                               bool is_machine,
                                               bool create_logon_trigger,
                                               bool create_hourly_trigger) {
  CComPtr<ITaskFolder> task_folder;
  HRESULT hr = GetTaskFolder(&task_folder);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Cannot get Root Folder pointer][0x%x]"), hr));
    return hr;
  }

  const CTime plus_5min(CTime::GetCurrentTime() + CTimeSpan(0, 0, 5, 0));
  const CString start_time(plus_5min.Format(_T("%Y-%m-%dT%H:%M:%S")));

  CString task_xml;
  VERIFY_SUCCEEDED(CreateScheduledTaskXml(
                             task_path,
                             task_parameters,
                             task_description,
                             start_time,
                             is_machine,
                             create_logon_trigger,
                             create_hourly_trigger,
                             &task_xml));

  CComPtr<IRegisteredTask> registered_task;
  return task_folder->RegisterTask(
      CComBSTR(task_name),
      CComBSTR(task_xml),
      TASK_CREATE_OR_UPDATE,
      CComVariant(),
      CComVariant(),
      is_machine ? TASK_LOGON_SERVICE_ACCOUNT : TASK_LOGON_INTERACTIVE_TOKEN,
      CComVariant(),
      &registered_task);
}

HRESULT V2ScheduledTasks::UninstallScheduledTask(const CString& task_name) {
  CComPtr<ITaskFolder> task_folder;
  HRESULT hr = GetTaskFolder(&task_folder);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Cannot get Root Folder pointer][0x%x]"), hr));
    return hr;
  }

  hr = task_folder->DeleteTask(CComBSTR(task_name), 0);
  if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
    UTIL_LOG(LE, (_T("[UninstallScheduledTask][Delete failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT V2ScheduledTasks::UninstallScheduledTasks(const CString& task_prefix) {
  CComPtr<ITaskFolder> task_folder;
  HRESULT hr = GetTaskFolder(&task_folder);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Cannot get Root Folder pointer][0x%x]"), hr));
    return hr;
  }

  CComPtr<IRegisteredTaskCollection> registered_task_collection;
  hr = task_folder->GetTasks(0, &registered_task_collection);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetTasks failed][0x%x]"), hr));
    return hr;
  }

  long num_tasks = 0;  // NOLINT
  hr = registered_task_collection->get_Count(&num_tasks);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[get_Count failed][0x%x]"), hr));
    return hr;
  }

  // Collections are 1-based.
  for (long i = 1; i <= num_tasks; ++i) {  // NOLINT
    CComPtr<IRegisteredTask> registered_task;
    hr = registered_task_collection->get_Item(CComVariant(i), &registered_task);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[Failed to get Item][%d][0x%x]"), i, hr));
      continue;
    }

    CComBSTR task_name;
    hr = registered_task->get_Name(&task_name);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[Failed to get Name][%d][0x%x]"), i, hr));
      continue;
    }

    if (String_StartsWith(task_name, task_prefix, true)) {
      UninstallScheduledTask(CString(task_name));
    }
  }

  return S_OK;
}

bool V2ScheduledTasks::IsInstalledScheduledTask(const CString& task_name) {
  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);

  UTIL_LOG(L3, (_T("[IsInstalledScheduledTask returned][0x%x]"), hr));
  return hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

bool V2ScheduledTasks::IsDisabledScheduledTask(const CString& task_name) {
  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetRegisteredTask failed][0x%x]"), hr));
    return false;
  }

  VARIANT_BOOL is_enabled(VARIANT_TRUE);
  hr = registered_task->get_Enabled(&is_enabled);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[get_Enabled failed][0x%x]"), hr));
    return false;
  }

  UTIL_LOG(L3, (_T("[IsDisabledScheduledTask returned][%d]"), !is_enabled));
  return !is_enabled;
}

bool V2ScheduledTasks::HasScheduledTaskEverRun(const CString& task_name) {
  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[HasScheduledTaskEverRun][get failed][0x%x]"), hr));
    return false;
  }

  DATE recent_run_time = 0;
  hr = registered_task->get_LastRunTime(&recent_run_time);

  // hr == SCHED_S_TASK_HAS_NOT_RUN if the task has never run.
  return hr != SCHED_S_TASK_HAS_NOT_RUN;
}

HRESULT V2ScheduledTasks::GetScheduledTaskStatus(const CString& task_name) {
  return GetScheduledTaskExitCode(task_name);
}

HRESULT V2ScheduledTasks::GetScheduledTaskExitCode(const CString& task_name) {
  CComPtr<IRegisteredTask> registered_task;
  HRESULT hr = GetRegisteredTask(task_name, &registered_task);

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetScheduledTaskExitCode][get failed][0x%x]"), hr));
    return hr;
  }

  HRESULT last_task_result(E_FAIL);
  hr = registered_task->get_LastTaskResult(&last_task_result);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[get_LastTaskResult failed][0x%x]"), hr));
    return hr;
  }

  UTIL_LOG(L6, (_T("[GetScheduledTaskExitCode][0x%x]"), last_task_result));
  return last_task_result;
}

ScheduledTasksInterface* const kInvalidInstance =
    reinterpret_cast<ScheduledTasksInterface* const>(-1);
ScheduledTasksInterface* instance_ = NULL;
LLock lock_;

ScheduledTasksInterface& Instance() {
  __mutexScope(lock_);
  ASSERT1(instance_ != kInvalidInstance);

  if (!instance_) {
    if (IsTaskScheduler2APIAvailable()) {
      instance_ = new V2ScheduledTasks();
    } else {
      instance_ = new V1ScheduledTasks();
    }
  }

  return *instance_;
}

CString GetCurrentTaskNameCore(bool is_machine) {
  UTIL_LOG(L3, (_T("[GetCurrentTaskNameCore][%d]"), is_machine));

  CString default_name(GetDefaultGoopdateTaskName(is_machine,
                                                  COMMANDLINE_MODE_CORE));
  return goopdate_utils::GetCurrentVersionedName(is_machine,
                                                 kRegValueTaskNameC,
                                                 default_name);
}

CString GetCurrentTaskNameUA(bool is_machine) {
  UTIL_LOG(L3, (_T("[GetCurrentTaskNameUA][%d]"), is_machine));

  CString default_name(GetDefaultGoopdateTaskName(is_machine,
                                                  COMMANDLINE_MODE_UA));
  return goopdate_utils::GetCurrentVersionedName(is_machine,
                                                 kRegValueTaskNameUA,
                                                 default_name);
}

CString CreateRandomTaskName(bool is_machine, CommandLineMode mode) {
  UTIL_LOG(L3, (_T("[CreateRandomTaskName][%d][%d]"), is_machine, mode));

  CString prefix(GetDefaultGoopdateTaskName(is_machine, mode));
  CString name(GenerateRandName(prefix));
  UTIL_LOG(L3, (_T("[Random name][%s]"), name));

  return name;
}

HRESULT SetTaskNameInRegistry(bool is_machine,
                              CommandLineMode mode,
                              const CString& name) {
  UTIL_LOG(L3, (_T("[SetTaskNameInRegistry][%d][%d][%s]"),
                is_machine, mode, name));

  const TCHAR* key_name = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  const TCHAR* key_value = mode == COMMANDLINE_MODE_CORE ?
                               kRegValueTaskNameC :
                               kRegValueTaskNameUA;
  return RegKey::SetValue(key_name, key_value, name);
}

// Returns the task name Omaha used to install in Omaha 1.2.x.
CString GetOmaha1LegacyTaskName(bool is_machine) {
  const CString kLegacyOmaha1TaskNameMachine = MAIN_EXE_BASE_NAME _T("Task");
  const CString kLegacyOmaha1TaskNameUser = MAIN_EXE_BASE_NAME _T("TaskUser");
  return is_machine ? kLegacyOmaha1TaskNameMachine : kLegacyOmaha1TaskNameUser;
}

// Returns the task name Omaha used to install in Omaha 2 before the
// "GoogleUpdate.exe does not run all the time" refactoring.
CString GetOmaha2LegacyTaskName(bool is_machine) {
  const CString& kLegacyOmaha2TaskNameUserPrefix = MAIN_EXE_BASE_NAME _T("TaskUser");
  const CString& kLegacyOmaha2TaskNameMachine = MAIN_EXE_BASE_NAME _T("TaskMachine");
  if (is_machine) {
    return kLegacyOmaha2TaskNameMachine;
  }

  CString task_name_user = kLegacyOmaha2TaskNameUserPrefix;
  CString user_sid;
  VERIFY_SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid));
  task_name_user += user_sid;
  return task_name_user;
}

HRESULT WaitForTaskStatus(const CString& task_name,
                          HRESULT status,
                          int time_ms) {
  int kSleepBetweenRetriesMs = 100;
  Timer timer(true);
  while (timer.GetMilliseconds() < time_ms) {
    if (Instance().GetScheduledTaskStatus(task_name) == status) {
      return status;
    }
    ::Sleep(kSleepBetweenRetriesMs);
  }
  return Instance().GetScheduledTaskStatus(task_name);
}

bool IsTaskScheduler2APIAvailable() {
  CComPtr<ITaskService> task_service;
  return SUCCEEDED(task_service.CoCreateInstance(CLSID_TaskScheduler,
                                                 NULL,
                                                 CLSCTX_INPROC_SERVER));
}

}  // namespace internal

void DeleteScheduledTasksInstance() {
  __mutexScope(internal::lock_);

  delete internal::instance_;
  internal::instance_ = internal::kInvalidInstance;
}

CString GetDefaultGoopdateTaskName(bool is_machine, CommandLineMode mode) {
  ASSERT1(mode == COMMANDLINE_MODE_CORE || mode == COMMANDLINE_MODE_UA);

  CString task_name;
  if (is_machine) {
    task_name = kScheduledTaskNameMachinePrefix;
  } else {
    task_name = kScheduledTaskNameUserPrefix;
    CString user_sid;
    VERIFY_SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid));
    task_name += user_sid;
  }

  task_name += (mode == COMMANDLINE_MODE_CORE) ? kScheduledTaskNameCoreSuffix :
                                                 kScheduledTaskNameUASuffix;
  return task_name;
}

HRESULT InstallGoopdateTaskForMode(const CString& task_path,
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

  if (internal::Instance().IsInstalledScheduledTask(task_name)) {
    // Update the currently installed scheduled task.
    HRESULT hr = internal::Instance().InstallScheduledTask(
                                        task_name,
                                        task_path,
                                        task_parameters,
                                        task_description,
                                        is_machine,
                                        mode == COMMANDLINE_MODE_CORE &&
                                        is_machine,
                                        mode == COMMANDLINE_MODE_UA);
    if (SUCCEEDED(hr)) {
      return hr;
    }
  }

  // Create a new task name and fall through to install that.
  task_name = internal::CreateRandomTaskName(is_machine, mode);
  if (task_name.IsEmpty()) {
    return E_UNEXPECTED;
  }

  ASSERT1(!internal::Instance().IsInstalledScheduledTask(task_name));

  HRESULT hr = internal::Instance().InstallScheduledTask(
                   task_name,
                   task_path,
                   task_parameters,
                   task_description,
                   is_machine,
                   mode == COMMANDLINE_MODE_CORE &&
                   is_machine,
                   mode == COMMANDLINE_MODE_UA);
  if (SUCCEEDED(hr)) {
    VERIFY_SUCCEEDED(internal::SetTaskNameInRegistry(is_machine,
                                                     mode,
                                                     task_name));
  }

  return hr;
}

HRESULT InstallGoopdateTasks(const CString& task_path, bool is_machine) {
  HRESULT hr = InstallGoopdateTaskForMode(task_path,
                                          is_machine,
                                          COMMANDLINE_MODE_CORE);
  if (FAILED(hr)) {
    return hr;
  }

  return InstallGoopdateTaskForMode(task_path, is_machine, COMMANDLINE_MODE_UA);
}

HRESULT UninstallGoopdateTasks(bool is_machine) {
  VERIFY_SUCCEEDED(internal::Instance().UninstallScheduledTask(
      internal::GetCurrentTaskNameCore(is_machine)));
  VERIFY_SUCCEEDED(internal::Instance().UninstallScheduledTask(
      internal::GetCurrentTaskNameUA(is_machine)));

  // Try to uninstall any tasks that we failed to update during a previous
  // overinstall. It is possible that we fail to uninstall these again here.
  VERIFY_SUCCEEDED(internal::Instance().UninstallScheduledTasks(
      scheduled_task_utils::GetDefaultGoopdateTaskName(
          is_machine, COMMANDLINE_MODE_CORE)));
  VERIFY_SUCCEEDED(internal::Instance().UninstallScheduledTasks(
      scheduled_task_utils::GetDefaultGoopdateTaskName(
          is_machine, COMMANDLINE_MODE_UA)));
  return S_OK;
}

HRESULT UninstallLegacyGoopdateTasks(bool is_machine) {
  const CString& legacy_omaha1_task =
      internal::GetOmaha1LegacyTaskName(is_machine);
  VERIFY_SUCCEEDED(
      internal::Instance().UninstallScheduledTask(legacy_omaha1_task));

  const CString& legacy_omaha2_task =
      internal::GetOmaha2LegacyTaskName(is_machine);
  VERIFY_SUCCEEDED(
      internal::Instance().UninstallScheduledTask(legacy_omaha2_task));

  return S_OK;
}

HRESULT StartGoopdateTaskCore(bool is_machine) {
  return internal::Instance().StartScheduledTask(
             internal::GetCurrentTaskNameCore(is_machine));
}

bool IsInstalledGoopdateTaskUA(bool is_machine) {
  return internal::Instance().IsInstalledScheduledTask(
                                  internal::GetCurrentTaskNameUA(is_machine));
}

bool IsDisabledGoopdateTaskUA(bool is_machine) {
  const CString& task_name(internal::GetCurrentTaskNameUA(is_machine));
  return internal::Instance().IsDisabledScheduledTask(task_name);
}

bool HasGoopdateTaskEverRunUA(bool is_machine) {
  const CString& task_name(internal::GetCurrentTaskNameUA(is_machine));
  return internal::Instance().HasScheduledTaskEverRun(task_name);
}

HRESULT GetExitCodeGoopdateTaskUA(bool is_machine) {
  const CString& task_name(internal::GetCurrentTaskNameUA(is_machine));
  return internal::Instance().GetScheduledTaskExitCode(task_name);
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
