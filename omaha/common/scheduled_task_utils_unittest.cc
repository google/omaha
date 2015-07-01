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

#include <windows.h>
#include <atlstr.h>
#include <mstask.h>
#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/scoped_ptr_cotask.h"
#include "omaha/base/user_info.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/scheduled_task_utils.h"
#include "omaha/common/scheduled_task_utils_internal.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const int kMaxWaitForProcessMs                  = 120000;

const TCHAR kLongRunningProcessesRelativePath[] =
    _T("unittest_support\\does_not_shutdown\\GoogleUpdate.exe");

CString GetLongRunningProcessPath() {
  const CString module_dir(app_util::GetCurrentModuleDirectory());
  return ConcatenatePath(module_dir, kLongRunningProcessesRelativePath);
}

}  // namespace

namespace scheduled_task_utils {

using internal::GetCurrentTaskNameCore;
using internal::GetCurrentTaskNameUA;
using internal::GetScheduledTaskStatus;
using internal::HasScheduledTaskEverRun;
using internal::StartScheduledTask;
using internal::StopScheduledTask;
using internal::WaitForTaskStatus;

namespace v2 = internal::v2;

using vista_util::IsUserAdmin;

namespace internal {

void StopScheduledTaskAndVerifyReadyState(const CString& task_name) {
  // For some reason, StopScheduleTask may not successfully stop the task
  // even it returns S_OK. So try to stop multiple times.
  for (int i = 0; i < 3; ++i) {
    EXPECT_SUCCEEDED(StopScheduledTask(task_name));

    if (SCHED_S_TASK_READY == WaitForTaskStatus(task_name,
                                                SCHED_S_TASK_READY,
                                                kMsPerSec)) {
      break;
    }
  }
  EXPECT_EQ(SCHED_S_TASK_READY, GetScheduledTaskStatus(task_name));
}

TEST(ScheduledTaskUtilsTest, ScheduledTasks) {
  const TCHAR kSchedTestTaskName[]            = _T("TestScheduledTask");
  const TCHAR kScheduledTaskExecutable[]      = _T("netstat.exe");
  const TCHAR kScheduledTaskParameters[]      = _T("20");
  const TCHAR kSchedTestTaskComment[]         = _T("Google Test Task");

  const CString task_path = ConcatenatePath(app_util::GetSystemDir(),
                                            kScheduledTaskExecutable);
  // Install/uninstall.
  EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                        task_path,
                                        _T(""),
                                        kSchedTestTaskComment,
                                        IsUserAdmin(),
                                        IsUserAdmin(),
                                        true,
                                        true));
  EXPECT_SUCCEEDED(UninstallScheduledTask(kSchedTestTaskName));

  // Calling InstallScheduledTask twice should succeed.
  for (int i = 0; i < 2; ++i) {
    EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                          task_path,
                                          _T(""),
                                          kSchedTestTaskComment,
                                          IsUserAdmin(),
                                          IsUserAdmin(),
                                          true,
                                          true));
  }

  // "Upgrade" to a new version, which now has parameters.
  EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                        task_path,
                                        kScheduledTaskParameters,
                                        kSchedTestTaskComment,
                                        IsUserAdmin(),
                                        IsUserAdmin(),
                                        true,
                                        true));

  EXPECT_FALSE(HasScheduledTaskEverRun(kSchedTestTaskName));

  // Start and stop.
  EXPECT_EQ(SCHED_S_TASK_HAS_NOT_RUN,
            GetScheduledTaskStatus(kSchedTestTaskName));
  EXPECT_SUCCEEDED(StartScheduledTask(kSchedTestTaskName));
  EXPECT_EQ(SCHED_S_TASK_RUNNING,
            WaitForTaskStatus(kSchedTestTaskName,
                              SCHED_S_TASK_RUNNING,
                              kMaxWaitForProcessMs));

  EXPECT_TRUE(HasScheduledTaskEverRun(kSchedTestTaskName));

  StopScheduledTaskAndVerifyReadyState(kSchedTestTaskName);

  // Finally, uninstall.
  EXPECT_SUCCEEDED(UninstallScheduledTask(kSchedTestTaskName));
}

