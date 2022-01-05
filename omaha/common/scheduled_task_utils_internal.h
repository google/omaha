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
#include <mstask.h>
#include <taskschd.h>

namespace omaha {

namespace scheduled_task_utils {

namespace internal {

class ScheduledTasksInterface {
 public:
  virtual ~ScheduledTasksInterface() {}

  // Installs a scheduled task. The task will run as either as SYSTEM or the
  // current user. The task will always have a daily trigger. Optional triggers
  // include logon and hourly triggers.
  virtual HRESULT InstallScheduledTask(const CString& task_name,
                                       const CString& task_path,
                                       const CString& task_parameters,
                                       const CString& task_comment,
                                       bool is_machine,
                                       bool create_logon_trigger,
                                       bool create_hourly_trigger) = 0;
  // Deletes a scheduled task.
  virtual HRESULT UninstallScheduledTask(const CString& task_name) = 0;

  // Deletes all scheduled tasks with the given prefix.
  virtual HRESULT UninstallScheduledTasks(const CString& task_prefix) = 0;

  // Runs a scheduled task immediately.
  virtual HRESULT StartScheduledTask(const CString& task_name) = 0;

  // Stops the task if it is already running.
  virtual HRESULT StopScheduledTask(const CString& task_name) = 0;

  // Returns true if the scheduled task exists.
  virtual bool IsInstalledScheduledTask(const CString& task_name) = 0;

  // Returns true if the scheduled task is disabled.
  virtual bool IsDisabledScheduledTask(const CString& task_name) = 0;

  // Returns true if the scheduled task ever ran.
  virtual bool HasScheduledTaskEverRun(const CString& task_name) = 0;

  // Returns the last task result or status code. List of codes are at
  // http://msdn.microsoft.com/en-us/library/aa383604.
  // v2 API: GetScheduledTaskStatus() and GetScheduledTaskExitCode() are
  // identical.
  // v1 API: the results correspond to ITask::GetStatus().
  virtual HRESULT GetScheduledTaskStatus(const CString& task_name) = 0;

  // Returns the last task result or status code. List of codes are at
  // http://msdn.microsoft.com/en-us/library/aa383604.
  // v2 API: GetScheduledTaskStatus() and GetScheduledTaskExitCode() are
  // identical.
  // v1 API: the results correspond to ITask::GetExitCode().
  virtual HRESULT GetScheduledTaskExitCode(const CString& task_name) = 0;
};

class V1ScheduledTasks : public ScheduledTasksInterface {
 public:
  V1ScheduledTasks();
  virtual ~V1ScheduledTasks();

  virtual HRESULT InstallScheduledTask(const CString& task_name,
                                       const CString& task_path,
                                       const CString& task_parameters,
                                       const CString& task_comment,
                                       bool is_machine,
                                       bool create_logon_trigger,
                                       bool create_hourly_trigger);
  virtual HRESULT UninstallScheduledTask(const CString& task_name);
  virtual HRESULT UninstallScheduledTasks(const CString& task_prefix);
  virtual HRESULT StartScheduledTask(const CString& task_name);
  virtual HRESULT StopScheduledTask(const CString& task_name);
  virtual bool IsInstalledScheduledTask(const CString& task_name);
  virtual bool IsDisabledScheduledTask(const CString& task_name);
  virtual bool HasScheduledTaskEverRun(const CString& task_name);
  virtual HRESULT GetScheduledTaskStatus(const CString& task_name);
  virtual HRESULT GetScheduledTaskExitCode(const CString& task_name);

 private:
  static HRESULT CreateLogonTrigger(ITask* task);
  static HRESULT CreatePeriodicTrigger(ITask* task, bool create_hourly_trigger);
  static HRESULT CreateScheduledTask(ITask* task,
                                     const CString& task_path,
                                     const CString& task_parameters,
                                     const CString& task_comment,
                                     bool is_machine,
                                     bool create_logon_trigger,
                                     bool create_hourly_trigger);

  DISALLOW_COPY_AND_ASSIGN(V1ScheduledTasks);
};

class V2ScheduledTasks : public ScheduledTasksInterface {
 public:
  V2ScheduledTasks();
  virtual ~V2ScheduledTasks();

  virtual HRESULT InstallScheduledTask(const CString& task_name,
                                       const CString& task_path,
                                       const CString& task_parameters,
                                       const CString& task_comment,
                                       bool is_machine,
                                       bool create_logon_trigger,
                                       bool create_hourly_trigger);
  virtual HRESULT UninstallScheduledTask(const CString& task_name);
  virtual HRESULT UninstallScheduledTasks(const CString& task_prefix);
  virtual HRESULT StartScheduledTask(const CString& task_name);
  virtual HRESULT StopScheduledTask(const CString& task_name);
  virtual bool IsInstalledScheduledTask(const CString& task_name);
  virtual bool IsDisabledScheduledTask(const CString& task_name);
  virtual bool HasScheduledTaskEverRun(const CString& task_name);
  virtual HRESULT GetScheduledTaskStatus(const CString& task_name);
  virtual HRESULT GetScheduledTaskExitCode(const CString& task_name);

 private:
  static HRESULT GetTaskFolder(ITaskFolder** task_folder);
  static HRESULT GetRegisteredTask(const CString& task_name,
                                   IRegisteredTask** task);
  static bool IsScheduledTaskRunning(const CString&  task_name);
  static HRESULT CreateScheduledTaskXml(const CString& task_path,
                                        const CString& task_parameters,
                                        const CString& task_description,
                                        const CString& start_time,
                                        bool is_machine,
                                        bool create_logon_trigger,
                                        bool create_hourly_trigger,
                                        CString* scheduled_task_xml);

  friend class ScheduledTaskUtilsV2Test;

  DISALLOW_COPY_AND_ASSIGN(V2ScheduledTasks);
};

// Returns the single instance of V2ScheduledTasks if the 2.0 API is available,
// otherwise returns the single instance of V1ScheduledTasks.
ScheduledTasksInterface& Instance();

// Gets the current name, say "GoogleUpdateTaskMachineCore", of the
// GoogleUpdateCore scheduled task, either from the registry, or a default
// value if there is no registration.
CString GetCurrentTaskNameCore(bool is_machine);

// Gets the current name, say "GoogleUpdateTaskMachineUA", of the
// GoogleUpdateUA scheduled task, either from the registry, or a default value
// if there is no registration.
CString GetCurrentTaskNameUA(bool is_machine);

// Creates a unique name, say "GoogleUpdateTaskMachineCore1c9b3d6baf90df3", or
// "GoogleUpdateTaskMachineUA1c9b3d6baf90df3", of the GoogleUpdateCore/UA
// scheduled task.
CString CreateRandomTaskName(bool is_machine, CommandLineMode mode);

// Stores the name of the GoogleUpdateCore/UA scheduled task in the registry.
// Subsequent invocations of GetCurrentTaskNameCore/UA() will return this value.
HRESULT SetTaskNameInRegistry(bool is_machine,
                              CommandLineMode mode,
                              const CString& name);

// Waits for the task to change its status to the specified status value and
// returns the status of the task. If the status did not change within the
// time, it returns the status of the task at the end of the wait.
HRESULT WaitForTaskStatus(const CString& task_name,
                                  HRESULT status,
                                  int time_ms);

// Returns true if the 2.0 API is available.
bool IsTaskScheduler2APIAvailable();

}  // namespace internal

}  // namespace scheduled_task_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_SCHEDULED_TASK_UTILS_INTERNAL_H_

