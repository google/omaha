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

#ifndef OMAHA_GOOPDATE_APP_COMMAND_COMPLETION_OBSERVER_H__
#define OMAHA_GOOPDATE_APP_COMMAND_COMPLETION_OBSERVER_H__

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <memory>

#include "omaha/base/synchronized.h"
#include "omaha/common/ping_event.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class AppCommandDelegate;

// Observes the executed process on a separate thread, capturing the output and
// exit code, and reporting status to a delegate. The public members of this
// class may safely be invoked from any thread.
// This is a COM object so that, during its lifetime, the process will not exit.
// The instance is AddRef'd in the instantiating thread and Release'd by the
// thread procedure when all work is completed.
class ATL_NO_VTABLE AppCommandCompletionObserver
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IUnknown {
 public:
  // Creates and starts an observer. |process| is the process to observe.
  // |output| is a readable handle to the process's output stream,
  // or NULL.
  // If Start() succeeds, |delegate| (optional) will be notified when the
  // process exits, or of an asynchronous failure. AppCommandCompletionObserver
  // holds a reference to |delegate| during the observer's lifetime.
  // If successful, the caller is responsible for Release'ing the instance
  // returned via |observer|.
  static HRESULT Start(HANDLE process,
                       HANDLE output,
                       const std::shared_ptr<AppCommandDelegate>& delegate,
                       AppCommandCompletionObserver** observer);

  // Returns the process status (one of
  // COMMAND_STATUS_{RUNNING,ERROR,COMPLETE}).
  AppCommandStatus GetStatus();

  // If an output stream handle was supplied, and if GetStatus() returns
  // COMMAND_STATUS_COMPLETE, returns the complete output of the observed
  // process. Otherwise, returns an empty string.
  CString GetOutput();

  // If COMPLETE, returns the observed process's exit code. Otherwise, returns
  // MAXDWORD.
  DWORD GetExitCode();

  // Waits up to |timeoutMs| milliseconds for the background thread to exit.
  // Returns true if the thread has exited, in which case the status will either
  // be COMPLETE or ERROR.
  bool Join(int timeoutMs);

  BEGIN_COM_MAP(AppCommandCompletionObserver)
  END_COM_MAP()

 protected:
  AppCommandCompletionObserver();
  virtual ~AppCommandCompletionObserver();

 private:
  // Initializes the object properties and spins off the background thread.
  HRESULT Init(HANDLE process,
               HANDLE output,
               const std::shared_ptr<AppCommandDelegate>& delegate);

  // Reads the child process's output for reporting to our client.
  void CaptureOutput();

  // Waits for the process to exit, returning S_OK and the exit_code or the
  // underlying error code if the wait fails.
  HRESULT WaitForProcessExit();

  // Captures the output, waits for the process to exit, and notifies the
  // delegate.
  void Execute();

  // Entrypoint for the background thread. |parameter| is the
  // AppCommandCompletionObserver instance. Calls Execute().
  static DWORD WINAPI ThreadStart(void* parameter);

  scoped_process process_;
  scoped_handle output_pipe_;
  std::shared_ptr<AppCommandDelegate> delegate_;

  scoped_handle thread_;

  LLock lock_;
  // The following are all protected by |lock_|.
  CString output_;
  DWORD exit_code_;
  AppCommandStatus status_;

  DISALLOW_COPY_AND_ASSIGN(AppCommandCompletionObserver);
};  // class AppCommandCompletionObserver

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_COMPLETION_OBSERVER_H__