/*
// Disabling this test because we are seeing random failures with 0x80070005 on
// the UninstallScheduledTask call. The production code is not affected by this
// failure as far as I can tell.
TEST(ScheduledTaskUtilsTest, ScheduledTasksV2) {
  if (!v2::IsTaskScheduler2APIAvailable()) {
    std::wcout << _T("\tTest did not run because this OS does not support the ")
                  _T("Task Scheduler 2.0 API.") << std::endl;
    return;
  }

  const TCHAR kSchedTestTaskName[]            = _T("TestScheduledTaskV2");
  const TCHAR kScheduledTaskExecutable[]      = _T("netstat.exe");
  const TCHAR kScheduledTaskParameters[]      = _T("20");
  const TCHAR kSchedTestTaskComment[]         = _T("Google Test Task V2");

  const CString task_path = ConcatenatePath(app_util::GetSystemDir(),
                                            kScheduledTaskExecutable);
  EXPECT_SUCCEEDED(InstallScheduledTask(kSchedTestTaskName,
                                        task_path,
                                        _T(""),
                                        kSchedTestTaskComment,
                                        IsUserAdmin(),
                                        IsUserAdmin(),
                                        true,
                                        true));

  // Start and stop.
  EXPECT_FALSE(v2::IsScheduledTaskRunning(kSchedTestTaskName));
  EXPECT_SUCCEEDED(v2::StartScheduledTask(kSchedTestTaskName));
  EXPECT_TRUE(v2::IsScheduledTaskRunning(kSchedTestTaskName));

  EXPECT_SUCCEEDED(v2::StopScheduledTask(kSchedTestTaskName));
  EXPECT_FALSE(v2::IsScheduledTaskRunning(kSchedTestTaskName));

  // Finally, uninstall.
  EXPECT_SUCCEEDED(UninstallScheduledTask(kSchedTestTaskName));
}
*/

}  // namespace internal


TEST(ScheduledTaskUtilsTest, GoopdateTasks) {
  const CString task_name = GetCurrentTaskNameCore(IsUserAdmin());
  const CString task_path = GetLongRunningProcessPath();

  // Install/uninstall.
  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(IsUserAdmin()));

  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  EXPECT_FALSE(HasScheduledTaskEverRun(task_name));

  // Start and stop.
  EXPECT_EQ(SCHED_S_TASK_HAS_NOT_RUN, GetScheduledTaskStatus(task_name));
  EXPECT_SUCCEEDED(StartGoopdateTaskCore(IsUserAdmin()));

  EXPECT_EQ(SCHED_S_TASK_RUNNING,
            WaitForTaskStatus(task_name,
                              SCHED_S_TASK_RUNNING,
                              kMaxWaitForProcessMs));

  EXPECT_TRUE(HasScheduledTaskEverRun(task_name));

  internal::StopScheduledTaskAndVerifyReadyState(task_name);

  // Finally, uninstall.
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(IsUserAdmin()));
}

