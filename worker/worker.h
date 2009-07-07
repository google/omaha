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

#ifndef OMAHA_WORKER_WORKER_H__
#define OMAHA_WORKER_WORKER_H__

#include <atlbase.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/atlregmapex.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/shutdown_callback.h"
#include "omaha/common/shutdown_handler.h"
#include "omaha/common/thread_pool.h"
#include "omaha/common/wtl_atlapp_wrapper.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/program_instance.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/worker/com_wrapper_shutdown_handler.h"

interface IJobObserver;

namespace omaha {

static const int kThreadPoolShutdownDelayMs = 60000;

class Goopdate;
class JobObserver;
class ProgressWnd;
class Reactor;
class Setup;
class UserInterface;
class WorkerJobStrategy;
class WorkerJob;
class WorkerShutdownHandler;

class Worker : public ShutdownCallback {
 public:
  explicit Worker(bool is_machine);
  virtual ~Worker();

  HRESULT Main(Goopdate* goopdate);
  HRESULT DoOnDemand(const WCHAR* guid,
                     const CString& lang,
                     IJobObserver* observer,
                     bool is_update_check_only);

  HRESULT Shutdown();
  HRESULT InitializeThreadPool();
  HRESULT InitializeShutDownHandler(ShutdownCallback* callback);

  Reactor* reactor() const { return reactor_.get(); }
  const CommandLineArgs& args() const { return args_; }
  CString cmd_line() const { return cmd_line_; }
  bool is_machine() const { return is_machine_; }
  bool is_local_system() const { return is_local_system_; }
  bool has_uninstalled() const { return has_uninstalled_; }
  WorkerComWrapperShutdownCallBack* shutdown_callback() const {
    return shutdown_callback_.get();
  }
  void set_shutdown_callback(WorkerComWrapperShutdownCallBack* callback) {
    ASSERT1(callback);
    shutdown_callback_.reset(callback);
  }

 private:
  HRESULT StartWorkerJob();
  HRESULT QueueWorkerJob(WorkerJob* worker_job, bool delete_after_run);
  // Stops and destroys the Worker and its members.
  void StopWorker(HRESULT* worker_job_error_code);
  HRESULT DoRun();
  bool EnsureSingleAppInstaller(const CString& guid);
  bool EnsureSingleUpdateWorker();
  HRESULT InitializeUI();

  HRESULT DoInstall();
  HRESULT DoUpdateApps();
  HRESULT DoInstallGoogleUpdateAndApp();

  // Uninstalls GoogleUpdate conditionally.
  void MaybeUninstallGoogleUpdate();

  HRESULT HandleSetupError(HRESULT error, int extra_code1);

  void CollectAmbientUsageStats();

  const CommandLineAppArgs& GetPrimaryJobInfo() const;

  // Displays an error using the normal UI. Does not display a message box when
  // running silently.
  void DisplayError(const CString& error_text, HRESULT error);


  // Displays an error in a Windows Message Box. Useful when UI initialization
  // fails. Does not display a message box when running silently.
  virtual void DisplayErrorInMessageBox(const CString& error_text);


  bool is_machine_;
  bool is_local_system_;
  bool has_uninstalled_;        // True if the worker has uninstalled Omaha.
  CString cmd_line_;            // Command line, as provided by the OS.
  scoped_ptr<ProgramInstance> single_update_worker_;
  scoped_ptr<ProgramInstance> single_install_worker_;
  // The ProgressWnd is owned by this class so that
  // JobObserverCallMethodDecorator can release it without destroying it as
  // expected in ProgressWnd and required for calling OnComplete in the same
  // thread as the message loop when displaying an early error.
  scoped_ptr<ProgressWnd> progress_wnd_;
  scoped_ptr<JobObserver> job_observer_;
  scoped_ptr<Reactor>     reactor_;
  scoped_ptr<ShutdownHandler> shutdown_handler_;
  CommandLineArgs args_;
  scoped_ptr<WorkerJob> worker_job_;
  scoped_ptr<WorkerComWrapperShutdownCallBack> shutdown_callback_;

  // Message loop for non-COM modes.
  CMessageLoop message_loop_;
  scoped_ptr<ThreadPool>  thread_pool_;

  friend class WorkerTest;

  DISALLOW_EVIL_CONSTRUCTORS(Worker);
};

}  // namespace omaha

#endif  // OMAHA_WORKER_WORKER_H__

