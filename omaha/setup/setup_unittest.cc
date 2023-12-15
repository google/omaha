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

#include "omaha/setup/setup.h"

#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/process.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system.h"
#include "omaha/base/thread.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/setup/setup_files.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

const int kProcessesCleanupWait = 30000;

const TCHAR* const kFutureVersionString = _T("9.8.7.6");

class HoldLock : public Runnable {
 public:
  explicit HoldLock(bool is_machine)
      : is_machine_(is_machine) {
    reset(lock_acquired_event_, ::CreateEvent(NULL, false, false, NULL));
    reset(stop_event_, ::CreateEvent(NULL, false, false, NULL));
  }

  virtual void Run() {
    GLock setup_lock;
    NamedObjectAttributes setup_lock_attr;
    GetNamedObjectAttributes(omaha::kSetupMutex, is_machine_, &setup_lock_attr);
    EXPECT_TRUE(setup_lock.InitializeWithSecAttr(setup_lock_attr.name,
                                                 &setup_lock_attr.sa));
    __mutexScope(setup_lock);

    EXPECT_TRUE(::SetEvent(get(lock_acquired_event_)));

    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(stop_event_), INFINITE));
  }

  void Stop() {
    EXPECT_TRUE(::SetEvent(get(stop_event_)));
  }

  void WaitForLockToBeAcquired() {
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(get(lock_acquired_event_), 2000));
  }

 private:
  const bool is_machine_;
  scoped_event lock_acquired_event_;
  scoped_event stop_event_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HoldLock);
};

}  // namespace

void CopyGoopdateFiles(const CString& omaha_path, const CString& version);

class SetupTest : public testing::Test {
 protected:
  typedef std::vector<uint32> Pids;

