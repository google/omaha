// Copyright 2013 Google Inc.
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
// Apps may define commands using Registry entries. There are two supported
// formats:
//
// Legacy Format:
// ROOT\\Software\\Google\\Update\\Clients\\{app-guid}
//   <command-id> = REG_SZ (command line)
//
// New Format:
// ROOT\\Software\\Google\\Update\\Clients\\{app-guid}\\Commands\\<command-id>
//   CommandLine        = REG_SZ
//   SendsPings         = DWORD
//   WebAccessible      = DWORD
//   ReportingId        = DWORD
//   AutoRunOnOSUpgrade = DWORD
//   RunAsUser          = DWORD
//
// Only the command line is required, all other values default to 0. It is not
// possible to set other values using the Legacy format.
//
// If RunAsUser is true, or if the command is registered at user level, the app
// command will be run at Medium integrity as the logged-in user. Otherwise,
// commands registered at system-level will run as System at High integrity.

#ifndef OMAHA_GOOPDATE_APP_COMMAND_CONFIGURATION_H__
#define OMAHA_GOOPDATE_APP_COMMAND_CONFIGURATION_H__

#include <atlstr.h>
#include <windows.h>
#include <map>
#include <memory>
#include <vector>

#include "base/basictypes.h"

namespace omaha {

class AppCommand;

// Loads metadata for named commands for installed apps. This class is not
// threadsafe.
class AppCommandConfiguration {
 public:
  static HRESULT Load(const CString& app_guid,
                      bool is_machine,
                      const CString& command_id,
                      std::unique_ptr<AppCommandConfiguration>* configuration);

  AppCommand* Instantiate(const CString& session_id) const;

  const CString& command_line() const { return command_line_; }

  bool sends_pings() const { return sends_pings_; }

  // Returns true if this command is allowed to be invoked through the
  // OneClick control.
  bool is_web_accessible() const { return is_web_accessible_; }

  bool run_as_user() const { return run_as_user_; }

  bool capture_output() const { return capture_output_; }

  int reporting_id() const { return reporting_id_; }

  // Returns true if this command should be executed upon an OS upgrade.
  bool auto_run_on_os_upgrade() const { return auto_run_on_os_upgrade_; }

  // Enumerates all defined new-format app commands for a specific app guid.
  // Legacy-format commands will be ignored.
  static HRESULT EnumCommandsForApp(bool is_machine,
                                    const CString& app_guid,
                                    std::vector<CString>* commands);

  // Enumerates all defined app commands on this Omaha install.  The output is
  // a map whose keys are app guids, and whose values are a set of command IDs.
  // Only new-format app commands will be detected; legacy commands are ignored.
  static HRESULT EnumAllCommands(
      bool is_machine,
      std::map<CString, std::vector<CString> >* commands);

 private:
  AppCommandConfiguration(const CString& app_guid,
                          bool is_machine,
                          const CString& command_id,
                          const CString& command_line,
                          bool sends_pings,
                          bool is_web_accessible,
                          bool auto_run_on_os_upgrade,
                          DWORD reporting_id,
                          bool run_as_user,
                          bool capture_output);

  // Identifying information.
  const CString app_guid_;
  const bool is_machine_;
  const CString command_id_;

  // Configuration from the registry.
  const CString command_line_;
  const bool sends_pings_;
  const bool is_web_accessible_;
  const bool run_as_user_;
  const bool capture_output_;
  const int reporting_id_;
  const bool auto_run_on_os_upgrade_;

  DISALLOW_COPY_AND_ASSIGN(AppCommandConfiguration);
};  // class AppCommand

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_CONFIGURATION_H__
