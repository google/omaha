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


#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/app_util.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/process.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/system.h"
#include "omaha/common/thread.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/setup/setup.h"
#include "omaha/setup/setup_files.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const int kProcessesCleanupWait = 30000;

const TCHAR* const kFutureVersionString = _T("9.8.7.6");

const TCHAR* const kAppMachineClientsPath =
    _T("HKLM\\Software\\Google\\Update\\Clients\\")
    _T("{50DA5C89-FF97-4536-BF3F-DF54C2F02EA8}\\");
const TCHAR* const kAppMachineClientStatePath =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{50DA5C89-FF97-4536-BF3F-DF54C2F02EA8}\\");
const TCHAR* const kApp2MachineClientsPath =
    _T("HKLM\\Software\\Google\\Update\\Clients\\")
    _T("{CB8E8A3C-7295-4529-B083-D5F76DCD4CC2}\\");

const TCHAR* const kAppUserClientsPath =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{50DA5C89-FF97-4536-BF3F-DF54C2F02EA8}\\");
const TCHAR* const kAppUserClientStatePath =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{50DA5C89-FF97-4536-BF3F-DF54C2F02EA8}\\");
const TCHAR* const kApp2UserClientsPath =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{CB8E8A3C-7295-4529-B083-D5F76DCD4CC2}\\");


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

class SetupFilesMockFailInstall : public SetupFiles {
 public:
  explicit SetupFilesMockFailInstall(bool is_machine)
      : SetupFiles(is_machine) {
  }

  virtual HRESULT Install() {
    return kExpetedError;
  }

  static const HRESULT kExpetedError = 0x86427531;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SetupFilesMockFailInstall);
};

}  // namespace

void CopyGoopdateFiles(const CString& omaha_path, const CString& version);

class SetupTest : public testing::Test {
 protected:

  typedef std::vector<uint32> Pids;

  static void SetUpTestCase() {
    CString exe_parent_dir = ConcatenatePath(
                                 app_util::GetCurrentModuleDirectory(),
                                 _T("unittest_support\\"));
    omaha_exe_path_ = ConcatenatePath(exe_parent_dir,
                                      _T("omaha_1.2.x\\GoogleUpdate.exe"));
    omaha_10_exe_path_ = ConcatenatePath(exe_parent_dir,
                                         _T("omaha_1.0.x\\GoogleUpdate.exe"));
    omaha_11_exe_path_ = ConcatenatePath(exe_parent_dir,
                                         _T("omaha_1.1.x\\GoogleUpdate.exe"));
    not_listening_exe_path_ = ConcatenatePath(
                                  exe_parent_dir,
                                  _T("does_not_shutdown\\GoogleUpdate.exe"));
  }

  explicit SetupTest(bool is_machine)
      : is_machine_(is_machine),
        omaha_path_(GetGoogleUpdateUserPath()) {
  }

  void SetUp() {
    ASSERT_SUCCEEDED(CreateDir(omaha_path_, NULL));
    setup_.reset(new omaha::Setup(is_machine_, &args_));
  }

  bool ShouldInstall() {
    SetupFiles setup_files(is_machine_);
    setup_files.Init();
    return setup_->ShouldInstall(&setup_files);
  }

  HRESULT StopGoogleUpdateAndWait() {
    return setup_->StopGoogleUpdateAndWait();
  }

  HRESULT LaunchInstalledWorker(bool do_setup_phase_2, HANDLE* process) {
    return setup_->LaunchInstalledWorker(do_setup_phase_2, process);
  }

  HRESULT TerminateCoreProcesses() const {
    return setup_->TerminateCoreProcesses();
  }

  bool InitLegacySetupLocks(GLock* lock10,
                            GLock* lock11_user,
                            GLock* lock11_machine) {
    return setup_->InitLegacySetupLocks(lock10, lock11_user, lock11_machine);
  }

  void PersistUpdateErrorInfo(bool is_machine,
                              HRESULT error,
                              int extra_code1,
                              const CString& version) {
    setup_->PersistUpdateErrorInfo(is_machine, error, extra_code1, version);
  }

  static bool HasXmlParser() { return Setup::HasXmlParser(); }

  bool launched_offline_worker() { return setup_->launched_offline_worker_; }

  void SetModeInstall() { setup_->mode_ = Setup::MODE_INSTALL; }
  void SetModeSelfInstall() { setup_->mode_ = Setup::MODE_SELF_INSTALL; }
  void SetModeSelfUpdate() { setup_->mode_ = Setup::MODE_SELF_UPDATE; }

  // Uses DoInstall() instead of Install() as necessary to avoid the elevation
  // check in Install().
  HRESULT TestInstall() {
    if (!is_machine_ || vista_util::IsUserAdmin()) {
      return setup_->Install(_T("foo"));
    } else {
      setup_->mode_ = Setup::MODE_INSTALL;
      return setup_->DoInstall();
    }
  }

  // Acquires the Setup Lock in another thread then calls TestInstall().
  void TestInstallWhileHoldingLock() {
    HoldLock hold_lock(is_machine_);

    Thread thread;
    thread.Start(&hold_lock);
    hold_lock.WaitForLockToBeAcquired();

    EXPECT_EQ(GOOPDATE_E_FAILED_TO_GET_LOCK, TestInstall());

    hold_lock.Stop();
    thread.WaitTillExit(1000);
  }

  // Acquires the Setup Lock in another thread then calls DoInstall().
  // Useful for testing modes other than MODE_INSTALL.
  void TestDoInstallWhileHoldingLock() {
    HoldLock hold_lock(is_machine_);

    Thread thread;
    thread.Start(&hold_lock);
    hold_lock.WaitForLockToBeAcquired();

    EXPECT_EQ(GOOPDATE_E_FAILED_TO_GET_LOCK, setup_->DoInstall());

    hold_lock.Stop();
    thread.WaitTillExit(1000);
  }

  void TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles() {
    SetupFilesMockFailInstall setup_files_mock(is_machine_);

    EXPECT_EQ(SetupFilesMockFailInstall::kExpetedError,
              setup_->DoProtectedGoogleUpdateInstall(&setup_files_mock));
  }

