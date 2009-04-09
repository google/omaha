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
// There are two phases of setup:
//  1) Copy the Google Update files to the install location.
//   * Executed by InstallGoogleUpdateAndApp().
//   * This is done in an instance running from a temp location.
//  2) Do everything else to install Google Update.
//   * Executed by SetupGoogleUpdate().
//   * This is done in an instance running from the installed location.
//
//  Uninstall() undoes both phases of setup.
//  All methods assume the instance is running with the correct permissions.

#ifndef OMAHA_SETUP_SETUP_H__
#define OMAHA_SETUP_SETUP_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

class GLock;
class HighresTimer;
struct NamedObjectAttributes;

struct CommandLineArgs;
class SetupFiles;

class Setup {
 public:
  Setup(bool is_machine, const CommandLineArgs* args);
  ~Setup();

  // Installs Google Update, if necessary, and launches a worker to install app.
  // Handles elevation.
  HRESULT Install(const CString& cmd_line);

  // Installs Google Update silently without installing an app.
  HRESULT InstallSelfSilently();

  // Updates Google Update.
  HRESULT UpdateSelfSilently();

  // Repairs Google Update for Code Red recovery.
  HRESULT RepairSilently();

  // Completes installation from the installed location.
  HRESULT SetupGoogleUpdate();

  // Obtains the Setup Lock and uninstalls all Google Update versions if
  // Google Update can be uninstalled.
  HRESULT Uninstall();

  // Verifies that Google Update is either properly installed or uninstalled
  // completely. Returns whether Google Update is installed.
  // TODO(omaha): Consider making this and did_uninstall_ non-static after
  // refactoring Setup phases. May require a Setup member in Goopdate.
  static void CheckInstallStateConsistency(bool is_machine);

  // Writes error info for silent updates to the registry so the installed Omaha
  // can send an update failed ping.
  static void PersistUpdateErrorInfo(bool is_machine,
                                     HRESULT error,
                                     int extra_code1,
                                     const CString& version);

  // Reads the error info for silent updates from the registry if present and
  // deletes it. Returns true if the data is valid.
  static bool ReadAndClearUpdateErrorInfo(bool is_machine,
                                          DWORD* error_code,
                                          DWORD* extra_code1,
                                          CString* version);

  // Marks Google Update EULA as accepted by deleting the registry value.
  // Does not touch apps' EULA state.
  static HRESULT SetEulaAccepted(bool is_machine);

  int extra_code1() const { return extra_code1_; }

 private:
  // Defines the mode of the instance.
  enum Mode {
    MODE_UNKNOWN,
    MODE_INSTALL,
    MODE_SELF_INSTALL,
    MODE_SELF_UPDATE,
    MODE_REPAIR,
    MODE_PHASE2,
    MODE_UNINSTALL,
  };

  typedef std::vector<uint32> Pids;

  // Does all non-elevation work for Install().
  HRESULT DoInstall();

  // Handles Setup lock acquisition failures and returns the error to report.
  HRESULT HandleLockFailed(int lock_version);

  // Does the install work within all necessary locks, which have already been
  // obtained.
  // If handoff_process is not NULL on a successful return, the caller should
  // wait for this process after releasing the locks.
  HRESULT DoProtectedInstall(HANDLE* handoff_process);

  // Uninstalls all Google Update versions after checking if Google Update can
  // be uninstalled.
  HRESULT DoProtectedUninstall();

  // Returns whether Google Update should be installed.
  bool ShouldInstall(SetupFiles* setup_files);

  // Returns whether the same version of Google Update should be over-installed.
  bool ShouldOverinstallSameVersion(SetupFiles* setup_files);

  // Increments the usage stat if a core process is not running and the existing
  // version is >= 1.2.0.0.
  void UpdateCoreNotRunningMetric(const CString& existing_version);

  HRESULT DoProtectedGoogleUpdateInstall(SetupFiles* setup_files);

  // Rolls back the changes made during DoProtectedGoogleUpdateInstall().
  // Call when that method fails.
  void RollBack(SetupFiles* setup_files);

  // Installs the Google Update files.
  HRESULT InstallPhase1();

  // Tells other instances to stop.
  HRESULT StopGoogleUpdate();

  // Tells other instances to stop then waits for them to exit.
  HRESULT StopGoogleUpdateAndWait();

  // Sets the non-legacy shutdown event to signal other instances for this user
  // or machine to exit.
  HRESULT SignalShutdownEvent();

