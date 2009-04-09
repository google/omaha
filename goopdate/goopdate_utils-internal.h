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

#ifndef OMAHA_GOOPDATE_GOOPDATE_UTILS_INTERNAL_H__
#define OMAHA_GOOPDATE_GOOPDATE_UTILS_INTERNAL_H__

namespace omaha {

namespace goopdate_utils {

namespace internal {

// Installs a scheduled task. The task will run as either as SYSTEM or the
// current user. The task will be triggered at each user logon, and/or when
// the task scheduler decides that the system is idle, and at fixed intervals.
HRESULT InstallScheduledTask(const TCHAR* task_name,
                             const TCHAR* task_path,
                             const TCHAR* task_parameters,
                             const TCHAR* task_comment,
                             bool is_machine,
                             bool create_logon_trigger,
                             bool create_periodic_trigger);

// Deletes a scheduled task.
HRESULT UninstallScheduledTask(const TCHAR* task_name);

// Deletes all scheduled tasks with the given prefix.
HRESULT UninstallScheduledTasks(const TCHAR* task_prefix);

// Runs a scheduled task immediately.
HRESULT StartScheduledTask(const TCHAR* task_name);

// Returns true if the scheduled task exists.
bool IsInstalledScheduledTask(const TCHAR* task_name);

// Returns a status code on success. List of status codes at
// http://msdn2.microsoft.com/en-us/library/aa381263.aspx
HRESULT GetScheduledTaskStatus(const TCHAR* task_name);

// Stops the task if it is already running.
HRESULT StopScheduledTask(const TCHAR* task_name);

}  // namespace internal

}  // namespace goopdate_utils

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_UTILS_INTERNAL_H__

