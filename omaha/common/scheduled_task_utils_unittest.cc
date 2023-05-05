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
#include <atltime.h>
#include <mstask.h>

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
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
    _T("unittest_support\\does_not_shutdown\\") MAIN_EXE_BASE_NAME _T(".exe");

CString GetLongRunningProcessPath() {
  const CString module_dir(app_util::GetCurrentModuleDirectory());
  return ConcatenatePath(module_dir, kLongRunningProcessesRelativePath);
}

}  // namespace

namespace scheduled_task_utils {

using internal::GetCurrentTaskNameCore;
using internal::GetCurrentTaskNameUA;
using internal::Instance;
using internal::IsTaskScheduler2APIAvailable;
using internal::WaitForTaskStatus;

using vista_util::IsUserAdmin;

namespace internal {

void StopScheduledTaskAndVerifyReadyState(const CString& task_name) {
  HRESULT wait_for_state(IsTaskScheduler2APIAvailable() ?
                         SCHED_S_TASK_TERMINATED :
                         SCHED_S_TASK_READY);

  // For some reason, StopScheduleTask may not successfully stop the task
  // even it returns S_OK. So try to stop multiple times.
  for (int i = 0; i < 3; ++i) {
    EXPECT_SUCCEEDED(Instance().StopScheduledTask(task_name));

    if (wait_for_state == WaitForTaskStatus(task_name,
                                            wait_for_state,
                                            kMsPerSec)) {
      break;
    }
  }
  EXPECT_EQ(wait_for_state, Instance().GetScheduledTaskStatus(task_name));
}

TEST(ScheduledTaskUtilsTest, ScheduledTasks) {
  const TCHAR kSchedTestTaskName[]            = _T("TestScheduledTask");
  const TCHAR kScheduledTaskExecutable[]      = _T("netstat.exe");
  const TCHAR kScheduledTaskParameters[]      = _T("20");
  const TCHAR kSchedTestTaskComment[]         = _T("Google Test Task");

  const CString task_path = ConcatenatePath(app_util::GetSystemDir(),
                                            kScheduledTaskExecutable);
  // Install/uninstall.
  EXPECT_SUCCEEDED(Instance().InstallScheduledTask(
                                  kSchedTestTaskName,
                                  task_path,
                                  _T(""),
                                  kSchedTestTaskComment,
                                  IsUserAdmin(),
                                  IsUserAdmin(),
                                  true));
  EXPECT_SUCCEEDED(Instance().UninstallScheduledTask(kSchedTestTaskName));

  // Calling InstallScheduledTask twice should succeed.
  for (int i = 0; i < 2; ++i) {
    EXPECT_SUCCEEDED(Instance().InstallScheduledTask(
                                    kSchedTestTaskName,
                                    task_path,
                                    _T(""),
                                    kSchedTestTaskComment,
                                    IsUserAdmin(),
                                    IsUserAdmin(),
                                    true));
  }

  // "Upgrade" to a new version, which now has parameters.
  EXPECT_SUCCEEDED(Instance().InstallScheduledTask(
                                  kSchedTestTaskName,
                                  task_path,
                                  kScheduledTaskParameters,
                                  kSchedTestTaskComment,
                                  IsUserAdmin(),
                                  IsUserAdmin(),
                                  true));

  EXPECT_FALSE(Instance().HasScheduledTaskEverRun(kSchedTestTaskName));
  EXPECT_SUCCEEDED(Instance().StartScheduledTask(kSchedTestTaskName));
  EXPECT_EQ(SCHED_S_TASK_RUNNING,
            WaitForTaskStatus(kSchedTestTaskName,
                              SCHED_S_TASK_RUNNING,
                              kMaxWaitForProcessMs));

  EXPECT_TRUE(Instance().HasScheduledTaskEverRun(kSchedTestTaskName));

  StopScheduledTaskAndVerifyReadyState(kSchedTestTaskName);

  // Finally, uninstall.
  EXPECT_SUCCEEDED(Instance().UninstallScheduledTask(kSchedTestTaskName));
}

class ScheduledTaskUtilsV2Test : public ::testing::TestWithParam<bool> {
 protected:
  bool IsMachine() {
    return GetParam();
  }

