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

#ifndef OMAHA_SETUP_SETUP_GOOGLE_UPDATE_H__
#define OMAHA_SETUP_SETUP_GOOGLE_UPDATE_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/base/constants.h"

namespace omaha {

class SetupGoogleUpdate {
 public:
  SetupGoogleUpdate(bool is_machine, bool is_self_update);
  ~SetupGoogleUpdate();

  HRESULT FinishInstall();

  // Uninstalls Google Update registrations created by FinishInstall().
  void Uninstall();

  // Build the command line to execute the installed core.
  CString BuildCoreProcessCommandLine() const;

  int extra_code1() const { return extra_code1_; }

 private:
  HRESULT FinishMachineInstall();

  // Installs appropriate launch mechanism(s) starts one of them if appropriate.
  HRESULT InstallLaunchMechanisms();

  // Uninstalls appropriate launch mechanism(s).
  void UninstallLaunchMechanisms();

  // Installs the scheduled task which runs the GoogleUpdate core.
  HRESULT InstallScheduledTask();

  // Installs the service and scheduled task.
  HRESULT InstallMachineLaunchMechanisms();

  // Add Google Update to the user's Run key.
  HRESULT InstallUserLaunchMechanisms();

  // Configures goopdate to run at startup.
  //
  // @param install: true if we should configure to run at startup, false if we
  //   should clean up the configuration (meaning that we should not run at
  //   startup)
  HRESULT ConfigureUserRunAtStartup(bool install);

  HRESULT InstallRegistryValues();

  // Create's the ClientStateMedium key with relaxed ACLs.
  // Only call for machine installs.
  HRESULT CreateClientStateMedium();

  // Writes the Installation ID to the registry.
  HRESULT SetInstallationId();

  // Register COM classes and interfaces.
  HRESULT RegisterOrUnregisterCOMLocalServer(bool register_server);

  // Build the install file path for support files. For example,
  CString BuildSupportFileInstallPath(const CString& filename) const;

  // Deletes the Omaha registry keys except the machine id and the user id
  // values under the main update key.
  HRESULT DeleteRegistryKeys();

  // Uninstall previous versions after an overinstall of the new version. We do
  // the following:
  //   * Delete all sub-directories under Google\\Update, except the running
  //     version's directory and the cache directory.
  HRESULT UninstallPreviousVersions();

  const bool is_machine_;
  const bool is_self_update_;
  CString this_version_;
  int extra_code1_;

#ifdef _DEBUG
  bool have_called_uninstall_previous_versions_;
#endif

  friend class SetupGoogleUpdateTest;
  friend class AppManagerTestBase;

  DISALLOW_COPY_AND_ASSIGN(SetupGoogleUpdate);
};

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_GOOGLE_UPDATE_H__

