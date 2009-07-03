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

#ifndef OMAHA_WORKER_INSTALL_MANAGER_H__
#define OMAHA_WORKER_INSTALL_MANAGER_H__

#include <windows.h>
#include <atlstr.h>
#include <queue>
#include <utility>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/synchronized.h"
#include "omaha/worker/job.h"

namespace omaha {

class Process;

class InstallManager {
 public:
  explicit InstallManager(bool is_machine);
  HRESULT InstallJob(Job* job);
  CompletionInfo error_info() const { return error_info_; }

 private:
  // These values are a public API. Do not remove or move existing values.
  enum InstallerResult {
    INSTALLER_RESULT_SUCCESS = 0,
    INSTALLER_RESULT_FAILED_CUSTOM_ERROR = 1,
    INSTALLER_RESULT_FAILED_MSI_ERROR = 2,
    INSTALLER_RESULT_FAILED_SYSTEM_ERROR = 3,
    INSTALLER_RESULT_EXIT_CODE = 4,
    INSTALLER_RESULT_DEFAULT = INSTALLER_RESULT_EXIT_CODE,
    INSTALLER_RESULT_MAX,
  };

  // Types of installers that Omaha supports.
  enum InstallerType {
    UNKNOWN_INSTALLER = 0,
    CUSTOM_INSTALLER,
    MSI_INSTALLER,
    MAX_INSTALLER  // Last Installer Type value.
  };

  // Gets the info about the signaled job and performs the installation.
  HRESULT StartAndMonitorInstaller();

  // Determines the executable, command line, and installer type for
  // the installation based on the filename.
  HRESULT BuildCommandLineFromFilename(const CString& filename,
                                       const CString& arguments,
                                       const CString& installer_data,
                                       CString* executable_name,
                                       CString* command_line,
                                       InstallerType* installer_type);

  // Executes the installer and waits for it to complete. Retries if necessary.
  HRESULT ExecuteAndWaitForInstaller(const CString& executable_name,
                                     const CString& command_line,
                                     const CString& app_guid,
                                     InstallerType installer_type);

  // Executes the installer for ExecuteAndWaitForInstaller.
  HRESULT DoExecuteAndWaitForInstaller(const CString& executable_name,
                                       const CString& command_line,
                                       const CString& app_guid,
                                       InstallerType installer_type);

  // Builds and returns the command line that is used to launch the msi.
  CString BuildMsiCommandLine(const CString& arguments,
                              const CString& filename,
                              const CString& enclosed_installer_data_file_path);

  // Determines whether the installer succeeded and returns completion info.
  HRESULT GetInstallerResult(const CString& app_guid,
                             InstallerType installer_type,
                             const Process& p,
                             CompletionInfo* completion_info);

  // Does most of the work for GetInstallerResult.
  void GetInstallerResultHelper(const CString& app_guid,
                                InstallerType installer_type,
                                uint32 exit_code,
                                CompletionInfo* completion_info);

  // Cleans up the registry from an installer that set custom result values.
  void CleanupInstallerResultRegistry(const CString& app_guid);

  // Executes the specified installer.
  HRESULT DoInstallation();

  // Installs the specified application and reports the results.
  HRESULT InstallDownloadedFile();

  // Validate that the installer wrote the client key and the product version.
  HRESULT CheckApplicationRegistration(const CString& previous_version);

  // TODO(omaha): consider helpers that take an app_guid and return
  // the installer results. They might be better since they abstract out
  // where the values are stored in registry. The current code is less
  // encapsulated and a lot more calling code needs to change when the physical
  // location changes in registry. Ideally only a few lines of code should
  // change, in the event the registry location changes.

  // Gets the InstallerResult type, possibly from the registry.
  static InstallerResult GetInstallerResultType(
      const CString& app_client_state_key);

  // Reads the InstallerError value from the registry if present.
  static void ReadInstallerErrorOverride(const CString& app_client_state_key,
                                         DWORD* installer_error);

  // Reads the InstallerResultUIString value from the registry if present.
  static void InstallManager::ReadInstallerResultUIStringOverride(
      const CString& app_client_state_key,
      CString* installer_result_uistring);

  // If we are installing an application.
  bool is_installing_;

  // Whether this object is running in a machine Goopdate instance.
  bool is_machine_;

  // The error information for the install.
  CompletionInfo error_info_;

  // Ensures that a single installer is run by us at a time.
  // Not sure if we can run installers in different sessions without
  // interference. In that case we can use a local lock instead of a
  // global lock.
  GLock installer_lock_;

  // The job instance.
  Job* job_;

  friend class InstallManagerTest;

  DISALLOW_EVIL_CONSTRUCTORS(InstallManager);
};

}  // namespace omaha

#endif  // OMAHA_WORKER_INSTALL_MANAGER_H__