  // Returns the path to the long-running GoogleUpdate.exe.
  static CString CopyGoopdateAndLongRunningFiles(const CString& omaha_path,
                                                 const CString& version) {
    CopyGoopdateFiles(omaha_path, version);

    CString long_running_target_path = ConcatenatePath(omaha_path,
                                                       _T("does_not_shutdown"));
    EXPECT_SUCCEEDED(CreateDir(long_running_target_path, NULL));
    long_running_target_path = ConcatenatePath(long_running_target_path,
                                               kOmahaShellFileName);
    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(ConcatenatePath(
                            app_util::GetCurrentModuleDirectory(),
                            _T("unittest_support\\does_not_shutdown")),
                        kOmahaShellFileName),
        long_running_target_path,
        false));

    return long_running_target_path;
  }

  static void SetUpTestCase() {
    not_listening_machine_exe_path_ =
        CopyGoopdateAndLongRunningFiles(GetGoogleUpdateMachinePath(),
                                        GetVersionString());
    not_listening_user_exe_path_ =
        CopyGoopdateAndLongRunningFiles(GetGoogleUpdateUserPath(),
                                        GetVersionString());
  }

  explicit SetupTest(bool is_machine)
      : is_machine_(is_machine),
        omaha_path_(is_machine ? GetGoogleUpdateMachinePath() :
                                 GetGoogleUpdateUserPath()),
        not_listening_exe_path_(is_machine ? not_listening_machine_exe_path_ :
                                             not_listening_user_exe_path_),
        not_listening_exe_opposite_path_(!is_machine ?
                                         not_listening_machine_exe_path_ :
                                         not_listening_user_exe_path_) {
    omaha_exe_path_ = ConcatenatePath(omaha_path_, MAIN_EXE_BASE_NAME _T(".exe"));
  }

  virtual void SetUp() {
    ASSERT_SUCCEEDED(CreateDir(omaha_path_, NULL));
    setup_.reset(new omaha::Setup(is_machine_));
  }

  bool ShouldInstall() {
    SetupFiles setup_files(is_machine_);
    setup_files.Init();
    return setup_->ShouldInstall();
  }

  HRESULT StopGoogleUpdateAndWait() {
    const int wait_time_before_kill_ms = 2000;
    return setup_->StopGoogleUpdateAndWait(wait_time_before_kill_ms);
  }

  HRESULT TerminateCoreProcesses() const {
    return setup_->TerminateCoreProcesses();
  }

  // Acquires the Setup Lock in another thread then calls TestInstall().
  void TestInstallWhileHoldingLock() {
    HoldLock hold_lock(is_machine_);

    Thread thread;
    thread.Start(&hold_lock);
    hold_lock.WaitForLockToBeAcquired();

    EXPECT_EQ(GOOPDATE_E_FAILED_TO_GET_LOCK,
              setup_->Install(RUNTIME_MODE_NOT_SET));

    hold_lock.Stop();
    thread.WaitTillExit(1000);
  }

  void StopGoogleUpdateAndWaitSucceedsTestHelper(bool use_job_objects_only) {
    if (is_machine_ && !vista_util::IsUserAdmin()) {
      std::wcout << _T("\tTest did not run because the user is not an admin.")
                 << std::endl;
      return;
    }

    if (IsBuildSystem()) {
      std::wcout << _T("\tTest not run because it is flaky on build system.")
                 << std::endl;
      return;
    }

    scoped_process core_process;
    scoped_process install_process;
    scoped_process opposite_process;
    scoped_process user_handoff_process;
    scoped_process user_install_goopdate_process;
    scoped_process user_install_slashinstall_process;
    scoped_process setup_phase1_job_process;
    scoped_process setup_phase1_job_opposite_process;
    scoped_process install_job_opposite_process;
    scoped_process silent_job_opposite_process;
    scoped_process silent_do_not_kill_job_opposite_process;
    scoped_job setup_phase1_job;
    scoped_job setup_phase1_job_opposite;
    scoped_job install_job_opposite;
    scoped_job silent_job_opposite;
    scoped_job silent_do_not_kill_job_opposite;

    StartCoreProcessesToShutdown(address(core_process));
    ASSERT_TRUE(core_process);

    if (use_job_objects_only && is_machine_) {
      // When starting the core process with psexec, there is a race condition
      // between that process initializing (and joining a Job Object) and
      // StopGoogleUpdateAndWait() looking for processes. If the latter wins,
      // the core process is not found and StopGoogleUpdateAndWait() does not
      // wait for the core process. As a result,
      // ::WaitForSingleObject(get(core_process), 0)) would fail intermittently.
      // Sleep here to allow the process to start and join the job.
      // Note that this race condition is similar to ones we might encounter
      // in the field when using Job Objects.
      ::Sleep(500);
    }

    // /install is always ignored.
    LaunchProcess(not_listening_exe_path_,
                  _T("/install foobar"),
                  is_machine_,
                  address(install_process));
    ASSERT_TRUE(install_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(install_process), 0));

    if (vista_util::IsUserAdmin()) {
      // GoogleUpdate running from the opposite directory should always be
      // ignored. Using a command line that would not be ignored if it were not
      // an opposite.
      LaunchProcess(not_listening_exe_opposite_path_,
                    _T(""),
                    false,
                    address(opposite_process));
      EXPECT_TRUE(opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(opposite_process), 0));
    } else {
      EXPECT_FALSE(is_machine_)
          << _T("Unexpected call for machine when non-admin.");
      // We can't launch a system process when non-admin.
      std::wcout << _T("\tPart of this test did not run because the user is ")
                    _T("not an admin.") << std::endl;
    }

    CString same_needsadmin = is_machine_ ? _T("\"needsadmin=True\"") :
                                            _T("\"needsadmin=False\"");
    // Machine setup looks for users running most modes from the machine
    // official directory, and user setup looks for users running most modes
    // from the user official directory.
    // Launching with needsadmin=<same> tests that machine still ignores
    // needsadmin=True and user ignores needsadmin=False when the opposite
    // instances are running.
    LaunchProcess(not_listening_exe_opposite_path_,
                  _T("/handoff ") + same_needsadmin,
                  false,  // As the user.
                  address(user_handoff_process));
    EXPECT_TRUE(user_handoff_process);
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForSingleObject(get(user_handoff_process), 0));

    // This process should be ignored even though it is running from the correct
    // official directory.
    LaunchProcess(not_listening_exe_path_,
                  _T("/install ") + same_needsadmin,
                  false,  // As the user.
                  address(user_install_slashinstall_process));
    EXPECT_TRUE(user_install_goopdate_process);
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForSingleObject(get(user_install_goopdate_process), 0));

    if (use_job_objects_only) {
      // This Job Object is ignored.
      // Only start this process when only using Job Objects because the
      // argument-less process would be caught by the command line search.
      LaunchJobProcess(is_machine_,
                       is_machine_,
                       kSetupPhase1NonSelfUpdateJobObject,
                       address(setup_phase1_job_process),
                       address(setup_phase1_job));
      ASSERT_TRUE(setup_phase1_job_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(setup_phase1_job_process), 0));
    }

    if (is_machine_ || vista_util::IsUserAdmin()) {
      // These processes should be ignored because they are for the opposite
      // set of processes.

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kSetupPhase1NonSelfUpdateJobObject,
                       address(setup_phase1_job_opposite_process),
                       address(setup_phase1_job_opposite));
      ASSERT_TRUE(setup_phase1_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(setup_phase1_job_opposite_process),
                                      0));

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kAppInstallJobObject,
                       address(install_job_opposite_process),
                       address(install_job_opposite));
      ASSERT_TRUE(install_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(install_job_opposite_process), 0));

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kSilentJobObject,
                       address(silent_job_opposite_process),
                       address(silent_job_opposite));
      ASSERT_TRUE(install_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(install_job_opposite_process), 0));

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kSilentDoNotKillJobObject,
                       address(silent_do_not_kill_job_opposite_process),
                       address(silent_do_not_kill_job_opposite));
      ASSERT_TRUE(install_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(install_job_opposite_process), 0));
    }

    EXPECT_SUCCEEDED(StopGoogleUpdateAndWait());
    EXPECT_EQ(0, setup_->extra_code1());

    // Verify the real core process exited and terminate the processes that are
    // not listening to shutdown.
    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(core_process), 0));

    // Terminate all the processes and wait for them to exit to avoid
    // interfering with other tests.
    std::vector<HANDLE> started_processes;
    started_processes.push_back(get(install_process));
    if (vista_util::IsUserAdmin()) {
      started_processes.push_back(get(opposite_process));
    }
    started_processes.push_back(get(user_handoff_process));
    started_processes.push_back(get(user_install_goopdate_process));
    started_processes.push_back(get(user_install_slashinstall_process));
    if (use_job_objects_only) {
      started_processes.push_back(get(setup_phase1_job_process));
    }
    if (is_machine_ || vista_util::IsUserAdmin()) {
      started_processes.push_back(get(setup_phase1_job_opposite_process));
      started_processes.push_back(get(install_job_opposite_process));
      started_processes.push_back(get(silent_job_opposite_process));
      started_processes.push_back(get(silent_do_not_kill_job_opposite_process));
    }

    for (size_t i = 0; i < started_processes.size(); ++i) {
      SETUP_LOG(L1, (_T("Terminating PID %u]"),
                     ::GetProcessId(started_processes[i])));
      EXPECT_TRUE(::TerminateProcess(started_processes[i], 1));
    }
    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForMultipleObjects(
                                  static_cast<DWORD>(started_processes.size()),
                                  &started_processes[0],
                                  true,  // wait for all
                                  kProcessesCleanupWait));
  }

  void StopGoogleUpdateAndWaitSucceedsTest() {
    StopGoogleUpdateAndWaitSucceedsTestHelper(false);
  }

  HRESULT GetRunningCoreProcesses(Pids* core_processes) {
    ASSERT1(core_processes);

    CString user_sid_to_use;
    if (is_machine_) {
      user_sid_to_use = kLocalSystemSid;
    } else {
      EXPECT_SUCCEEDED(user_info::GetProcessUser(NULL, NULL, &user_sid_to_use));
    }

    DWORD flags = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                  EXCLUDE_CURRENT_PROCESS |
                  INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;

    std::vector<CString> command_lines;
    command_lines.push_back(_T("/c"));

    return Process::FindProcesses(flags,
                                  kOmahaShellFileName,
                                  true,
                                  user_sid_to_use,
                                  command_lines,
                                  core_processes);
  }

  void KillRunningCoreProcesses() {
    Pids core_processes;
    EXPECT_SUCCEEDED(GetRunningCoreProcesses(&core_processes));

    for (size_t i = 0; i < core_processes.size(); ++i) {
      scoped_process process(::OpenProcess(PROCESS_TERMINATE,
                                           FALSE,
                                           core_processes[i]));
      EXPECT_TRUE(process);
      EXPECT_TRUE(::TerminateProcess(get(process), static_cast<uint32>(-2)));
    }
  }

  // Uses psexec to start processes as SYSTEM if necessary.
  // Assumes that psexec blocks until the process exits.
  // TODO(omaha): Start the opposite instances and wait for them in
  // StopGoogleUpdateAndWaitSucceedsTest. They should not close.
  void StartCoreProcessesToShutdown(HANDLE* core_process) {
    ASSERT_TRUE(core_process);

    // Find the core process or start one if necessary.
    Pids core_processes;
    EXPECT_SUCCEEDED(GetRunningCoreProcesses(&core_processes));
    ASSERT_LE(core_processes.size(), static_cast<size_t>(1))
        << _T("Only one core should be running.");

    if (core_processes.empty()) {
      LaunchProcess(omaha_exe_path_,
                    _T("/c"),
                    is_machine_,
                    core_process);
    } else {
      *core_process = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE,
                                    FALSE,
                                    core_processes[0]);
    }
    ASSERT_TRUE(*core_process);

    HANDLE processes[] = {*core_process};
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForMultipleObjects(arraysize(processes),
                                                     processes,
                                                     true,  // wait for all
                                                     0));
  }

  // Launches an instance of GoogleUpdate.exe that doesn't exit.
  void StopGoogleUpdateAndWaitProcessesDoNotStopTest() {
    LaunchProcessAndExpectStopGoogleUpdateAndWaitKillsProcess(
        is_machine_,
        _T(""));
  }

  void LaunchProcessAndExpectStopGoogleUpdateAndWaitKillsProcess(
      bool is_machine_process,
      const CString& args) {
    ASSERT_TRUE(args);

    if (is_machine_ && !vista_util::IsUserAdmin()) {
      std::wcout << _T("\tTest did not run because the user is not an admin.")
                 << std::endl;
      return;
    }

    scoped_process process;
    LaunchProcess(not_listening_exe_path_,
                  args,
                  is_machine_process,
                  address(process));
    ASSERT_TRUE(process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(process), 0));

    // Tests that use this method intermittently fail when run on build systems.
    // This code attempts to ensure that the process is further along in the
    // initialization process by waiting until Process::GetCommandLine succeeds.
    HRESULT hr = E_FAIL;
    CString process_cmd;
    for (int tries = 0; tries < 100 && FAILED(hr); ++tries) {
      hr = Process::GetCommandLine(::GetProcessId(get(process)), &process_cmd);
      if (FAILED(hr)) {
        ::Sleep(50);
      }
    }
    EXPECT_SUCCEEDED(hr);

    EXPECT_EQ(S_OK, StopGoogleUpdateAndWait());
    // Make sure the process has been killed.
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(get(process), kProcessesCleanupWait));
  }

  // Starts a core process for this user, a core for the opposite type of user,
  // and a /cr process (similar command line to /c), and a process without args.
  void TestTerminateCoreProcessesWithBothTypesRunningAndOtherProcesses() {
    scoped_process core_process;
    scoped_process opposite_core_process;
    scoped_process codered_process;
    scoped_process noargs_process;
    LaunchProcess(not_listening_exe_path_,
                  _T("/c"),
                  is_machine_,
                  address(core_process));
    ASSERT_TRUE(core_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(core_process), 0));
    LaunchProcess(not_listening_exe_opposite_path_,
                  _T("/c"),
                  !is_machine_,
                  address(opposite_core_process));
    ASSERT_TRUE(opposite_core_process);
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForSingleObject(get(opposite_core_process), 0));
    LaunchProcess(not_listening_exe_path_,
                  _T("/cr"),
                  is_machine_,
                  address(codered_process));
    ASSERT_TRUE(codered_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(codered_process), 0));
    LaunchProcess(not_listening_exe_path_,
                  _T(""),
                  is_machine_,
                  address(noargs_process));
    ASSERT_TRUE(noargs_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(noargs_process), 0));

    EXPECT_SUCCEEDED(TerminateCoreProcesses());

    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(core_process), 8000));
    HANDLE ignored_processes[] = {get(opposite_core_process),
                                  get(codered_process),
                                  get(noargs_process)};
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForMultipleObjects(arraysize(ignored_processes),
                                                 ignored_processes,
                                                 false,  // wait for any
                                                 1000));

    HANDLE started_processes[] = {get(core_process),
                                  get(opposite_core_process),
                                  get(codered_process),
                                  get(noargs_process)};
    EXPECT_TRUE(::TerminateProcess(get(opposite_core_process), 1));
    EXPECT_TRUE(::TerminateProcess(get(codered_process), 1));
    EXPECT_TRUE(::TerminateProcess(get(noargs_process), 1));
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForMultipleObjects(arraysize(started_processes),
                                                 started_processes,
                                                 true,  // wait for all
                                                 kProcessesCleanupWait));
  }

  // Starts test process that doesn't exit and assigns it to the specified job.
  // The handle returned by ShellExecute does not have PROCESS_SET_QUOTA access
  // rights, so we get a new handle with the correct access rights to return to
  // the caller.
  void LaunchJobProcess(bool is_machine,
                        bool as_system,
                        const CString& job_base_name,
                        HANDLE* process,
                        HANDLE* job) {
    ASSERT_TRUE(process);
    ASSERT_TRUE(job);
    ASSERT_TRUE(is_machine || !as_system);

    scoped_process launched_process;
    LaunchProcess(is_machine ? not_listening_machine_exe_path_ :
                               not_listening_user_exe_path_,
                  _T(""),
                  as_system,
                  address(launched_process));
    ASSERT_TRUE(launched_process);

    *process = ::OpenProcess(PROCESS_ALL_ACCESS,
                             false,
                             ::GetProcessId(get(launched_process)));
    ASSERT_TRUE(*process);

    NamedObjectAttributes job_attr;
    GetNamedObjectAttributes(job_base_name, is_machine, &job_attr);
    *job = ::CreateJobObject(&job_attr.sa, job_attr.name);
    ASSERT_TRUE(*job);

    if (ERROR_ALREADY_EXISTS != ::GetLastError()) {
      // Configure the newly created Job Object so it is compatible with any
      // real processes that may be created.
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION extended_info = {0};

      ASSERT_TRUE(::QueryInformationJobObject(
                      *job,
                      ::JobObjectExtendedLimitInformation,
                      &extended_info,
                      sizeof(extended_info),
                      NULL)) <<
          _T("Last Error: ") << ::GetLastError() << std::endl;

      extended_info.BasicLimitInformation.LimitFlags =
          JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
      ASSERT_TRUE(::SetInformationJobObject(*job,
                                            ::JobObjectExtendedLimitInformation,
                                            &extended_info,
                                            sizeof(extended_info))) <<
          _T("Last Error: ") << ::GetLastError() << std::endl;
    }

    ASSERT_TRUE(::AssignProcessToJobObject(*job, *process)) <<
        _T("Last Error: ") << ::GetLastError() << std::endl;
  }

  void TestGetRuntimeMode() {
    EXPECT_EQ(RUNTIME_MODE_NOT_SET, setup_->GetRuntimeMode());

    const TCHAR* key = ConfigManager::Instance()->registry_update(is_machine_);

    EXPECT_SUCCEEDED(RegKey::SetValue(key,
                                      kRegValueRuntimeMode,
                                      static_cast<DWORD>(RUNTIME_MODE_TRUE)));
    EXPECT_EQ(RUNTIME_MODE_TRUE, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(RegKey::SetValue(
        key, kRegValueRuntimeMode, static_cast<DWORD>(RUNTIME_MODE_PERSIST)));
    EXPECT_EQ(RUNTIME_MODE_PERSIST, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(RegKey::SetValue(key,
                                      kRegValueRuntimeMode,
                                      static_cast<DWORD>(RUNTIME_MODE_FALSE)));
    EXPECT_EQ(RUNTIME_MODE_NOT_SET, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(RegKey::SetValue(key, kRegValueRuntimeMode, 999UL));
    EXPECT_EQ(RUNTIME_MODE_NOT_SET, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(RegKey::DeleteValue(key, kRegValueRuntimeMode));
    EXPECT_EQ(RUNTIME_MODE_NOT_SET, setup_->GetRuntimeMode());
  }

  void TestSetRuntimeMode() {
    EXPECT_EQ(RUNTIME_MODE_NOT_SET, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(setup_->SetRuntimeMode(RUNTIME_MODE_TRUE));
    EXPECT_EQ(RUNTIME_MODE_TRUE, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(setup_->SetRuntimeMode(RUNTIME_MODE_PERSIST));
    EXPECT_EQ(RUNTIME_MODE_PERSIST, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(setup_->SetRuntimeMode(RUNTIME_MODE_NOT_SET));
    EXPECT_EQ(RUNTIME_MODE_PERSIST, setup_->GetRuntimeMode());

    EXPECT_SUCCEEDED(setup_->SetRuntimeMode(RUNTIME_MODE_FALSE));
    EXPECT_EQ(RUNTIME_MODE_NOT_SET, setup_->GetRuntimeMode());
  }

  const bool is_machine_;
  const CString omaha_path_;
  CString omaha_exe_path_;
  CString not_listening_exe_path_;
  CString not_listening_exe_opposite_path_;
  std::unique_ptr<omaha::Setup> setup_;

  static CString not_listening_user_exe_path_;
  static CString not_listening_machine_exe_path_;
};

CString SetupTest::not_listening_user_exe_path_;
CString SetupTest::not_listening_machine_exe_path_;

class SetupMachineTest : public SetupTest {
 protected:
  SetupMachineTest() : SetupTest(true) {
  }

  static void SetUpTestCase() {
    // SeDebugPrivilege is required for elevated admins to open process open
    // Local System processes with PROCESS_QUERY_INFORMATION access rights.
    System::AdjustPrivilege(SE_DEBUG_NAME, true);

    SetupTest::SetUpTestCase();
  }
};

class SetupUserTest : public SetupTest {
 protected:
  SetupUserTest() : SetupTest(false) {
  }
};

class SetupFutureVersionInstalledUserTest : public SetupUserTest {
 protected:
  SetupFutureVersionInstalledUserTest()
      : SetupUserTest(),
        future_version_path_(ConcatenatePath(omaha_path_,
                                             kFutureVersionString)) {
  }

  virtual void SetUp() {
    SetupUserTest::SetUp();

    // Save the existing version if present.
    RegKey::GetValue(USER_REG_CLIENTS_GOOPDATE,
                     kRegValueProductVersion,
                     &existing_version_);
    InstallFutureVersion();
  }

  virtual void TearDown() {
    EXPECT_SUCCEEDED(DeleteDirectory(future_version_path_));
    if (existing_version_.IsEmpty()) {
      EXPECT_SUCCEEDED(RegKey::DeleteValue(USER_REG_CLIENTS_GOOPDATE,
                                           kRegValueProductVersion));
    } else {
      EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                        kRegValueProductVersion,
                                        existing_version_));
    }

    SetupUserTest::TearDown();
  }

  void InstallFutureVersion() {
    DeleteDirectory(future_version_path_);
    EXPECT_FALSE(File::IsDirectory(future_version_path_));

    EXPECT_SUCCEEDED(CreateDir(future_version_path_, NULL));

    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        kOmahaShellFileName),
        omaha_path_ + kOmahaShellFileName,
        false));

    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        kOmahaDllName),
        ConcatenatePath(future_version_path_, kOmahaDllName),
        false));
    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        _T("goopdateres_en.dll")),
        ConcatenatePath(future_version_path_, _T("goopdateres_en.dll")),
        false));

    EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                      kRegValueProductVersion,
                                      kFutureVersionString));
  }

  CString existing_version_;  // Saves the existing version from the registry.
  const CString future_version_path_;
  static const TCHAR* const kAppGuid_;
};

