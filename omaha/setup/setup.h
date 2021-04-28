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
//
// There are two phases of setup. Both are done in an instance running from a
// temp location.
//  1) Copy the Google Update files to the install location.
//  2) Do everything else to install Google Update.
//   * Executed by SetupGoogleUpdate().
//
//  Uninstall() undoes both phases of setup.
//  All methods assume the instance is running with the correct permissions.

#ifndef OMAHA_SETUP_SETUP_H__
#define OMAHA_SETUP_SETUP_H__

#include <windows.h>
#include <atlstr.h>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class GLock;
class HighresTimer;
struct NamedObjectAttributes;

class SetupFiles;

class Setup {
 public:
  explicit Setup(bool is_machine);
  ~Setup();

  // Installs Omaha if necessary.
  HRESULT Install(RuntimeMode runtime_mode);

  // Acquires the Setup Lock and uninstalls all Omaha versions if Omaha can be
  // uninstalled.
  HRESULT Uninstall(bool send_uninstall_ping);

  // Verifies that Omaha is either properly installed or uninstalled completely.
  // TODO(omaha): Consider making this and did_uninstall_ non-static after
  // refactoring Setup phases. May require a Setup member in Goopdate.
  static void CheckInstallStateConsistency(bool is_machine);

  int extra_code1() const { return extra_code1_; }

  void set_is_self_update(bool is_self_update) {
    is_self_update_ = is_self_update;
  }

 private:
  typedef std::vector<uint32> Pids;

  // Completes installation.
  HRESULT SetupGoogleUpdate();

  // Handles Setup lock acquisition failures and returns the error to report.
  HRESULT HandleLockFailed(int lock_version);

  // Does the install work within all necessary locks, which have already been
  // acquired.
  HRESULT DoProtectedInstall(RuntimeMode runtime_mode);

  // Uninstalls all Google Update versions after checking if Google Update can
  // be uninstalled.
  HRESULT DoProtectedUninstall(bool send_uninstall_ping);

  // Returns whether Google Update should be installed.
  bool ShouldInstall();

  // Returns whether the currently installed version of Google Update should be
  // over-installed.
  bool ShouldOverinstall();

  HRESULT DoProtectedGoogleUpdateInstall(SetupFiles* setup_files);

  // Rolls back the changes made during DoProtectedGoogleUpdateInstall().
  // Call when that method fails.
  void RollBack(SetupFiles* setup_files);

  // Tells other instances to stop.
  HRESULT StopGoogleUpdate();

  // Returns how long to wait before terminating GoogleUpdate.exe forcefully.
  int GetForceKillWaitTimeMs() const;

  // Tells other instances to stop then waits for them to exit.
  HRESULT StopGoogleUpdateAndWait(int wait_time_before_kill_ms);

  // Sets the shutdown event to signal other instances for this user or machine
  // to exit.
  HRESULT SignalShutdownEvent();

  // Releases all the shutdown events.
  void ReleaseShutdownEvents();

  // Waits for other instances of GoogleUpdate.exe to exit.
  HRESULT WaitForOtherInstancesToExit(const Pids& pids,
                                      int wait_time_before_kill_ms);

  // Gets the list of all the GoogleUpdate.exe processes to wait for.
  HRESULT GetPidsToWaitFor(Pids* pids) const;

  // Gets a list of GoogleUpdate.exe processes for user or machine that are
  // running from the respective official directory, except "/install" or
  // "/registerproduct" instances.
  // In the machine case we search in all the accounts since the workers can be
  // running in any admin account and the machine update worker runs as SYSTEM.
  // In the user case, we only search the user's account.
  // In both cases, the command line location is used to determine the
  // machine/user cases.
  HRESULT GetPidsToWaitForUsingCommandLine(Pids* pids) const;

  // Returns whether elevation is required to perform this install.
  bool IsElevationRequired() const;

// TODO(omaha3): Support offline builds. Prefer to detect and maybe copy outside
// Setup.
#if 0
  // Given a guid, finds and copies the offline manifest and binaries from the
  // current module directory to the offline_dir passed in. offline_dir is
  // typically the Google\Update\Offline\ directory. The offline manifest is
  // copied to offline_dir\{GUID}.gup. The binaries are in the format
  // "Installer.msi.{GUID}", and they are copied to the offline_dir under the
  // subdirectory {GUID}, as Installer.msi.
  static HRESULT CopyOfflineFilesForGuid(const CString& app_guid,
                                         const CString& offline_dir);

  // For all the applications that have been requested, copy the offline
  // binaries. Calls CopyOfflineFilesForGuid() for each app_guid.
  bool CopyOfflineFiles(const CString& offline_dir);
#endif

  // Starts the core.
  HRESULT StartMachineCoreProcess() const;
  HRESULT StartUserCoreProcess(const CString& core_cmd_line) const;

  // Returns the pids of running Omaha 2 Core processes for this user/system.
  HRESULT FindCoreProcesses(Pids* found_core_pids) const;

  // Forcefully kills appropriate core processes using ::TerminateProcess().
  HRESULT TerminateCoreProcesses() const;

  // Verifies that the appropriate core is running.
  bool IsCoreProcessRunning() const;

  // Starts the long-lived Core process.
  HRESULT StartCore() const;

  // Returns the SID to use for process searches, mutexes, etc. during this
  // installation.
  HRESULT GetAppropriateSid(CString* sid) const;

  // Initializes the Setup Lock with correct name and security attributes.
  static bool InitSetupLock(bool is_machine, GLock* setup_lock);

  // Returns true if GoogleUpdate can be uninstalled now.
  bool CanUninstallGoogleUpdate() const;

  // The state of the RuntimeMode in the registry.
  RuntimeMode GetRuntimeMode() const;
  HRESULT SetRuntimeMode(RuntimeMode runtime_mode) const;

  // Sends the uninstall ping and waits for the ping to be sent.
  HRESULT SendUninstallPing();

  const bool is_machine_;
  bool is_self_update_;
  CString saved_version_;  // Previous version saved for roll back.
  scoped_event shutdown_event_;
  int extra_code1_;

  std::unique_ptr<HighresTimer> metrics_timer_;

  // Whether this process uninstalled Google Update for any reason.
  // Access must be protected by the Setup Lock.
  static bool did_uninstall_;

  friend class SetupTest;
  friend class SetupOfflineInstallerTest;
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_H__