  static HRESULT CreateScheduledTaskXml(const CString& task_path,
                                        const CString& task_parameters,
                                        const CString& task_description,
                                        const CString& start_time,
                                        bool is_machine,
                                        bool create_logon_trigger,
                                        bool create_hourly_trigger,
                                        CString* scheduled_task_xml) {
    return V2ScheduledTasks::CreateScheduledTaskXml(task_path,
                                                    task_parameters,
                                                    task_description,
                                                    start_time,
                                                    is_machine,
                                                    create_logon_trigger,
                                                    create_hourly_trigger,
                                                    scheduled_task_xml);
  }
};

INSTANTIATE_TEST_CASE_P(IsMachine,
                        ScheduledTaskUtilsV2Test,
                        ::testing::Bool());

TEST_P(ScheduledTaskUtilsV2Test, CreateScheduledTaskXml) {
  if (!IsTaskScheduler2APIAvailable()) {
    std::wcout << _T("\tTest did not run because this OS does not support the ")
                  _T("Task Scheduler 2.0 API.") << std::endl;
    return;
  }

  const TCHAR kSchedTestTaskName[]            = _T("TestScheduledTaskV2");
  const TCHAR kScheduledTaskExecutable[]      = _T("netstat.exe");
  const TCHAR kScheduledTaskParameters[]      = _T("20");
  const TCHAR kSchedTestTaskDescription[]     = _T("Google Test Task V2");

  const CTime plus_5min(CTime::GetCurrentTime() + CTimeSpan(0, 0, 5, 0));
  const CString start_time(plus_5min.Format(_T("%Y-%m-%dT%H:%M:%S")));

  const CString task_path = ConcatenatePath(app_util::GetSystemDir(),
                                            kScheduledTaskExecutable);
  CString logon_trigger;
  if (IsMachine()) {
    logon_trigger =
        _T("  <LogonTrigger>\n")
        _T("    <Enabled>true</Enabled>\n")
        _T("  </LogonTrigger>\n");
  }

  CString hourly_trigger =
        _T("    <Repetition>\n")
        _T("      <Interval>PT1H</Interval>\n")
        _T("      <Duration>P1D</Duration>\n")
        _T("    </Repetition>\n");

  CString user_id;
  CString principal_attributes;

  if (IsMachine()) {
    user_id = _T("S-1-5-18");
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

  CString expected_task_xml;
  SafeCStringFormat(
      &expected_task_xml,
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
      kSchedTestTaskDescription,
      logon_trigger,
      start_time,
      hourly_trigger,
      user_id,
      principal_attributes,
      quoted_task_path,
      kScheduledTaskParameters);

  CString task_xml;
  EXPECT_SUCCEEDED(ScheduledTaskUtilsV2Test::CreateScheduledTaskXml(task_path,
                                              kScheduledTaskParameters,
                                              kSchedTestTaskDescription,
                                              start_time,
                                              IsMachine(),
                                              IsMachine(),
                                              true,
                                              &task_xml));

  EXPECT_STREQ(expected_task_xml, task_xml);
}

TEST_P(ScheduledTaskUtilsV2Test, InstallScheduledTask) {
  if (!IsTaskScheduler2APIAvailable()) {
    std::wcout << _T("\tTest did not run because this OS does not support the ")
                  _T("Task Scheduler 2.0 API.") << std::endl;
    return;
  }

  const TCHAR kSchedTestTaskName[]            = _T("TestScheduledTaskV2");
  const TCHAR kScheduledTaskExecutable[]      = _T("netstat.exe");
  const TCHAR kScheduledTaskParameters[]      = _T("20");
  const TCHAR kSchedTestTaskDescription[]     = _T("Google Test Task V2");

  const CTime plus_5min(CTime::GetCurrentTime() + CTimeSpan(0, 0, 5, 0));
  const CString start_time(plus_5min.Format(_T("%Y-%m-%dT%H:%M:%S")));

  const CString task_path = ConcatenatePath(app_util::GetSystemDir(),
                                            kScheduledTaskExecutable);
  EXPECT_SUCCEEDED(Instance().InstallScheduledTask(
                                  kSchedTestTaskName,
                                  task_path,
                                  kScheduledTaskParameters,
                                  kSchedTestTaskDescription,
                                  IsMachine(),
                                  IsMachine(),
                                  true));

  EXPECT_SUCCEEDED(Instance().UninstallScheduledTask(kSchedTestTaskName));
}

}  // namespace internal


TEST(ScheduledTaskUtilsTest, GoopdateTasks) {
  const CString task_path = GetLongRunningProcessPath();

  // Install/uninstall.
  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(IsUserAdmin()));

  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  const CString task_name = GetCurrentTaskNameCore(IsUserAdmin());
  EXPECT_FALSE(Instance().HasScheduledTaskEverRun(task_name));

  // Start and stop.
  EXPECT_EQ(SCHED_S_TASK_HAS_NOT_RUN,
            Instance().GetScheduledTaskStatus(task_name));
  EXPECT_SUCCEEDED(StartGoopdateTaskCore(IsUserAdmin()));

  EXPECT_EQ(SCHED_S_TASK_RUNNING,
            WaitForTaskStatus(task_name,
                              SCHED_S_TASK_RUNNING,
                              kMaxWaitForProcessMs));

  EXPECT_TRUE(Instance().HasScheduledTaskEverRun(task_name));

  internal::StopScheduledTaskAndVerifyReadyState(task_name);

  // Finally, uninstall.
  EXPECT_SUCCEEDED(UninstallGoopdateTasks(IsUserAdmin()));
}

TEST(ScheduledTaskUtilsTest, V1OnlyGoopdateTaskInUseOverinstall) {
  if (IsTaskScheduler2APIAvailable()) {
    std::wcout << _T("\tTest did not run because this OS supports the ")
                  _T("Task Scheduler 2.0 API.") << std::endl;
    return;
  }

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
  const CString task_path = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("unittest_support\\SaveArguments.exe"));

  EXPECT_SUCCEEDED(InstallGoopdateTasks(task_path, IsUserAdmin()));
  const CString task_name = GetCurrentTaskNameUA(IsUserAdmin());
  EXPECT_EQ(SCHED_S_TASK_HAS_NOT_RUN,
            GetExitCodeGoopdateTaskUA(IsUserAdmin()));
  EXPECT_FALSE(Instance().HasScheduledTaskEverRun(task_name));

  // Start the task and wait for it to run and become ready again. The task
  // runs a program that returns right away. Sometimes the task does not run
  // for unknown reason. Attempting to run the task multiple times does not
  // work. This remains a flaky test.
  EXPECT_SUCCEEDED(Instance().StartScheduledTask(task_name));
  HRESULT wait_for_state(IsTaskScheduler2APIAvailable() ?
                         S_OK :
                         SCHED_S_TASK_READY);
  EXPECT_EQ(wait_for_state, WaitForTaskStatus(task_name,
                                              wait_for_state,
                                              kMaxWaitForProcessMs));
  EXPECT_TRUE(Instance().HasScheduledTaskEverRun(task_name));
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