const TCHAR* const SetupFutureVersionInstalledUserTest::kAppGuid_ =
    _T("{01D33078-BA95-4da6-A3FC-F31593FD4AA2}");

class SetupRegistryProtectedUserTest : public SetupUserTest {
 protected:
  SetupRegistryProtectedUserTest() : SetupUserTest() {
  }

  static void SetUpTestCase() {
    this_version_ = GetVersionString();
  }

  static CString this_version_;
};

CString SetupRegistryProtectedUserTest::this_version_;

class SetupRegistryProtectedMachineTest : public SetupMachineTest {
 protected:
  SetupRegistryProtectedMachineTest()
      : SetupMachineTest(),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  virtual void SetUp() {
    SetupMachineTest::SetUp();

    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));

    SetupMachineTest::TearDown();
  }

  const CString hive_override_key_name_;
};

// TODO(omaha3): These tests are left over from Omaha2's InstallSelfSilently(),
// which like Omaha3's Install(), does not install the app. It did, however,
// start another process to complete installation. This would cause it to fail
// if the shell or main DLL were missing.
TEST_F(SetupFutureVersionInstalledUserTest, DISABLED_Install_NoRunKey) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);

  // Write the future version to the override registry.
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  CString dll_path = ConcatenatePath(future_version_path_, kOmahaDllName);
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            setup_->Install(RUNTIME_MODE_NOT_SET));
  EXPECT_EQ(0, setup_->extra_code1());

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