  void StopGoogleUpdateAndWaitSucceedsTestHelper(bool use_job_objects_only) {
    if (is_machine_ && !vista_util::IsUserAdmin()) {
      std::wcout << _T("\tTest did not run because the user is not an admin.")
                 << std::endl;
      return;
    }

    // This test has been the most problematic, so do not run on build systems.
    if (!ShouldRunLargeTest() || IsBuildSystem()) {
      return;
    }

    scoped_process core_process;
    scoped_process omaha_1_0_process;
    scoped_process omaha_1_1_process;
    scoped_process install_process;
    scoped_process opposite_process;
    scoped_process user_handoff_process;
    scoped_process user_install_goopdate_process;
    scoped_process user_install_needsadmin_process;
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

    StartCoreProcessesToShutdown(address(core_process),
                                 address(omaha_1_0_process),
                                 address(omaha_1_1_process));
    EXPECT_TRUE(core_process);
    EXPECT_TRUE(omaha_1_0_process);
    EXPECT_TRUE(omaha_1_1_process);

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
                  _T("/install"),
                  is_machine_,
                  address(install_process));
    EXPECT_TRUE(install_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(install_process), 0));

    if (vista_util::IsUserAdmin()) {
      // Other users are usually ignored - use always ignored command line.
      LaunchProcess(not_listening_exe_path_,
                    _T(""),
                    !is_machine_,
                    address(opposite_process));
      EXPECT_TRUE(opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(opposite_process), 0));
    } else {
      EXPECT_FALSE(is_machine_)
          << _T("Unexpected call for for machine when non-admin.");
      // We can't launch a system process when non-admin.
      std::wcout << _T("\tPart of this test did not run because the user is ")
                    _T("not an admin.") << std::endl;
    }

    CString same_needsadmin = is_machine_ ? _T("\"needsadmin=True\"") :
                                            _T("\"needsadmin=False\"");
    CString opposite_needsadmin = is_machine_ ? _T("\"needsadmin=False\"") :
                                                _T("\"needsadmin=True\"");
    // Machine setup looks for users running /handoff and /ig with
    // needsadmin=True and user setup ignores /handoff and /ig with
    // needsadmin=True.
    // Launching with needsadmin=<opposite> tests that machine ignores
    // needsadmin=False and user ignores needsadmin=True.
    LaunchProcess(not_listening_exe_path_,
                  _T("/handoff ") + opposite_needsadmin,
                  false,  // As the user.
                  address(user_handoff_process));
    EXPECT_TRUE(user_handoff_process);
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForSingleObject(get(user_handoff_process), 0));

    LaunchProcess(not_listening_exe_path_,
                  _T("/ig ") + opposite_needsadmin,
                  false,  // As the user.
                  address(user_install_goopdate_process));
    EXPECT_TRUE(user_install_goopdate_process);
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForSingleObject(get(user_install_goopdate_process), 0));

    // This process should be ignored even though it has needsadmin=<same>.
    LaunchProcess(not_listening_exe_path_,
                  _T("/install ") + same_needsadmin,
                  false,  // As the user.
                  address(user_install_needsadmin_process));
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
      EXPECT_TRUE(setup_phase1_job_process);
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
      EXPECT_TRUE(setup_phase1_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(setup_phase1_job_opposite_process),
                                      0));

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kAppInstallJobObject,
                       address(install_job_opposite_process),
                       address(install_job_opposite));
      EXPECT_TRUE(install_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(install_job_opposite_process), 0));

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kSilentJobObject,
                       address(silent_job_opposite_process),
                       address(silent_job_opposite));
      EXPECT_TRUE(install_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(install_job_opposite_process), 0));

      LaunchJobProcess(!is_machine_,
                       !is_machine_,
                       kSilentDoNotKillJobObject,
                       address(silent_do_not_kill_job_opposite_process),
                       address(silent_do_not_kill_job_opposite));
      EXPECT_TRUE(install_job_opposite_process);
      EXPECT_EQ(WAIT_TIMEOUT,
                ::WaitForSingleObject(get(install_job_opposite_process), 0));
    }

    EXPECT_SUCCEEDED(StopGoogleUpdateAndWait());
    EXPECT_EQ(0, setup_->extra_code1());

    // Verify all real processes exited and terminate all the ones that aren't
    // listening to shutdown.
    HANDLE processes[] = {get(core_process),
                          get(omaha_1_0_process),
                          get(omaha_1_1_process)};
    if (use_job_objects_only) {
      // Since we only wait for processes in the Job Objects, legacy processes
      // may not have stopped by the time StopGoogleUpdateAndWait() returns.
      EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(core_process), 0));

      // Now wait for the legacy processes to exit to avoid interfering with
      // other tests.
      EXPECT_EQ(WAIT_OBJECT_0, ::WaitForMultipleObjects(arraysize(processes),
                                                        processes,
                                                        true,  // wait for all
                                                        8000));
    } else {
      EXPECT_EQ(WAIT_OBJECT_0, ::WaitForMultipleObjects(arraysize(processes),
                                                        processes,
                                                        true,  // wait for all
                                                        0));
    }

    // Terminate all the processes and wait for them to exit to avoid
    // interfering with other tests.
    std::vector<HANDLE> started_processes;
    started_processes.push_back(get(install_process));
    if (vista_util::IsUserAdmin()) {
      started_processes.push_back(get(opposite_process));
    }
    started_processes.push_back(get(user_handoff_process));
    started_processes.push_back(get(user_install_goopdate_process));
    started_processes.push_back(get(user_install_needsadmin_process));
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
      EXPECT_TRUE(::TerminateProcess(started_processes[i], 1));
    }
    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForMultipleObjects(started_processes.size(),
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
      EXPECT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &user_sid_to_use));
    }

    DWORD flags = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                  EXCLUDE_CURRENT_PROCESS |
                  INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;

    std::vector<CString> command_lines;
    command_lines.push_back(_T("/c"));

    return Process::FindProcesses(flags,
                                  _T("GoogleUpdate.exe"),
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
  void StartCoreProcessesToShutdown(HANDLE* core_process,
                                    HANDLE* omaha_1_0_process,
                                    HANDLE* omaha_1_1_process) {
    ASSERT_TRUE(core_process);
    ASSERT_TRUE(omaha_1_0_process);
    ASSERT_TRUE(omaha_1_1_process);

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

    // Start the legacy versions.
    LaunchProcess(omaha_10_exe_path_, _T(""), is_machine_, omaha_1_0_process);
    LaunchProcess(omaha_11_exe_path_, _T(""), is_machine_, omaha_1_1_process);

    HANDLE processes[] = {*core_process,
                          *omaha_1_0_process,
                          *omaha_1_1_process};
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForMultipleObjects(arraysize(processes),
                                                     processes,
                                                     true,  // wait for all
                                                     0));
  }

  // Launches an instance of GoogleUpdate.exe that doesn't exit.
  void StopGoogleUpdateAndWaitProcessesDoNotStopTest() {
    LaunchProcessAndExpectStopGoogleUpdateAndWaitTimesOut(is_machine_,
                                                   _T(""),
                                                   COMMANDLINE_MODE_NOARGS);
  }

  void LaunchProcessAndExpectStopGoogleUpdateAndWaitTimesOut(
      bool is_machine_process,
      const CString& args,
      CommandLineMode expected_running_process_mode) {
    ASSERT_TRUE(args);

    if (is_machine_ && !vista_util::IsUserAdmin()) {
      std::wcout << _T("\tTest did not run because the user is not an admin.")
                 << std::endl;
      return;
    }

    if (!ShouldRunLargeTest()) {
      return;
    }
    scoped_process process;
    LaunchProcess(not_listening_exe_path_,
                  args,
                  is_machine_process,
                  address(process));
    EXPECT_TRUE(process);
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

    EXPECT_EQ(GOOPDATE_E_INSTANCES_RUNNING, StopGoogleUpdateAndWait());
    EXPECT_EQ(expected_running_process_mode, setup_->extra_code1());
    // Make sure the process is still running to help debug failures.
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(process), 0));


    EXPECT_TRUE(::TerminateProcess(get(process), 1));
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(get(process), kProcessesCleanupWait));
  }

  // If start using Job Objects again, make sure the test only uses Job Objects.
  void LaunchProcessInJobAndExpectStopGoogleUpdateAndWaitTimesOut(
      const CString& job_base_name,
      bool machine_as_system) {
    ASSERT_TRUE(is_machine_ || !machine_as_system);

    if (!ShouldRunLargeTest()) {
      return;
    }


    scoped_process process;
    scoped_job job;

    LaunchJobProcess(is_machine_,
                     machine_as_system,
                     job_base_name,
                     address(process),
                     address(job));
    EXPECT_TRUE(process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(process), 0));

    EXPECT_EQ(GOOPDATE_E_INSTANCES_RUNNING, StopGoogleUpdateAndWait());
    EXPECT_EQ(COMMANDLINE_MODE_NOARGS, setup_->extra_code1());

    EXPECT_TRUE(::TerminateProcess(get(process), 1));
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
    EXPECT_TRUE(core_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(core_process), 0));
    LaunchProcess(not_listening_exe_path_,
                  _T("/c"),
                  !is_machine_,
                  address(opposite_core_process));
    EXPECT_TRUE(opposite_core_process);
    EXPECT_EQ(WAIT_TIMEOUT,
              ::WaitForSingleObject(get(opposite_core_process), 0));
    LaunchProcess(not_listening_exe_path_,
                  _T("/cr"),
                  is_machine_,
                  address(codered_process));
    EXPECT_TRUE(codered_process);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(codered_process), 0));
    LaunchProcess(not_listening_exe_path_,
                  _T(""),
                  is_machine_,
                  address(noargs_process));
    EXPECT_TRUE(noargs_process);
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

  // Sets the legacy setup locks then attempts to run setup with the legacy
  // versions. When the lock is not held and a newer version is installed,
  // the legacy executables do a handoff install and exit. Since we have
  // acquired the locks, the legacy executables should wait and not exit.
  // Check both after a second.
  // If Omaha 1.2.x is not found, a fake version is installed to prevent the
  // legacy installers from installing.
  // When is_machine_ is true, the legacy installers will start the Service.
  void AcquireLegacySetupLocksTest() {
    if (is_machine_ && !vista_util::IsUserAdmin()) {
      std::wcout << _T("\tTest did not run because the user is not an admin.")
                 << std::endl;
      return;
    }

    if (!ShouldRunLargeTest()) {
      return;
    }

    CString omaha_clients_key_name = is_machine_ ?
                                     MACHINE_REG_CLIENTS_GOOPDATE :
                                     USER_REG_CLIENTS_GOOPDATE;
    CString version;
    if (FAILED(RegKey::GetValue(omaha_clients_key_name,
                                _T("pv"),
                                &version)) ||
        (_T("1.2.") != version.Left(4))) {
      // Fool the legacy instances into thinking a newer version is installed
      // so that they don't try to install when we release the locks.
      // Do not worry about restoring it because there is only a potential
      // problem when running a legacy metainstaller after this test.
      version = _T("1.2.0.654");
      EXPECT_SUCCEEDED(RegKey::SetValue(omaha_clients_key_name,
                                        _T("pv"),
                                        version));
    }

    // Legacy installers check for the files and over-install if they are not
    // found, so we copy files to the version directory.
    CString install_dir = ConcatenatePath(
        is_machine_ ?
        ConfigManager::Instance()->GetMachineGoopdateInstallDir() :
        ConfigManager::Instance()->GetUserGoopdateInstallDir(),
        version);
    EXPECT_SUCCEEDED(File::CopyTree(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        _T("unittest_support\\omaha_1.2.x\\")),
        install_dir,
        false));

    // This block of code reproduces what we do in Setup::DoInstall().
    GLock lock10, lock11_user, lock11_machine;
    EXPECT_TRUE(InitLegacySetupLocks(&lock10, &lock11_user, &lock11_machine));
    EXPECT_TRUE(lock10.Lock(500));
    EXPECT_TRUE(lock11_user.Lock(500));
    if (is_machine_) {
      EXPECT_TRUE(lock11_machine.Lock(500));
    }

    // Start the legacy processes, and verify that they are blocked.
    scoped_process omaha_1_0_process;
    scoped_process omaha_1_1_process;
    scoped_process omaha_1_0_process_as_system;
    scoped_process omaha_1_1_process_as_system;
    LaunchProcess(omaha_10_exe_path_,
                  _T("/install"),
                  false,  // As the user.
                  address(omaha_1_0_process));
    LaunchProcess(omaha_11_exe_path_,
                  _T("/update"),
                  false,  // As the user.
                  address(omaha_1_1_process));
    if (is_machine_) {
      // For the machine case, tests the legacy process running both as Local
      // System and as the user.
      LaunchProcess(omaha_10_exe_path_,
                    _T("/install"),
                    true,  // As Local System.
                    address(omaha_1_0_process_as_system));
      LaunchProcess(omaha_11_exe_path_,
                    _T("/update"),
                    true,  // As Local System.
                    address(omaha_1_1_process_as_system));
    }


    HANDLE processes[] = {get(omaha_1_0_process),
                          get(omaha_1_1_process),
                          get(omaha_1_0_process_as_system),
                          get(omaha_1_1_process_as_system)};
    int num_processes = arraysize(processes) - (is_machine_ ? 0 : 2);
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForMultipleObjects(num_processes,
                                                     processes,
                                                     false,  // wait for any
                                                     1000));

    // Release the locks and the legacy processes should exit
    ASSERT_TRUE(lock10.Unlock());
    ASSERT_TRUE(lock11_user.Unlock());
    if (is_machine_) {
      ASSERT_TRUE(lock11_machine.Unlock());
    }

    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForMultipleObjects(num_processes,
                                                      processes,
                                                      true,  // wait for all
                                                      kProcessesCleanupWait));
  }

  // Starts dummy process that doesn't exit and assigns it to the specified job.
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
    LaunchProcess(not_listening_exe_path_,
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

  const bool is_machine_;
  const CString omaha_path_;
  scoped_ptr<omaha::Setup> setup_;
  CommandLineArgs args_;

  static CString omaha_exe_path_;
  static CString omaha_10_exe_path_;
  static CString omaha_11_exe_path_;
  static CString not_listening_exe_path_;
};

