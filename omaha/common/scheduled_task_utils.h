// Copyright 2007-2009 Google Inc.
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

#ifndef OMAHA_COMMON_SCHEDULED_TASK_UTILS_H_
#define OMAHA_COMMON_SCHEDULED_TASK_UTILS_H_

#include <windows.h>
#include <atlstr.h>
// TODO(omaha3): Try to avoid depending on goopdate command line in these files.
// If Omaha 3 ends up needing multiple tasks, consider using a bool or
// task-specific enum instead.
#include "omaha/common/command_line.h"

namespace omaha {

namespace scheduled_task_utils {

// Deletes the single instance of the internal Scheduled Tasks interface. The
// instance is created internally by methods in scheduled_task_utils.
void DeleteScheduledTasksInstance();

// This method will return the default scheduled task name. This default value
// is also used as the prefix for generating unique task names.
CString GetDefaultGoopdateTaskName(bool is_machine, CommandLineMode mode);

HRESULT InstallGoopdateTasks(const CString& task_path, bool is_machine);
HRESULT UninstallGoopdateTasks(bool is_machine);
HRESULT UninstallLegacyGoopdateTasks(bool is_machine);
HRESULT StartGoopdateTaskCore(bool is_machine);
bool IsInstalledGoopdateTaskUA(bool is_machine);
bool IsDisabledGoopdateTaskUA(bool is_machine);
bool HasGoopdateTaskEverRunUA(bool is_machine);

HRESULT GetExitCodeGoopdateTaskUA(bool is_machine);
bool IsUATaskHealthy(bool is_machine);

}  // namespace scheduled_task_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_SCHEDULED_TASK_UTILS_H_