// Command line must be valid to avoid displaying invalid command line error.
TEST_F(SetupFutureVersionInstalledUserTest, Install_ValidRunKey) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);

  // Write the future version to the override registry.
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  const TCHAR kRunKey[] =
      _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
  const CString shell_path = ConcatenatePath(omaha_path_, kOmahaShellFileName);
  CString run_value;
  run_value.Format(_T("\"%s\" /cr"), shell_path);
  ASSERT_SUCCEEDED(RegKey::SetValue(kRunKey, _T("Google Update"), run_value));

  CString dll_path = ConcatenatePath(future_version_path_, kOmahaDllName);
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  EXPECT_SUCCEEDED(setup_->Install(RUNTIME_MODE_NOT_SET));
  EXPECT_EQ(0, setup_->extra_code1());

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(SetupUserTest, Install_LockTimedOut) {
  TestInstallWhileHoldingLock();
}

TEST_F(SetupMachineTest, Install_LockTimedOut) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  TestInstallWhileHoldingLock();
}

//
// ShouldInstall tests.
//

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_OlderVersion) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    _T("1.0.3.4")));
  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_NewerVersion) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledPath,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENT_STATE_GOOPDATE));

  CopyGoopdateFiles(omaha_path_, kFutureVersionString);
  EXPECT_FALSE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_NewerVersionMissingInstalledVersion) {
  ASSERT_SUCCEEDED(RegKey::DeleteValue(USER_REG_UPDATE,
                                       kRegValueInstalledVersion));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  CopyGoopdateFiles(omaha_path_, kFutureVersionString);
  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_NewerVersionMissingProductVersion) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::DeleteValue(USER_REG_CLIENTS_GOOPDATE,
                                       kRegValueProductVersion));

  CopyGoopdateFiles(omaha_path_, kFutureVersionString);
  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_NewerVersionFilesMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(
      DeleteDirectory(ConcatenatePath(omaha_path_, kFutureVersionString)));
  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_NewerVersionShellMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  CopyGoopdateFiles(omaha_path_, kFutureVersionString);
  CString shell_path = ConcatenatePath(omaha_path_, kOmahaShellFileName);
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_SameVersionFilesMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(
      DeleteDirectory(ConcatenatePath(omaha_path_, this_version_)));
  CString file_path = ConcatenatePath(
                          ConcatenatePath(omaha_path_, this_version_),
                          kOmahaDllName);
  ASSERT_FALSE(File::Exists(file_path));

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_SameVersionFilesPresent) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_FALSE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentMissingInstalledVer) {
  ASSERT_SUCCEEDED(RegKey::DeleteValue(USER_REG_UPDATE,
                                       kRegValueInstalledVersion));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentMissingProductVersion) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::DeleteValue(USER_REG_CLIENTS_GOOPDATE,
                                       kRegValueProductVersion));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentNewerInstalledVer) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    kFutureVersionString));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionRequiredFileMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString path = ConcatenatePath(ConcatenatePath(omaha_path_, this_version_),
                                 kOmahaDllName);
  ASSERT_SUCCEEDED(File::Remove(path));
  ASSERT_FALSE(File::Exists(path));

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionOptionalFileMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString path = ConcatenatePath(ConcatenatePath(omaha_path_, this_version_),
                                 kPSFileNameMachine64);
  ASSERT_SUCCEEDED(File::Remove(path));
  ASSERT_FALSE(File::Exists(path));

  EXPECT_FALSE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_SameVersionShellMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString shell_path = ConcatenatePath(omaha_path_, kOmahaShellFileName);
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  EXPECT_TRUE(ShouldInstall());
}