CString SetupTest::omaha_exe_path_;
CString SetupTest::omaha_10_exe_path_;
CString SetupTest::omaha_11_exe_path_;
CString SetupTest::not_listening_exe_path_;

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
    args_.extra_args_str = _T("\"appname=foo&lang=en\"");
    CommandLineAppArgs app_args;
    app_args.app_guid = StringToGuid(kAppGuid_);
    args_.extra.apps.push_back(app_args);
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
    ASSERT_FALSE(File::IsDirectory(future_version_path_));

    ASSERT_SUCCEEDED(CreateDir(future_version_path_, NULL));

    ASSERT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        _T("GoogleUpdate.exe")),
        omaha_path_ + _T("GoogleUpdate.exe"),
        false));

    ASSERT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        _T("goopdate.dll")),
        ConcatenatePath(future_version_path_, _T("goopdate.dll")),
        false));
    ASSERT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        _T("goopdateres_en.dll")),
        ConcatenatePath(future_version_path_, _T("goopdateres_en.dll")),
        false));

    ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
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
  SetupRegistryProtectedUserTest()
      : SetupUserTest(),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  static void SetUpTestCase() {
    this_version_ = GetVersionString();
    expected_is_overinstall_ = !OFFICIAL_BUILD;
#ifdef DEBUG
    if (RegKey::HasValue(MACHINE_REG_UPDATE_DEV, kRegValueNameOverInstall)) {
      DWORD value = 0;
      EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                        kRegValueNameOverInstall,
                                        &value));
      expected_is_overinstall_ = value != 0;
    }