TEST(ScheduledTaskUtilsTest, GoopdateTaskInUseOverinstall) {
  const CString task_path = GetLongRunningProcessPath();
  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));

  CString original_task_name(GetCurrentTaskNameCore(IsUserAdmin()));

  // Open the file underlying the current task in exclusive mode, so that
  // InstallGoopdateTasks() is forced to create a new task.
  CComPtr<ITaskScheduler> scheduler;
  EXPECT_SUCCEEDED(scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                              NULL,
                                              CLSCTX_INPROC_SERVER));
  CComPtr<ITask> task;
  EXPECT_SUCCEEDED(scheduler->Activate(original_task_name,
                                       __uuidof(ITask),
                                       reinterpret_cast<IUnknown**>(&task)));
  CComQIPtr<IPersistFile> persist(task);
  EXPECT_TRUE(persist);
  scoped_ptr_cotask<OLECHAR> job_file;
  EXPECT_SUCCEEDED(persist->GetCurFile(address(job_file)));
  persist.Release();

  File file;
  EXPECT_SUCCEEDED(file.OpenShareMode(job_file.get(), false, false, 0));

  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  CString new_task_name(GetCurrentTaskNameCore(IsUserAdmin()));
  EXPECT_STRNE(original_task_name, new_task_name);

  // Cleanup.
  file.Close();
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(IsUserAdmin()));
}

TEST(ScheduledTaskUtilsTest, GetExitCodeGoopdateTaskUA) {
  const CString task_name = GetCurrentTaskNameUA(IsUserAdmin());
  const CString task_path = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("unittest_support\\SaveArguments.exe"));

  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  EXPECT_EQ(SCHED_S_TASK_HAS_NOT_RUN,
            GetExitCodeGoopdateTaskUA(IsUserAdmin()));
  EXPECT_FALSE(HasScheduledTaskEverRun(task_name));

  // Start the task and wait for it to run and become ready again. The task
  // runs a program that returns right away. Sometimes the task does not run
  // for unknown reason. Attempting to run the task multiple times does not
  // work. This remains a flaky test.
  EXPECT_SUCCEEDED(StartScheduledTask(task_name));
  EXPECT_EQ(SCHED_S_TASK_READY,
            WaitForTaskStatus(task_name,
                              SCHED_S_TASK_READY,
                              kMaxWaitForProcessMs));
  EXPECT_TRUE(HasScheduledTaskEverRun(task_name));
  EXPECT_EQ(S_OK, GetExitCodeGoopdateTaskUA(IsUserAdmin()));

  EXPECT_SUCCEEDED(File::Remove(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("unittest_support\\saved_arguments.txt"))));
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(IsUserAdmin()));
}

TEST(ScheduledTaskUtilsTest, GetDefaultGoopdateTaskName_Core_Machine) {
  CString expected_task_name(kScheduledTaskNameMachinePrefix);
  expected_task_name += kScheduledTaskNameCoreSuffix;
  EXPECT_STREQ(expected_task_name,
               GetDefaultGoopdateTaskName(true, COMMANDLINE_MODE_CORE));
}

TEST(ScheduledTaskUtilsTest, GetDefaultGoopdateTaskName_Core_User) {
  CString expected_task_name_user = kScheduledTaskNameUserPrefix;
  CString user_sid;
  EXPECT_SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid));
  expected_task_name_user += user_sid;
  expected_task_name_user += kScheduledTaskNameCoreSuffix;
  EXPECT_STREQ(expected_task_name_user,
               GetDefaultGoopdateTaskName(false, COMMANDLINE_MODE_CORE));
}

TEST(ScheduledTaskUtilsTest, GetDefaultGoopdateTaskName_UA_Machine) {
  CString expected_task_name(kScheduledTaskNameMachinePrefix);
  expected_task_name += kScheduledTaskNameUASuffix;
  EXPECT_STREQ(expected_task_name,
               GetDefaultGoopdateTaskName(true, COMMANDLINE_MODE_UA));
}

TEST(ScheduledTaskUtilsTest, GetDefaultGoopdateTaskName_UA_User) {
  CString expected_task_name_user = kScheduledTaskNameUserPrefix;
  CString user_sid;
  EXPECT_SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid));
  expected_task_name_user += user_sid;
  expected_task_name_user += kScheduledTaskNameUASuffix;
  EXPECT_STREQ(expected_task_name_user,
               GetDefaultGoopdateTaskName(false, COMMANDLINE_MODE_UA));
}

}  // namespace scheduled_task_utils

}  // namespace omaha

