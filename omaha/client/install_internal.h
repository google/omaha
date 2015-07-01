// Copyright 2009-2010 Google Inc.
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

#ifndef OMAHA_CLIENT_INSTALL_INTERNAL_H_
#define OMAHA_CLIENT_INSTALL_INTERNAL_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/command_line.h"

namespace omaha {

class SplashScreen;

namespace internal {

// Elevates TBD and waits for it to exit.
HRESULT DoElevation(bool is_interactive,
                    bool is_install_elevated_instance,
                    const CString& cmd_line,
                    DWORD* exit_code);

// Installs Omaha if necessary and app(s) if is_app_install.
HRESULT DoInstall(bool is_machine,
                  bool is_app_install,
                  bool is_eula_required,
                  bool is_oem_install,
                  const CString& current_version,
                  const CommandLineArgs& args,
                  const CString& session_id,
                  SplashScreen* splash_screen,
                  int* extra_code1,
                  bool* has_setup_succeeded,
                  bool* has_launched_handoff,
                  bool* has_ui_been_displayed);

// Installs the specified applications.
HRESULT InstallApplications(bool is_machine,
                            bool is_eula_required,
                            const CommandLineArgs& args,
                            const CString& session_id,
                            SplashScreen* splash_screen,
                            bool* has_ui_been_displayed,
                            bool* has_launched_handoff);

// Starts Omaha elevated if possible and waits for it to exit.
// The same arguments are passed to the elevated instance.
HRESULT ElevateAndWait(const CString& cmd_line, DWORD* exit_code);

// IsOfflineInstall() checks if the offline manifest and offline files for the
// 'apps' exist in the current directory.
bool IsOfflineInstall(const std::vector<CommandLineAppArgs>& apps);
bool IsOfflineInstallForApp(const CString& app_id);

// CopyOfflineManifest() and CopyOfflineFilesForApp() find and copy the offline
// manifest and offline files respectively, from the current module directory to
// the offline_dir. offline_dir is typically an unique directory under the
// Google\Update\Offline\ directory.
// The offline manifest is copied to offline_dir\<kOfflineManifestFileName>.
// The binaries are in the format "file.<app_id>". Each file is copied to the
// offline_dir under the subdirectory "<app_id>", as "file". For instance,
// "Installer.msi.<app_id>" is copied as "<app_id>/Installer.msi".
HRESULT CopyOfflineManifest(const CString& offline_dir);
HRESULT CopyOfflineFilesForApp(const CString& app_id,
                               const CString& offline_dir);

// For all the applications that have been requested in the apps parameter, copy
// the offline binaries.
bool CopyOfflineFiles(bool is_machine,
                      const std::vector<CommandLineAppArgs>& apps,
                      CString* offline_dir);

// Launches a /handoff process from the installed location to install the app.
HRESULT LaunchHandoffProcess(bool is_machine,
                             const CString& offline_dir,
                             const CommandLineArgs& install_args,
                             const CString& session_id,
                             HANDLE* process);

// Waits for the process to exit and returns the exit code.
HRESULT WaitForProcessExit(HANDLE process,
                           SplashScreen* splash_screen,
                           bool* has_ui_been_displayed,
                           uint32* exit_code);

// Displays an error message and reports the error as appropriate.
void HandleInstallError(HRESULT error,
                        int extra_code1,
                        const CString& session_id,
                        bool is_machine,
                        bool is_interactive,
                        bool is_eula_required,
                        bool is_oem_install,
                        bool is_enterprise_install,
                        const CString& current_version,
                        const CString& install_source,
                        const CommandLineExtraArgs& extra_args,
                        bool has_setup_succeeded,
                        bool has_launched_handoff,
                        bool* has_ui_been_displayed);

// Returns the error text for the corresponding error value.
CString GetErrorText(HRESULT error, const CString& bundle_name);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_INTERNAL_H_