#endif
  }

  virtual void SetUp() {
    SetupUserTest::SetUp();

    RegKey::DeleteKey(hive_override_key_name_, true);
    // Do not override HKLM because it contains the CSIDL_* definitions.
    OverrideSpecifiedRegistryHives(hive_override_key_name_, false, true);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));

    SetupUserTest::TearDown();
  }

  const CString hive_override_key_name_;

  static CString this_version_;
  static bool expected_is_overinstall_;
};

CString SetupRegistryProtectedUserTest::this_version_;
bool SetupRegistryProtectedUserTest::expected_is_overinstall_;

class SetupRegistryProtectedMachineTest : public SetupMachineTest {
 protected:
  SetupRegistryProtectedMachineTest()
      : SetupMachineTest(),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  virtual void SetUp() {
    SetupMachineTest::SetUp();

    // Prime the cache of CSIDL_PROGRAM_FILES so it is available even after
    // we override HKLM.
    CString program_files_path;
    EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));

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

class SetupOfflineInstallerTest : public testing::Test {
 protected:
  static bool CallCopyOfflineFiles(const CommandLineArgs& args,
                                   const CString& target_location) {
    omaha::Setup setup(false, &args);
    return setup.CopyOfflineFiles(target_location);
  }

  static HRESULT CallCopyOfflineFilesForGuid(const CString& app_guid,
                                             const CString& target_location) {
    return omaha::Setup::CopyOfflineFilesForGuid(app_guid, target_location);
  }
};


TEST_F(SetupFutureVersionInstalledUserTest, Install_HandoffWithShellMissing) {
  CString shell_path = ConcatenatePath(omaha_path_, _T("GoogleUpdate.exe"));
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  scoped_event ui_displayed_event;
  ASSERT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kUiDisplayedEventEnvironmentVariableName,
      is_machine_,
      address(ui_displayed_event)));

  EXPECT_EQ(GOOPDATE_E_HANDOFF_FAILED, setup_->Install(_T("/foo")));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), setup_->extra_code1());
}

TEST_F(SetupFutureVersionInstalledUserTest,
       Install_HandoffWithGoopdateDllMissing) {
  CString dll_path = ConcatenatePath(future_version_path_, _T("goopdate.dll"));
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  scoped_event ui_displayed_event;
  ASSERT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kUiDisplayedEventEnvironmentVariableName,
      is_machine_,
      address(ui_displayed_event)));

  EXPECT_EQ(GOOPDATE_E_HANDOFF_FAILED, setup_->Install(_T("/foo")));
  EXPECT_EQ(GOOGLEUPDATE_E_DLL_NOT_FOUND, setup_->extra_code1());
}

