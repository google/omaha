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

#ifndef OMAHA_COMMON_SCHEDULED_TASK_UTILS_INTERNAL_H_
#define OMAHA_COMMON_SCHEDULED_TASK_UTILS_INTERNAL_H_

#include <windows.h>
#include <taskschd.h>

namespace omaha {

namespace scheduled_task_utils {

namespace internal {

// Gets the current name, say "GoogleUpdateTaskMachineCore", of the
// GoogleUpdateCore scheduled task, either from the registry, or a default
// value if there is no registration.
CString GetCurrentTaskNameCore(bool is_machine);

// Creates a unique name, say "GoogleUpdateTaskMachineCore1c9b3d6baf90df3", of
// the GoogleUpdateCore scheduled task, and stores it in the registry.
// Subsequent invocations of GetCurrentTaskNameCore() will return this new
// value.
HRESULT CreateAndSetVersionedTaskNameCoreInRegistry(bool machine);

// Gets the current name, say "GoogleUpdateTaskMachineUA", of the
// GoogleUpdateUA scheduled task, either from the registry, or a default value
// if there is no registration.
CString GetCurrentTaskNameUA(bool is_machine);

// Creates a unique name, say "GoogleUpdateTaskMachineUA1c9b3d6baf90df3", of
// the GoogleUpdateUA scheduled task, and stores it in the registry.
// Subsequent invocations of GetCurrentTaskNameUA() will return this new
// value.
HRESULT CreateAndSetVersionedTaskNameUAInRegistry(bool machine);

// Installs a scheduled task. The task will run as either as SYSTEM or the
// current user. The task will be triggered at each user logon, and/or
// fixed intervals.
HRESULT InstallScheduledTask(const TCHAR* task_name,
                             const TCHAR* task_path,
                             const TCHAR* task_parameters,
                             const TCHAR* task_comment,
                             bool is_machine,
                             bool create_logon_trigger,
                             bool create_daily_trigger,
                             bool create_hourly_trigger);

// Deletes a scheduled task.
HRESULT UninstallScheduledTask(const TCHAR* task_name);

// Deletes all scheduled tasks with the given prefix.
HRESULT UninstallScheduledTasks(const TCHAR* task_prefix);

// Runs a scheduled task immediately.
HRESULT StartScheduledTask(const TCHAR* task_name);

// Returns true if the scheduled task exists.
bool IsInstalledScheduledTask(const TCHAR* task_name);

// Returns the priority at which the scheduled task process will run. Returns 0
// on failure.
DWORD GetScheduledTaskPriority(const TCHAR* task_name);

// Returns true if the scheduled task ever ran.
bool HasScheduledTaskEverRun(const TCHAR* task_name);

// Returns a status code on success. List of status codes at
// http://msdn2.microsoft.com/en-us/library/aa381263.aspx
HRESULT GetScheduledTaskStatus(const TCHAR* task_name);

// Stops the task if it is already running.
HRESULT StopScheduledTask(const TCHAR* task_name);

// Waits for the task to change its status to the specified status value and
// returns the status of the task. If the status did not change within the
// time, it returns the status of the task at the end of the wait.
HRESULT WaitForTaskStatus(const TCHAR* task_name, HRESULT status, int time_ms);

namespace v2 {

// Task Scheduler 2.0 API helpers.
bool IsTaskScheduler2APIAvailable();
HRESULT GetRegisteredTask(const TCHAR* task_name, IRegisteredTask** task);
bool IsScheduledTaskRunning(const TCHAR* task_name);
HRESULT StartScheduledTask(const TCHAR* task_name);
HRESULT StopScheduledTask(const TCHAR* task_name);

}  // namespace v2

}  // namespace internal

}  // namespace scheduled_task_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_SCHEDULED_TASK_UTILS_INTERNAL_H_