//
// GetRuntimeMode/SetRuntimeMode tests.
//

TEST_F(SetupRegistryProtectedUserTest, GetRuntimeMode) {
  TestGetRuntimeMode();
}

TEST_F(SetupRegistryProtectedUserTest, SetRuntimeMode) {
  TestSetRuntimeMode();
}

TEST_F(SetupRegistryProtectedMachineTest, GetRuntimeMode) {
  TestGetRuntimeMode();
}

TEST_F(SetupRegistryProtectedMachineTest, SetRuntimeMode) {
  TestSetRuntimeMode();
}

//
// StopGoogleUpdateAndWait tests.
//
// These are "large" tests.
// They kill currently running GoogleUpdate processes, including the core, owned
// by the current user or SYSTEM.
// A core may already be running, so if a core process is found, it is used for
// the tests. Otherwise, they launch a core from a previous build.
// Using a previous build ensures that the shutdown event hasn't changed.
// There is no test directly that this version can shutdown itself, but that is
// much less likely to break.
// The Succeeds tests will fail if any processes that don't listen to the
// shutdown event are running.
//

// TODO(omaha3): Make these tests pass.
TEST_F(SetupUserTest, DISABLED_StopGoogleUpdateAndWait_Succeeds) {
  StopGoogleUpdateAndWaitSucceedsTest();
}