// The setup would cause a failure but we shouldn't see it because the event is
// already signaled.
// There is a race condition, especially on build machines, where the process
// may have exited by the time we check for it and the event. Thus, there are
// two expected cases.
TEST_F(SetupFutureVersionInstalledUserTest,
       Install_HandoffWithEventSignaledBeforeExit) {
  CString dll_path = ConcatenatePath(future_version_path_, _T("goopdate.dll"));
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  scoped_event ui_displayed_event;
  ASSERT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kUiDisplayedEventEnvironmentVariableName,
      is_machine_,
      address(ui_displayed_event)));
  ASSERT_TRUE(::SetEvent(get(ui_displayed_event)));

  HRESULT hr = setup_->Install(_T("/foo"));
  EXPECT_TRUE(SUCCEEDED(hr) || GOOPDATE_E_HANDOFF_FAILED == hr);
  if (SUCCEEDED(hr)) {
    EXPECT_EQ(0, setup_->extra_code1());

    // There is a timing issue where GoogleUpdate.exe may not look at pv in the
    // registry until after the existing_version_ has been restored to the
    // registry. This causes GoogleUpdate.exe to find goopdate.dll in the
    // existing_version_ and run with a UI. Wait so that this does not happen.
#if OFFICIAL_BUILD
    ::Sleep(8000);
#else
    ::Sleep(1000);
#endif
  } else {
    EXPECT_EQ(GOOGLEUPDATE_E_DLL_NOT_FOUND, setup_->extra_code1());
  }
}

// Silent installs ignore the event, so we should always get the exit code.
// Also, exit code is reported directly instead of GOOPDATE_E_HANDOFF_FAILED.
TEST_F(SetupFutureVersionInstalledUserTest,
       Install_SilentHandoffWithEventSignaledBeforeExit) {
  CString dll_path = ConcatenatePath(future_version_path_, _T("goopdate.dll"));
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  scoped_event ui_displayed_event;
  ASSERT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kUiDisplayedEventEnvironmentVariableName,
      is_machine_,
      address(ui_displayed_event)));
  ASSERT_TRUE(::SetEvent(get(ui_displayed_event)));

  args_.is_silent_set = true;
  EXPECT_EQ(GOOGLEUPDATE_E_DLL_NOT_FOUND, setup_->Install(_T("/foo")));
  EXPECT_EQ(0, setup_->extra_code1());
}

// Offline installs ignore the event regardless of whether they are silent, so
// we should always get the exit code.
// Also, exit code is reported directly instead of GOOPDATE_E_HANDOFF_FAILED.
TEST_F(SetupFutureVersionInstalledUserTest,
       Install_InteractiveOfflineHandoffWithEventSignaledBeforeExit) {
  CString dll_path = ConcatenatePath(future_version_path_, _T("goopdate.dll"));
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  scoped_event ui_displayed_event;
  ASSERT_SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kUiDisplayedEventEnvironmentVariableName,
      is_machine_,
      address(ui_displayed_event)));
  ASSERT_TRUE(::SetEvent(get(ui_displayed_event)));

  // Make Setup think this is an offline install.
  CString manifest_path;
  manifest_path.Format(_T("%s\\%s.gup"),
                           app_util::GetCurrentModuleDirectory(),
                           kAppGuid_);
  File manifest_file;
  ASSERT_SUCCEEDED(manifest_file.Open(manifest_path, true, false));
  ASSERT_SUCCEEDED(manifest_file.Touch());
  ASSERT_SUCCEEDED(manifest_file.Close());

  CString installer_path;
  installer_path.Format(_T("%s\\unittest_installer.exe.%s"),
                            app_util::GetCurrentModuleDirectory(),
                            kAppGuid_);
  File installer_file;
  EXPECT_SUCCEEDED(installer_file.Open(installer_path, true, false));
  EXPECT_SUCCEEDED(installer_file.Touch());
  EXPECT_SUCCEEDED(installer_file.Close());

  EXPECT_EQ(GOOGLEUPDATE_E_DLL_NOT_FOUND, setup_->Install(_T("/foo")));
  EXPECT_EQ(0, setup_->extra_code1());

  EXPECT_FALSE(args_.is_silent_set);  // Ensure not silent as voids this test.
  EXPECT_TRUE(launched_offline_worker());

  EXPECT_TRUE(::DeleteFile(manifest_path));
  EXPECT_TRUE(::DeleteFile(installer_path));
}

TEST_F(SetupFutureVersionInstalledUserTest,
       InstallSelfSilently_NoRunKey) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);

  // Write the future version to the override registry.
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  CString dll_path = ConcatenatePath(future_version_path_, _T("goopdate.dll"));
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            setup_->InstallSelfSilently());
  EXPECT_EQ(0, setup_->extra_code1());

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(SetupFutureVersionInstalledUserTest,
       InstallSelfSilently_ValidRunKey) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);

  // Write the future version to the override registry.
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  const TCHAR kRunKey[] =
      _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
  const CString shell_path = ConcatenatePath(GetGoogleUpdateUserPath(),
                                             _T("GoogleUpdate.exe"));
  CString run_value;
  run_value.Format(_T("\"%s\" /just_exit"), shell_path);
  ASSERT_SUCCEEDED(RegKey::SetValue(kRunKey, _T("Google Update"), run_value));

  CString dll_path = ConcatenatePath(future_version_path_, _T("goopdate.dll"));
  ASSERT_SUCCEEDED(File::Remove(dll_path));
  ASSERT_FALSE(File::Exists(dll_path));

  EXPECT_SUCCEEDED(setup_->InstallSelfSilently());
  EXPECT_EQ(0, setup_->extra_code1());

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(SetupUserTest, HasXmlParser) {
  EXPECT_TRUE(HasXmlParser());
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

TEST_F(SetupRegistryProtectedUserTest, Install_OEM) {
  // The test fixture only disables HKCU by default. Disable HKLM too.
  OverrideSpecifiedRegistryHives(hive_override_key_name_, true, true);

  if (vista_util::IsVistaOrLater()) {
    ASSERT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    ASSERT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  ASSERT_TRUE(ConfigManager::Instance()->IsWindowsInstalling());

  args_.is_oem_set = true;
  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            setup_->Install(_T("foo")));

  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, kRegValueOemInstallTimeSec));
  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

TEST_F(SetupRegistryProtectedMachineTest, Install_OemElevationRequired) {
  if (vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user IS an admin.")
               << std::endl;
    return;
  }

  if (vista_util::IsVistaOrLater()) {
    ASSERT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    ASSERT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  ASSERT_TRUE(ConfigManager::Instance()->IsWindowsInstalling());

  args_.is_oem_set = true;
  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            setup_->Install(_T("foo")));

  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

TEST_F(SetupRegistryProtectedMachineTest, Install_OemNotAuditMode) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  args_.is_oem_set = true;
  EXPECT_EQ(GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE,
            setup_->Install(_T("foo")));
  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));
}

