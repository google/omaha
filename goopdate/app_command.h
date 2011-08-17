// Copyright 2011 Google Inc.
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
//   CommandLine   = REG_SZ
//   SendsPings    = DWORD
//   WebAccessible = DWORD
//   ReportingId   = DWORD
//
// Only the command line is required, all other values default to 0. It is not
// possible to set other values using the Legacy format.

#ifndef OMAHA_GOOPDATE_APP_COMMAND_H__
#define OMAHA_GOOPDATE_APP_COMMAND_H__

#include <windows.h>
#include <string>
#include "base/basictypes.h"

namespace omaha {

// Loads, provides metadata for, and executes named commands for installed
// apps.
class AppCommand {
 public:
  static HRESULT Load(const CString& app_guid,
                      bool is_machine,
                      const CString& cmd_id,
                      const CString& session_id,
                      AppCommand** app_command);

  // Executes the command at the current integrity level. If successful,
  // the caller is responsible for closing the process HANDLE. This method does
  // not enforce the 'web accessible' constraint (this is the caller's
  // responsibility).
  HRESULT Execute(HANDLE* process) const;

  // Returns true if this command is allowed to be invoked through the
  // OneClick control.
  bool is_web_accessible() const { return is_web_accessible_; }

 private:
  AppCommand(const CString& app_guid,
             bool is_machine,
             const CString& cmd_id,
             const CString& cmd_line,
             bool sends_pings,
             const CString& session_id,
             bool is_web_accessible,
             DWORD reporting_id);

  // Starts a thread which waits for the process to exit, sends a ping, and then
  // terminates. Waits up to 15 minutes before aborting the wait. A ping is also
  // sent if the wait times out or if a failure occurs while starting the
  // thread, initializing the wait, or retrieving the process exit code.
  //
  // Duplicates the process HANDLE, leaving the caller with ownership of the
  // passed handle.
  //
  // The thread holds a COM object until the work is completed in order to
  // avoid early termination if all other clients of the process release their
  // references.
  void StartBackgroundThread(HANDLE process) const;

  // Identifying information.
  const CString app_guid_;
  const bool is_machine_;
  const CString cmd_id_;
  const CString session_id_;

  // Configuration from the registry.
  const CString cmd_line_;
  const bool sends_pings_;
  const bool is_web_accessible_;
  const int reporting_id_;

  DISALLOW_COPY_AND_ASSIGN(AppCommand);
};  // class AppCommand

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_H__