  // Signals the legacy quiet mode events to cause all legacy Google Update
  // processes for this user or machine to exit.
  HRESULT SignalLegacyShutdownEvents();

  // Creates and sets the specified legacy event.
  HRESULT CreateLegacyEvent(const CString& event_name,
                            HANDLE* event_handle) const;

  // Releases all the shutdown events.
  void ReleaseShutdownEvents();

  // Sets the setup complete event, signalling the other setup instance to
  // release the Setup Lock.
  void SetSetupCompleteEvent() const;

  // Waits for other instances of GoogleUpdate.exe to exit.
  HRESULT WaitForOtherInstancesToExit(const Pids& pids);

  // Gets the list of all the GoogleUpdate.exe processes to wait for.
  HRESULT GetPidsToWaitFor(Pids* pids) const;

  // Gets the list of GoogleUpdate processes to wait for based on processes'
  // command line.
  HRESULT GetPidsToWaitForUsingCommandLine(Pids* pids) const;

  // Returns whether elevation is required to perform this install.
  bool IsElevationRequired() const;

  // Starts Omaha elevated if possible and waits for it to exit.
  // The same arguments are passed to the elevated instance.
  HRESULT ElevateAndWait(const CString& cmd_line);

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

  // Launches a worker process from the installed location to run setup phase 2,
  // if specified, and install the app.
  HRESULT LaunchInstalledWorker(bool do_setup_phase_2, HANDLE* process);

  // Starts the core.
  HRESULT StartMachineCoreProcess() const;
  HRESULT StartUserCoreProcess(const CString& core_cmd_line) const;

  // Returns the pids of running Omaha 2 Core processes for this user/system.
  HRESULT FindCoreProcesses(Pids* found_core_pids) const;

  // Forcefully kills appropriate core processes using ::TerminateProcess().
  HRESULT TerminateCoreProcesses() const;

  // Verifies that the appropriate core is running.
  bool IsCoreProcessRunning() const;

  // Starts the installed long-lived process.
  HRESULT StartCore() const;

  // Waits for the process or an event.
  // If the process exits before the event is signaled, the exit code is
  // returned in exit_code.
  // The event should not be signaled until at least the point where the process
  // is handling and reporting its own errors.
  HRESULT WaitForProcessExitOrEvent(HANDLE process,
                                    HANDLE event,
                                    uint32* exit_code) const;

  // Waits for the UI in the handoff worker to be initialized such that it can
  // handle its own errors.
  HRESULT WaitForHandoffWorker(HANDLE process) const;

  // Returns the SID to use for process searches, mutexes, etc. during this
  // installation.
  HRESULT GetAppropriateSid(CString* sid) const;

  // Initializes the Setup Lock with correct name and security attributes.
  static bool InitSetupLock(bool is_machine, GLock* setup_lock);

  // Initializes legacy Setup Locks with correct name and security attributes.
  bool InitLegacySetupLocks(GLock* lock10,
                            GLock* lock11_user,
                            GLock* lock11_machine);

  // Returns true if it can instantiate MSXML parser.
  static bool HasXmlParser();

  // Returns true if GoogleUpdate can be uninstalled now.
  bool CanUninstallGoogleUpdate() const;

  bool IsInteractiveInstall() const;

  bool ShouldWaitForWorkerProcess() const;

  // Sets values for OEM installs in the registry.
  HRESULT SetOemInstallState();

  // Sets or clears the flag that prevents Google Update from using the network
  // until the EULA has been accepted based on whether the eularequired flag
  // appears on the command line.
  HRESULT SetEulaRequiredState();

  // Marks Google Update EULA as not accepted if it is not already installed.
  // Does not touch apps' EULA state.
  static HRESULT SetEulaNotAccepted(bool is_machine);

  const bool is_machine_;
  Mode mode_;
  const CommandLineArgs* const args_;
  CString saved_version_;  // Previous version saved for roll back.
  scoped_event legacy_1_0_shutdown_event_;
  scoped_event legacy_1_1_shutdown_event_;
  scoped_event shutdown_event_;
  int extra_code1_;

  // Whether an offline worker has been launched. Not valid until worker has
  // been launched. If true, this Setup instance is for offline metainstaller.
  bool launched_offline_worker_;

  scoped_ptr<HighresTimer> metrics_timer_;

  // Whether this process uninstalled Google Update for any reason.
  // Access must be protected by the Setup Lock.
  static bool did_uninstall_;

  friend class SetupTest;
  friend class SetupOfflineInstallerTest;
  DISALLOW_EVIL_CONSTRUCTORS(Setup);
};

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_H__