// Prevents the install from continuing by holding the Setup Lock.
TEST_F(SetupRegistryProtectedMachineTest, Install_OemFails) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  if (vista_util::IsVistaOrLater()) {
    ASSERT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));

    // TODO(omaha): Overriding HKLM causes HasXmlParser() to fail on Vista,
    // preventing this test from running correctly.
    return;
  } else {
    ASSERT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }
  ASSERT_TRUE(ConfigManager::Instance()->IsWindowsInstalling());

  HoldLock hold_lock(is_machine_);
  Thread thread;
  thread.Start(&hold_lock);
  hold_lock.WaitForLockToBeAcquired();

  args_.is_oem_set = true;
  EXPECT_EQ(GOOPDATE_E_FAILED_TO_GET_LOCK, setup_->Install(_T("foo")));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueMachineId));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueUserId));
  EXPECT_TRUE(
      RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec));

  hold_lock.Stop();
  thread.WaitTillExit(1000);
}

TEST_F(SetupMachineTest, LaunchInstalledWorker_OemInstallNotOffline) {
  SetModeInstall();
  CommandLineAppArgs app1;
  args_.extra.apps.push_back(app1);
  args_.is_oem_set = true;
  EXPECT_EQ(GOOPDATE_E_OEM_WITH_ONLINE_INSTALLER,
            LaunchInstalledWorker(true, NULL));
}

TEST_F(SetupMachineTest, LaunchInstalledWorker_OemUpdateNotOffline) {
  SetModeSelfUpdate();
  args_.is_oem_set = true;
  ExpectAsserts expect_asserts;
  EXPECT_EQ(GOOPDATE_E_OEM_WITH_ONLINE_INSTALLER,
            LaunchInstalledWorker(true, NULL));
}

TEST_F(SetupMachineTest, LaunchInstalledWorker_EulaRequiredNotOffline) {
  SetModeInstall();
  CommandLineAppArgs app1;
  args_.extra.apps.push_back(app1);
  args_.is_eula_required_set = true;
  EXPECT_EQ(GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER,
            LaunchInstalledWorker(true, NULL));
}

TEST_F(SetupUserTest, LaunchInstalledWorker_EulaRequiredNotOffline) {
  SetModeInstall();
  CommandLineAppArgs app1;
  args_.extra.apps.push_back(app1);
  args_.is_eula_required_set = true;
  EXPECT_EQ(GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER,
            LaunchInstalledWorker(true, NULL));
}

//
// ShouldInstall tests.
//

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_NewerVersion) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    _T("1.0.3.4")));
  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_OlderVersion) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    _T("9.8.7.6")));
  EXPECT_FALSE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesMissingSameLanguage) {
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
                          _T("goopdate.dll"));
  ASSERT_FALSE(File::Exists(file_path));

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentSameLanguage) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_EQ(expected_is_overinstall_, ShouldInstall());

  if (ShouldRunLargeTest()) {
    // Override OverInstall to test official behavior on non-official builds.

    DWORD existing_overinstall(0);
    bool had_existing_overinstall = SUCCEEDED(RegKey::GetValue(
                                                  MACHINE_REG_UPDATE_DEV,
                                                  kRegValueNameOverInstall,
                                                  &existing_overinstall));

    EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueNameOverInstall,
                                      static_cast<DWORD>(0)));
#ifdef DEBUG
    EXPECT_FALSE(ShouldInstall());
#else
    EXPECT_EQ(expected_is_overinstall_, ShouldInstall());
#endif

    // Restore "overinstall"
    if (had_existing_overinstall) {
      EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                        kRegValueNameOverInstall,
                                        existing_overinstall));
    } else {
      EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                           kRegValueNameOverInstall));
    }
  }
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentSameLanguageMissingInstalledVer) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentSameLanguageNewerInstalledVer) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    kRegValueInstalledVersion));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentDifferentLanguage) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_EQ(expected_is_overinstall_, ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest,
       ShouldInstall_SameVersionFilesPresentNoLanguage) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_EQ(expected_is_overinstall_, ShouldInstall());
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
                                 _T("goopdate.dll"));
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
                                 _T("GoopdateBho.dll"));
  ASSERT_SUCCEEDED(File::Remove(path));
  ASSERT_FALSE(File::Exists(path));

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_SameVersionShellMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString shell_path = ConcatenatePath(omaha_path_, _T("GoogleUpdate.exe"));
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  EXPECT_TRUE(ShouldInstall());
}

TEST_F(SetupRegistryProtectedUserTest, ShouldInstall_NewerVersionShellMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueInstalledVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  CopyGoopdateFiles(omaha_path_, kFutureVersionString);
  CString shell_path = ConcatenatePath(omaha_path_, _T("GoogleUpdate.exe"));
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  EXPECT_FALSE(ShouldInstall());

  EXPECT_SUCCEEDED(
      DeleteDirectory(ConcatenatePath(omaha_path_, kFutureVersionString)));
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
// The tests also start 1.0.x and 1.1.x legacy instances.
// The Succeeds tests will fail if any processes that don't listen to the
// shutdown event are running.
//

TEST_F(SetupUserTest, StopGoogleUpdateAndWait_Succeeds) {
  StopGoogleUpdateAndWaitSucceedsTest();
}

TEST_F(SetupMachineTest, StopGoogleUpdateAndWait_Succeeds) {
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
  LaunchProcessAndExpectStopGoogleUpdateAndWaitTimesOut(
      false,
      _T("/handoff \"needsadmin=True\""),
      COMMANDLINE_MODE_HANDOFF_INSTALL);
}

TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_MachineInstallGoogleUpdateWorkerRunningAsUser) {
  LaunchProcessAndExpectStopGoogleUpdateAndWaitTimesOut(
      false,
      _T("/ig \"needsadmin=True\""),
      COMMANDLINE_MODE_IG);
}

TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_UserHandoffWorkerRunningAsSystem) {
  LaunchProcessAndExpectStopGoogleUpdateAndWaitTimesOut(
      true,
      _T("/handoff \"needsadmin=False\""),
      COMMANDLINE_MODE_HANDOFF_INSTALL);
}