TEST_F(SetupMachineTest, DISABLED_StopGoogleUpdateAndWait_Succeeds) {
  StopGoogleUpdateAndWaitSucceedsTest();
}

// TODO(omaha): If start using Job Objects again, enable these tests.
/*
TEST_F(SetupUserTest, StopGoogleUpdateAndWait_SucceedsUsingOnlyJobObjects) {
  StopGoogleUpdateAndWaitWithProcessSearchDisabledSucceedsTest();
}

TEST_F(SetupMachineTest, StopGoogleUpdateAndWait_SucceedsUsingOnlyJobObjects) {
  StopGoogleUpdateAndWaitWithProcessSearchDisabledSucceedsTest();
}
*/

TEST_F(SetupUserTest, StopGoogleUpdateAndWait_ProcessesDoNotStop) {
  StopGoogleUpdateAndWaitProcessesDoNotStopTest();
}

TEST_F(SetupMachineTest, StopGoogleUpdateAndWait_ProcessesDoNotStop) {
  StopGoogleUpdateAndWaitProcessesDoNotStopTest();
}

TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_MachineHandoffWorkerRunningAsUser) {
  LaunchProcessAndExpectStopGoogleUpdateAndWaitKillsProcess(
      false,
      _T("/handoff \"needsadmin=True\""));
}

