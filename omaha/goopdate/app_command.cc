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

#include "omaha/goopdate/app_command.h"

#include "base/file.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/app_command_completion_observer.h"
#include "omaha/goopdate/app_command_delegate.h"
#include "omaha/goopdate/app_command_verifier.h"

namespace omaha {

AppCommand::AppCommand(const CString& cmd_line,
                       bool is_web_accessible,
                       bool run_as_user,
                       bool capture_output,
                       bool auto_run_on_os_upgrade,
                       AppCommandDelegate* delegate)
    : command_formatter_(cmd_line),
      is_web_accessible_(is_web_accessible),
      run_as_user_(run_as_user),
      capture_output_(capture_output),
      auto_run_on_os_upgrade_(auto_run_on_os_upgrade),
      delegate_(delegate) {
}

AppCommand::~AppCommand() {
}

HRESULT AppCommand::Execute(AppCommandVerifier* verifier,
                            const std::vector<CString>& parameters,
                            HANDLE* process) {
  ASSERT1(process);

  completion_observer_.Release();

  if (!process) {
    return E_INVALIDARG;
  }
  *process = NULL;

  CString process_name;
  CString command_line_arguments;
  HRESULT hr = command_formatter_.Format(parameters,
                                         &process_name,
                                         &command_line_arguments);
  // Keep the file locked for writing so other processes can't modify the file
  // while we are verifying it and executing it.
  FileLock file_lock;  // The destructor of the FileLock unlocks the file
  if (SUCCEEDED(hr)) {
    hr = file_lock.Lock(process_name);
  }

  if (SUCCEEDED(hr) && verifier) {
    hr = verifier->VerifyExecutable(process_name);
  }

  scoped_process temp_process;

  CString command_line = process_name;
  EnclosePath(&command_line);
  command_line.AppendChar(_T(' '));
  command_line.Append(command_line_arguments);

  scoped_handle pipe_read;

  if (SUCCEEDED(hr)) {
    if (run_as_user_) {
      // Pass |true| for |is_machine|, since that's the only time |run_as_user_|
      // should be set.
      hr = goopdate_utils::LaunchCmdLine(
          true, command_line, address(temp_process),
          capture_output_ ? address(pipe_read) : NULL);
    } else {
      hr = omaha::vista::RunAsCurrentUser(
          command_line,
          capture_output_ ? address(pipe_read) : NULL,
          address(temp_process));
    }
  }

  if (delegate_.get()) {
    delegate_->OnLaunchResult(hr);
  }

  scoped_handle return_process;

  if (SUCCEEDED(hr)) {
    hr = DuplicateHandleIntoCurrentProcess(::GetCurrentProcess(),
                                           get(temp_process),
                                           address(return_process));
  }


  if (SUCCEEDED(hr) && !auto_run_on_os_upgrade_) {
    hr = AppCommandCompletionObserver::Start(release(temp_process),
                                             release(pipe_read),
                                             delegate_,
                                             &completion_observer_);
  }

  if (FAILED(hr)) {
    // If we have previously reported a successful launch to the delegate, let's
    // also inform them about the failure to observe.
    if (valid(temp_process) && delegate_.get()) {
      delegate_->OnObservationFailure(hr);
    }
  } else {
    *process = release(return_process);
  }

  return hr;
}

AppCommandStatus AppCommand::GetStatus() {
  if (!completion_observer_) {
    return COMMAND_STATUS_INIT;
  }

  return completion_observer_->GetStatus();
}

DWORD AppCommand::GetExitCode() {
  if (!completion_observer_) {
    return MAXDWORD;
  }

  return completion_observer_->GetExitCode();
}

CString AppCommand::GetOutput() {
  if (!completion_observer_) {
    return CString();
  }

  return completion_observer_->GetOutput();
}

bool AppCommand::Join(int timeoutMs) {
  if (!completion_observer_) {
      return false;
  }
  return completion_observer_->Join(timeoutMs);
}

}  // namespace omaha