TEST_F(SetupMachineTest,
       StopGoogleUpdateAndWait_UserInstallGoogleUpdateWorkerRunningAsSystem) {
  LaunchProcessAndExpectStopGoogleUpdateAndWaitTimesOut(
      true,
      _T("/ig \"needsadmin=False\""),
      COMMANDLINE_MODE_IG);
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

TEST_F(SetupUserTest, AcquireLegacySetupLocks_LegacyExesWait) {
  AcquireLegacySetupLocksTest();
}

TEST_F(SetupMachineTest, AcquireLegacySetupLocks_LegacyExesWait) {
  AcquireLegacySetupLocksTest();
}

TEST_F(SetupRegistryProtectedUserTest, PersistUpdateErrorInfo) {
  PersistUpdateErrorInfo(is_machine_, 0x98765432, 77, _T("1.2.3.4"));

  DWORD value(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    &value));
  EXPECT_EQ(0x98765432, value);

  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    &value));
  EXPECT_EQ(77, value);

  CString version;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    &version));
  EXPECT_FALSE(version.IsEmpty());
  EXPECT_STREQ(_T("1.2.3.4"), version);
}

TEST_F(SetupRegistryProtectedMachineTest, PersistUpdateErrorInfo) {
  PersistUpdateErrorInfo(is_machine_, 0x98765430, 0x12345678, _T("2.3.4.5"));

  DWORD value(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    &value));
  EXPECT_EQ(0x98765430, value);

  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    &value));
  EXPECT_EQ(0x12345678, value);

  CString version;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    &version));
  EXPECT_FALSE(version.IsEmpty());
  EXPECT_STREQ(_T("2.3.4.5"), version);
}

TEST_F(SetupRegistryProtectedUserTest,
       ReadAndClearUpdateErrorInfo_KeyDoesNotExist) {
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(Setup::ReadAndClearUpdateErrorInfo(false,
                                                  &self_update_error_code,
                                                  &self_update_extra_code1,
                                                  &self_update_version));
}

TEST_F(SetupRegistryProtectedMachineTest,
       ReadAndClearUpdateErrorInfo_KeyDoesNotExist) {
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(Setup::ReadAndClearUpdateErrorInfo(true,
                                                  &self_update_error_code,
                                                  &self_update_extra_code1,
                                                  &self_update_version));
}

TEST_F(SetupRegistryProtectedUserTest,
       ReadAndClearUpdateErrorInfo_UpdateErrorCodeDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_FALSE(Setup::ReadAndClearUpdateErrorInfo(false,
                                                  &self_update_error_code,
                                                  &self_update_extra_code1,
                                                  &self_update_version));
}

TEST_F(SetupRegistryProtectedMachineTest,
       ReadAndClearUpdateErrorInfo_UpdateErrorCodeDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_FALSE(Setup::ReadAndClearUpdateErrorInfo(true,
                                                  &self_update_error_code,
                                                  &self_update_extra_code1,
                                                  &self_update_version));
}

TEST_F(SetupRegistryProtectedUserTest,
       ReadAndClearUpdateErrorInfo_AllValuesPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_TRUE(Setup::ReadAndClearUpdateErrorInfo(false,
                                                 &self_update_error_code,
                                                 &self_update_extra_code1,
                                                 &self_update_version));

  EXPECT_EQ(0x87654321, self_update_error_code);
  EXPECT_EQ(55, self_update_extra_code1);
  EXPECT_STREQ(_T("0.2.4.8"), self_update_version);
}

TEST_F(SetupRegistryProtectedMachineTest,
       ReadAndClearUpdateErrorInfo_AllValuesPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_TRUE(Setup::ReadAndClearUpdateErrorInfo(true,
                                                 &self_update_error_code,
                                                 &self_update_extra_code1,
                                                 &self_update_version));

  EXPECT_EQ(0x87654321, self_update_error_code);
  EXPECT_EQ(55, self_update_extra_code1);
  EXPECT_STREQ(_T("0.2.4.8"), self_update_version);
}

TEST_F(SetupRegistryProtectedUserTest,
       ReadAndClearUpdateErrorInfo_ValuesPresentInMachineOnly) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(Setup::ReadAndClearUpdateErrorInfo(false,
                                                  &self_update_error_code,
                                                  &self_update_extra_code1,
                                                  &self_update_version));
  // Clean up HKLM, which isn't overridden.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                       kRegValueSelfUpdateErrorCode));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                       kRegValueSelfUpdateExtraCode1));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                       kRegValueSelfUpdateVersion));
}

TEST_F(SetupOfflineInstallerTest, ValidOfflineInstaller) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");

  CString offline_manifest_path(guid_string);
  offline_manifest_path += _T(".gup");
  offline_manifest_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          offline_manifest_path);
  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("server_manifest_one_app.xml")),
      offline_manifest_path,
      false));

  CString installer_exe = _T("foo_installer.exe");
  CString tarred_installer_path;
  tarred_installer_path.Format(_T("%s.%s"), installer_exe, guid_string);
  tarred_installer_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          tarred_installer_path);

  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("GoogleUpdate.exe")),
      tarred_installer_path,
      false));

  CommandLineArgs args;
  CommandLineAppArgs app1;
  app1.app_guid = StringToGuid(guid_string);
  args.extra.apps.push_back(app1);
  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  ASSERT_TRUE(CallCopyOfflineFiles(args, target_location));

  CString target_manifest = ConcatenatePath(target_location,
                                            guid_string + _T(".gup"));
  EXPECT_TRUE(File::Exists(target_manifest));
  CString target_file = ConcatenatePath(
      ConcatenatePath(target_location, guid_string), installer_exe);
  EXPECT_TRUE(File::Exists(target_file));

  EXPECT_SUCCEEDED(DeleteDirectory(target_location));
  EXPECT_SUCCEEDED(File::Remove(tarred_installer_path));
  EXPECT_SUCCEEDED(File::Remove(offline_manifest_path));
}

TEST_F(SetupOfflineInstallerTest, NoOfflineInstaller) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");
  CommandLineArgs args;
  CommandLineAppArgs app1;
  app1.app_guid = StringToGuid(guid_string);
  args.extra.apps.push_back(app1);
  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  EXPECT_FALSE(CallCopyOfflineFiles(args, target_location));
}

