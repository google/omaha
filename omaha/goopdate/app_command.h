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

#ifndef OMAHA_GOOPDATE_APP_COMMAND_H__
#define OMAHA_GOOPDATE_APP_COMMAND_H__

#include <windows.h>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/scoped_any.h"
#include "omaha/goopdate/app_command_formatter.h"
#include "third_party/bar/shared_ptr.h"

namespace omaha {

class AppCommandDelegate;
class AppCommandVerifier;
class AppCommandCompletionObserver;

// Executes an application command with the appropriate user identity and
// integrity. Provides the client with access to the status, exit code, and
// output of the command. This class is not threadsafe.
class AppCommand {
 public:
  // Instantiates an application command corresponding to |cmd_line|.
  // |is_web_accessible| is an access control flag (that it is the client's
  // responsibility to enforce.
  // |run_as_user| must be false for user-level commands. Otherwise, it
  // indicates whether the command should run as a medium-integrity user or with
  // system-integrity.
  // |capture_output| indicates that the command's output should be recorded for
  // later retrieval.
  // |auto_run_on_os_upgrade| indicates that this command should be executed
  // upon an OS upgrade. auto_run_on_os_upgrade are currently run as
  // fire-and-forget. The values for GetStatus(), GetExitCode(), and GetOutput()
  // are not applicable for auto_run_on_os_upgrade commands.
  // |delegate|, if supplied, will be notified when the process is started and
  // completes (or fails). The AppCommand takes ownership of the delegate,
  // which will only be destroyed when both the launched process has completed
  // and the AppCommand instance has been destroyed.
  AppCommand(const CString& cmd_line,
             bool is_web_accessible,
             bool run_as_user,
             bool capture_output,
             bool auto_run_on_os_upgrade,
             AppCommandDelegate* delegate);

  ~AppCommand();

  // Executes the command. If successful, the caller is responsible for closing
  // the process HANDLE. This method does not enforce the 'web accessible'
  // constraint (this is the caller's responsibility).
  // |verifier| is optional and, if provided, will be used to verify the safety
  // of the executable implementing the command.
  // |parameters| are positional parameters to the command and must correspond
  // in size to the number of expected parameters (command-defined).
  HRESULT Execute(AppCommandVerifier* verifier,
                  const std::vector<CString>& parameters,
                  HANDLE* process);

  // Returns the status of the last execution of this instance, or
  // COMMAND_STATUS_INIT if none.
  AppCommandStatus GetStatus();

  // Returns the exit code if status is COMMAND_STATUS_COMPLETE. Otherwise,
  // returns MAXDWORD.
  DWORD GetExitCode();

  // Returns the command output if status is COMMAND_STATUS_COMPLETE. Otherwise,
  // returns an empty string.
  CString GetOutput();

  // Blocks up to |timeoutMs| milliseconds until the currently running command
  // completes. Returns true if the command has completed.
  bool Join(int timeoutMs);

  // Returns true if this command is allowed to be invoked through the
  // OneClick control.
  bool is_web_accessible() const { return is_web_accessible_; }

 private:
  const AppCommandFormatter command_formatter_;
  const bool is_web_accessible_;
  const bool run_as_user_;
  const bool capture_output_;
  const bool auto_run_on_os_upgrade_;

  shared_ptr<AppCommandDelegate> delegate_;

  // Tracks state of the currently running process.
  CComPtr<AppCommandCompletionObserver> completion_observer_;

  DISALLOW_COPY_AND_ASSIGN(AppCommand);
};  // class AppCommand

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_H__