// Process mode is unknown because Omaha 3 does not recognize IG.
TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_MachineLegacyInstallGoogleUpdateWorkerRunningAsUser) {   // NOLINT
  LaunchProcessAndExpectStopGoogleUpdateAndWaitKillsProcess(
      false,
      _T("/ig \"needsadmin=True\""));
}

TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_UserHandoffWorkerRunningAsSystem) {
  LaunchProcessAndExpectStopGoogleUpdateAndWaitKillsProcess(
      true,
      _T("/handoff \"needsadmin=False\""));
}

// Process mode is unknown because Omaha 3 does not recognize IG.
TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_UserLegacyInstallGoogleUpdateWorkerRunningAsSystem) {   // NOLINT
  LaunchProcessAndExpectStopGoogleUpdateAndWaitKillsProcess(
      true,
      _T("/ig \"needsadmin=False\""));
}

TEST_F(SetupUserTest, TerminateCoreProcesses_NoneRunning) {
  KillRunningCoreProcesses();

  EXPECT_SUCCEEDED(TerminateCoreProcesses());
}

TEST_F(SetupUserTest,
       TerminateCoreProcesses_BothTypesRunningAndSimilarArgsProcess) {
  TestTerminateCoreProcessesWithBothTypesRunningAndOtherProcesses();
}

TEST_F(SetupMachineTest, TerminateCoreProcesses_NoneRunning) {
  KillRunningCoreProcesses();

  EXPECT_SUCCEEDED(TerminateCoreProcesses());
}

TEST_F(SetupMachineTest,
       TerminateCoreProcesses_BothTypesRunningAndSimilarArgsProcess) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
}
  TestTerminateCoreProcessesWithBothTypesRunningAndOtherProcesses();
}

}  // namespace omaha