TEST_F(SetupOfflineInstallerTest, ValidCopyOfflineFilesForGuid) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");

  CString offline_manifest_path(guid_string);
  offline_manifest_path += _T(".gup");
  offline_manifest_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          offline_manifest_path);
  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("server_manifest_one_app.xml")),
      offline_manifest_path,
      false));

  CString installer_exe = _T("foo_installer.exe");
  CString tarred_installer_path;
  tarred_installer_path.Format(_T("%s.%s"), installer_exe, guid_string);
  tarred_installer_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          tarred_installer_path);

  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("GoogleUpdate.exe")),
      tarred_installer_path,
      false));

  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  ASSERT_SUCCEEDED(CallCopyOfflineFilesForGuid(guid_string, target_location));

  CString target_manifest = ConcatenatePath(target_location,
                                            guid_string + _T(".gup"));
  EXPECT_TRUE(File::Exists(target_manifest));
  CString target_file = ConcatenatePath(
      ConcatenatePath(target_location, guid_string), installer_exe);
  EXPECT_TRUE(File::Exists(target_file));

  EXPECT_SUCCEEDED(DeleteDirectory(target_location));
  EXPECT_SUCCEEDED(File::Remove(tarred_installer_path));
  EXPECT_SUCCEEDED(File::Remove(offline_manifest_path));
}

TEST_F(SetupOfflineInstallerTest, NoCopyOfflineFilesForGuid) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");
  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  ASSERT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            CallCopyOfflineFilesForGuid(guid_string, target_location));
}

// A few tests for the public method. The bulk of the EULA cases are covered by
// Install() and DoProtectedGoogleUpdateInstall() tests.
TEST_F(SetupRegistryProtectedMachineTest, SetEulaAccepted_KeyDoesNotExist) {
  EXPECT_EQ(S_OK, Setup::SetEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest, SetEulaAccepted_ValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  EXPECT_EQ(S_FALSE, Setup::SetEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest, SetEulaAccepted_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(Setup::SetEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest, SetEulaAccepted_KeyDoesNotExist) {
  EXPECT_EQ(S_OK, Setup::SetEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest, SetEulaAccepted_ValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  EXPECT_EQ(S_FALSE, Setup::SetEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest, SetEulaAccepted_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(Setup::SetEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_UpdateKeyDoesNotExist) {
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueDoesNotExistAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaNotRequired_EulaValueExistsZeroAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       DoInstall_SelfInstall_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeSelfInstall();
  args_.is_eula_required_set = false;
  TestDoInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

// Self-update does not clear eulaaccepted. This case should never happen.
TEST_F(SetupRegistryProtectedMachineTest,
       DoInstall_SelfUpdate_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeSelfUpdate();
  args_.is_eula_required_set = false;
  TestDoInstallWhileHoldingLock();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// EULA required is not handled by DoInstall(). It is handled later by
// DoProtectedGoogleUpdateInstall().
TEST_F(SetupRegistryProtectedMachineTest,
       Install_EulaRequired_EulaValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  args_.is_eula_required_set = true;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_UpdateKeyDoesNotExist) {
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);

  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);

  // ClientState for Google Update (never used) and other apps is not affected.
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

// The existing value is ignored if there are not two registered apps. This is
// an artifact of the implementation and not a requirement.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// One app is not sufficient for detecting that Google Update is already
// installed.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueDoesNotExistOneAppRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Even Google Update registered is not sufficient for detecting that Google
// Update is already installed.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueDoesNotExistGoogleUpdateRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Important: The existing state is not changed because two apps are registered.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueDoesNotExistTwoAppsRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

// The existing state is not changed because Google Update is already
// installed, but there is no way to differentiate this from writing 0.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsZeroTwoAppsRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// The existing state is not changed because Google Update is already installed.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsOneTwoAppsRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_SelfInstall_EulaRequired_UpdateKeyDoesNotExist) {  // NOLINT
  SetModeSelfInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Self-update does not set eulaaccepted. This case should never happen.
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_SelfUpdate_EulaRequired_UpdateKeyDoesNotExist) {  // NOLINT
  SetModeSelfUpdate();
  args_.is_eula_required_set = true;
  ExpectAsserts expect_asserts;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

// EULA not required is not handled by DoProtectedGoogleUpdateInstall(). It
// would have already been handled by DoInstall().
TEST_F(SetupRegistryProtectedMachineTest,
       DoProtectedGoogleUpdateInstall_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = false;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_UpdateKeyDoesNotExist) {
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(USER_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueDoesNotExistAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaNotRequired_EulaValueExistsZeroAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  args_.is_eula_required_set = false;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       DoInstall_SelfInstall_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeSelfInstall();
  args_.is_eula_required_set = false;
  TestDoInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

// Self-update does not clear eulaaccepted. This case should never happen.
TEST_F(SetupRegistryProtectedUserTest,
       DoInstall_SelfUpdate_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeSelfUpdate();
  args_.is_eula_required_set = false;
  TestDoInstallWhileHoldingLock();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// EULA required is not handled by DoInstall(). It is handled later by
// DoProtectedGoogleUpdateInstall().
TEST_F(SetupRegistryProtectedUserTest,
       Install_EulaRequired_EulaValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  args_.is_eula_required_set = true;
  TestInstallWhileHoldingLock();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_UpdateKeyDoesNotExist) {
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);

  // ClientState for Google Update (never used) and other apps is not affected.
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

// The existing value is ignored if there are not two registered apps. This is
// an artifact of the implementation and not a requirement.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(USER_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// One app is not sufficient for detecting that Google Update is already
// installed.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueDoesNotExistOneAppRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Even Google Update registered is not sufficient for detecting that Google
// Update is already installed.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueDoesNotExistGoogleUpdateRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Important: The existing state is not changed because two apps are registered.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueDoesNotExistTwoAppsRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

// The existing state is not changed because Google Update is already
// installed, but there is no way to differentiate this from writing 0.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsZeroTwoAppsRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// The existing state is not changed because Google Update is already installed.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaRequired_EulaValueExistsOneTwoAppsRegistered) {  // NOLINT
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_SelfInstall_EulaRequired_UpdateKeyDoesNotExist) {  // NOLINT
  SetModeSelfInstall();
  args_.is_eula_required_set = true;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Self-update does not set eulaaccepted. This case should never happen.
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_SelfUpdate_EulaRequired_UpdateKeyDoesNotExist) {  // NOLINT
  SetModeSelfUpdate();
  args_.is_eula_required_set = true;
  ExpectAsserts expect_asserts;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

// EULA not required is not handled by DoProtectedGoogleUpdateInstall(). It
// would have already been handled by DoInstall().
TEST_F(SetupRegistryProtectedUserTest,
       DoProtectedGoogleUpdateInstall_EulaNotRequired_EulaValueExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = false;
  TestDoProtectedGoogleUpdateInstallWithFailingSetupFiles();
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

}  // namespace omaha
