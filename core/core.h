// Copyright 2007-2010 Google Inc.
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


// Core uses a reactor design pattern to register event handlers for kernel
// events, demultiplex them, and transfer control to the respective event
// handlers.

#ifndef OMAHA_CORE_CORE_H_
#define OMAHA_CORE_CORE_H_

#include <atlbase.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <string>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/shutdown_callback.h"
#include "omaha/core/google_update_core.h"
#include "omaha/core/system_monitor.h"
#include "omaha/goopdate/google_update3.h"

namespace omaha {

class Reactor;
class Scheduler;
class ShutdownHandler;

// To support hosting ATL COM objects, Core derives from CAtlExeModuleT. Other
// than the ATL module count, no functionality of CAtlExeModuleT is used.
class Core
    : public ShutdownCallback,
      public SystemMonitorObserver,
      public CAtlExeModuleT<Core> {
 public:
  Core();
  virtual ~Core();

  // Executes the instance entry point with given parameters.
  HRESULT Main(bool is_system, bool is_crash_handler_enabled);

  // Starts an update worker process if the Core is meant to run all the time.
  // If not, causes the Core to exit the process.
  HRESULT StartUpdateWorker() const;

  // Starts a code red process.
  HRESULT StartCodeRed() const;

  // Starts the crash handler.
  HRESULT StartCrashHandler() const;

  // Aggregates the core metrics.
  void AggregateMetrics() const;

  Reactor* reactor() const { return reactor_.get(); }
  bool is_system() const { return is_system_; }

  virtual LONG Unlock() throw() {
    // We are long-running independent of the ATL module count, therefore
    // transition to zero does not by itself unload the process.
    return CAtlModuleT<Core>::Unlock();
  }

 private:

  HRESULT DoMain(bool is_system, bool is_crash_handler_enabled);

  // Starts an update worker process.
  HRESULT StartUpdateWorkerInternal() const;

  bool AreScheduledTasksHealthy() const;
  bool IsServiceHealthy() const;
  bool IsCheckingForUpdates() const;
  bool ShouldRunForever() const;

  // ShutdownCallback interface.
  // Signals the core to stop handling events and exit.
  virtual HRESULT Shutdown();
  virtual HRESULT ShutdownInternal() const;

  // SystemMonitorObserver interface.
  virtual void LastCheckedDeleted();
  virtual void NoRegisteredClients();

  HRESULT DoRun();
  HRESULT DoHandleEvents();

  // Collects ambient core metrics.
  void CollectMetrics()const;

  bool is_system_;

  // True if the core has to kickoff the crash handler.
  bool is_crash_handler_enabled_;

  DWORD main_thread_id_;        // The id of the thread that runs Core::Main.

  scoped_ptr<Reactor>               reactor_;
  scoped_ptr<ShutdownHandler>       shutdown_handler_;
  scoped_ptr<Scheduler>             scheduler_;
  scoped_ptr<SystemMonitor>         system_monitor_;

  friend class CoreUtilsTest;

  DISALLOW_EVIL_CONSTRUCTORS(Core);
};

}  // namespace omaha

#endif  // OMAHA_CORE_CORE_H_
