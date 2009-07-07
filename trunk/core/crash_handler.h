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

#ifndef OMAHA_CORE_CRASH_HANDLER_H_
#define OMAHA_CORE_CRASH_HANDLER_H_

#include <windows.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/shutdown_callback.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/crash_generation/crash_generation_server.h"

namespace omaha {

class Reactor;
class ShutdownHandler;

class CrashHandler : public ShutdownCallback {
 public:
  CrashHandler();
  virtual ~CrashHandler();

  // Executes the instance entry point with given parameters.
  HRESULT Main(bool is_system);

  Reactor* reactor() const { return reactor_.get(); }
  bool is_system() const { return is_system_; }

 private:
  typedef google_breakpad::CrashGenerationServer CrashGenerationServer;

  // ShutdownCallback interface.
  // Signals the CrashHandler to stop handling events and exit.
  virtual HRESULT Shutdown();

  HRESULT DoRun();
  HRESULT DoHandleEvents();

  bool is_system_;

  DWORD main_thread_id_;  // The id of the thread that runs CrashHandler::Main.

  scoped_ptr<Reactor>               reactor_;
  scoped_ptr<ShutdownHandler>       shutdown_handler_;

  DISALLOW_EVIL_CONSTRUCTORS(CrashHandler);
};

}  // namespace omaha

#endif  // OMAHA_CORE_CRASH_HANDLER_H_

