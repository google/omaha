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
// InstallerWrapper supports installing one app at a time. Installs from
// multiple instances are serialized by a mutex.

#ifndef OMAHA_GOOPDATE_INSTALLER_WRAPPER_H_
#define OMAHA_GOOPDATE_INSTALLER_WRAPPER_H_

#include <windows.h>
#include <atlstr.h>
#include <queue>
#include <utility>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/synchronized.h"
#include "omaha/goopdate/installer_result_info.h"

// TODO(omaha): consider removing this dependency on the model.
#include "omaha/goopdate/model.h"

namespace omaha {

class AppVersion;
class Process;


class InstallerWrapper {
 public:
  explicit InstallerWrapper(bool is_machine);
  ~InstallerWrapper();
  HRESULT Initialize();

  // Installs the specified app.
  // This is a blocking call. All errors are reported through the return
  // value. Depending on the return value, messages may be obtained as follows:
  //  * SUCCEEDED(hr): result_info may contain a custom success message.
  //  * GOOPDATEINSTALL_E_INSTALLER_FAILED: output parameters contain
  //    information and a message for the installer error.
  //  * Other error values: Callers may use GetMessageForError() to convert the
  //    error value to an error message.
  HRESULT InstallApp(HANDLE user_token,
                     const GUID& app_guid,
                     const CString& installer_path,
                     const CString& arguments,
                     const CString& installer_data,
                     const CString& language,
                     const CString& untrusted_data,
                     int install_priority,
                     InstallerResultInfo* result_info);

  // Validate that the installer wrote the client key and the product version.
  HRESULT CheckApplicationRegistration(const GUID& app_guid,
                                       const CString& registered_version,
                                       const CString& expected_version,
                                       const CString& previous_version,
                                       bool is_update) const;

  // Obtains the localized text for Omaha errors that may occur during install.
  static CString GetMessageForError(HRESULT error_code,
                                    const CString& installer_filename,
                                    const CString& language);

  void set_num_tries_when_msi_busy(int num_tries_when_msi_busy);

 private:
  // Types of installers that Omaha supports.
  enum InstallerType {
    UNKNOWN_INSTALLER = 0,
    CUSTOM_INSTALLER,
    MSI_INSTALLER,
    MAX_INSTALLER  // Last Installer Type value.
  };

  // Determines the executable, command line, and installer type for
  // the installation based on the filename.
  static HRESULT BuildCommandLineFromFilename(const CString& filename,
                                              const CString& arguments,
                                              const CString& installer_data,
                                              CString* executable_name,
                                              CString* command_line,
                                              InstallerType* installer_type);

  // Executes the installer and waits for it to complete. Retries if necessary.
  HRESULT ExecuteAndWaitForInstaller(HANDLE user_token,
                                     const GUID& app_guid,
                                     const CString& executable_name,
                                     const CString& command_line,
                                     InstallerType installer_type,
                                     const CString& language,
                                     const CString& untrusted_data,
                                     int install_priority,
                                     InstallerResultInfo* result_info);

  // Executes the installer for ExecuteAndWaitForInstaller.
  HRESULT DoExecuteAndWaitForInstaller(HANDLE user_token,
                                       const GUID& app_guid,
                                       const CString& executable_name,
                                       const CString& command_line,
                                       InstallerType installer_type,
                                       const CString& language,
                                       const CString& untrusted_data,
                                       int install_priority,
                                       InstallerResultInfo* result_info);

  // Determines whether the installer succeeded and returns completion info.
  HRESULT GetInstallerResult(const GUID& app_guid,
                             InstallerType installer_type,
                             const Process& p,
                             const CString& language,
                             InstallerResultInfo* result_info);

  // Does most of the work for GetInstallerResult.
  void GetInstallerResultHelper(const GUID& app_guid,
                                InstallerType installer_type,
                                uint32 exit_code,
                                const CString& language,
                                InstallerResultInfo* result_info);

  // Cleans up the registry from an installer that set custom result values.
  void ClearInstallerResultApiValues(const CString& app_guid);

  // Installs the specified application and reports the results.
  HRESULT DoInstallApp(HANDLE user_token,
                       const GUID& app_guid,
                       const CString& installer_path,
                       const CString& arguments,
                       const CString& installer_data,
                       const CString& language,
                       const CString& untrusted_data,
                       int install_priority,
                       InstallerResultInfo* result_info);

  // Whether this object is running in a machine Goopdate instance.
  const bool is_machine_;

  // The number of times to try installing an MSI when an MSI install is
  // already running. There is an exponential backoff starting from
  // kMsiAlreadyRunningRetryDelayBaseMs.
  int num_tries_when_msi_busy_;

  // This is the base retry delay between retries when msiexec returns
  // ERROR_INSTALL_ALREADY_RUNNING. We exponentially backoff from this value.
  // Note that there is an additional delay for the MSI call, so the tries may
  // be a few seconds further apart.
  static const int kMsiAlreadyRunningRetryDelayBaseMs = 5000;

  // Interval to wait for installer completion.
  static const int kInstallerCompleteIntervalMs = 30 * 60 * 1000;

  // Ensures that a single installer is run by us at a time.
  // Not sure if we can run installers in different sessions without
  // interference. In that case we can use a local lock instead of a
  // global lock.
  GLock installer_lock_;

  friend class InstallerWrapperTest;

  DISALLOW_COPY_AND_ASSIGN(InstallerWrapper);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_INSTALLER_WRAPPER_H_

