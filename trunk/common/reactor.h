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
//
// reactor.{h, cc} implements the reactor design pattern.
//
// For each signaled handle the reactor calls back the event handler only once.
// To get further notifications, the handle must be registered again with the
// reactor. This may sound like a lot of work but this is the only way to
// guarantee that the event handler is only called once.
// UnregisterHandle can't be called when handling a callback or it results in
// a deadlock. When handling a callback, the only possible operation is
// registering back a handle using Register.

#ifndef OMAHA_COMMON_REACTOR_H__
#define OMAHA_COMMON_REACTOR_H__

#include <windows.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

class EventHandler;

class Reactor {
 public:
  Reactor();
  ~Reactor();

  // Starts demultiplexing and dispatching events.
  HRESULT HandleEvents();

  // Registers an event handler for a handle. The reactor does not own the
  // handle. Registering the same handle twice results in undefined behavior.
  // The flags parameter can be one of the WT* thread pool values or 0 for
  // a reasonable default.
  HRESULT RegisterHandle(HANDLE handle,
                         EventHandler* event_handler,
                         uint32 flags);

  // Registers the handle again. This method can be called from the callback.
  HRESULT RegisterHandle(HANDLE handle);

  // Unregisters the handle. The method blocks and waits for any callback to
  // complete if an event dispatching is in progress.
  HRESULT UnregisterHandle(HANDLE handle);

 private:
  struct RegistrationState {
    RegistrationState()
        : reactor(NULL),
          event_handler(NULL),
          handle(NULL),
          wait_handle(NULL),
          flags(0) {}

    Reactor*      reactor;
    EventHandler* event_handler;
    HANDLE        handle;
    HANDLE        wait_handle;
    uint32        flags;
  };

  static void __stdcall Callback(void* param, BOOLEAN timer_or_wait);
  void DoCallback(RegistrationState* registration_state);

  HRESULT DoRegisterHandle(HANDLE handle);

  // Releases the ownership of the registration state corresponding to a handle.
  RegistrationState* ReleaseHandlerState(HANDLE handle);

  CRITICAL_SECTION cs_;
  std::vector<RegistrationState*> handlers_;

  DISALLOW_EVIL_CONSTRUCTORS(Reactor);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_REACTOR_H__
