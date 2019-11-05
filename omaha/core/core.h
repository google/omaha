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
#include <memory>
#include <string>

#include "base/basictypes.h"
#include "omaha/base/shutdown_callback.h"
#include "omaha/core/google_update_core.h"
#include "omaha/core/scheduler.h"
#include "omaha/core/system_monitor.h"
#include "omaha/goopdate/google_update3.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

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

  bool is_system() const { return is_system_; }

  virtual LONG Unlock() throw() {
    // CAtlExeModuleT::Unlock() posts a WM_QUIT message if the ATL module count
    // drops to zero. The Core needs to keep running when either of these
    // conditions are true:
    // * Shutdown event is not signaled
    // * ATL module count is non-zero
    // Because CAtlExeModuleT::Unlock() only transitions on the latter condition
    // we bypass it and call on CAtlModuleT<Core>::Unlock() instead which just
    // decrements the module count.
    // The shutdown logic for the Core is handled in ShutdownInternal().
    return CAtlModuleT<Core>::Unlock();
  }

  static HRESULT StartCoreIfNeeded(bool is_system);

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

  bool ShouldRunCodeRed() const;
  HRESULT UpdateLastCodeRedCheckTime() const;

  bool HasOSUpgraded() const;
  CString GetOSUpgradeVersionsString() const;
  void LaunchAppCommandsOnOSUpgrade() const;

  // Collects ambient core metrics.
  void CollectMetrics()const;

  HRESULT InitializeScheduler(const Scheduler* scheduler);

  bool is_system_;

  // True if the core has to kickoff the crash handler.
  bool is_crash_handler_enabled_;

  DWORD main_thread_id_;        // The id of the thread that runs Core::Main.

  friend class CoreUtilsTest;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

}  // namespace omaha

#endif  // OMAHA_CORE_CORE_H_
