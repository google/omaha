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


#include "omaha/common/reactor.h"

#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/event_handler.h"

namespace omaha {

Reactor::Reactor() {
  CORE_LOG(L4, (_T("[Reactor::Reactor]")));
  ::InitializeCriticalSection(&cs_);
}

Reactor::~Reactor() {
  CORE_LOG(L4, (_T("[Reactor::~Reactor]")));

  // Each handle must be unregistered before destroying the reactor.
  ASSERT1(handlers_.empty());
  ::DeleteCriticalSection(&cs_);
}

// The reactor loop is just an efficient wait, as the demultiplexing of the
// events is actually done by the OS thread pool.
// TODO(omaha): replace the alertable wait with waiting on an event and provide
// a method for the reactor to stop handling events.
HRESULT Reactor::HandleEvents() {
  CORE_LOG(L1, (_T("[Reactor::HandleEvents]")));
  VERIFY1(::SleepEx(INFINITE, true) == WAIT_IO_COMPLETION);
  CORE_LOG(L1, (_T("[Reactor::HandleEvents exit]")));
  return S_OK;
}

void __stdcall Reactor::Callback(void* param, BOOLEAN timer_or_wait) {
  ASSERT1(param);

  // Since we wait an INFINITE the wait handle is always signaled.
  VERIFY1(!timer_or_wait);
  RegistrationState* state = static_cast<RegistrationState*>(param);
  ASSERT1(state->reactor);
  state->reactor->DoCallback(state);
}

// Method does not check to see if the same handle is registered twice.
HRESULT Reactor::RegisterHandle(HANDLE handle,
                                EventHandler* event_handler,
                                uint32 flags) {
  ASSERT1(handle);
  ASSERT1(event_handler);

  if (!handle || !event_handler) {
    return E_INVALIDARG;
  }

  scoped_ptr<RegistrationState> state(new RegistrationState);
  state->event_handler = event_handler;
  state->handle = handle;
  state->reactor = this;
  state->flags = flags | WT_EXECUTEONLYONCE;

  // The reactor only calls the handler once.
  ASSERT1(WT_EXECUTEDEFAULT == 0);

  // As soon as the handle is registered, the thread pool can queue up a
  // callback and reenter the reactor on a different thread.
  // Acquire the critical section before registering the handle.
  ::EnterCriticalSection(&cs_);
#if DEBUG
  // The same handle should not be registered multiple times.
  std::vector<RegistrationState*>::iterator it = handlers_.begin();
  for (; it != handlers_.end(); ++it) {
    ASSERT((*it)->handle != handle, (_T("[already registered %d]"), handle));
  }
#endif
  bool res = !!::RegisterWaitForSingleObject(&state->wait_handle,
                                             state->handle,
                                             &Reactor::Callback,
                                             state.get(),
                                             INFINITE,
                                             state->flags);
  HRESULT hr = res ? S_OK : HRESULTFromLastError();
  if (SUCCEEDED(hr)) {
    handlers_.push_back(state.release());
  }
  ::LeaveCriticalSection(&cs_);

  return hr;
}

HRESULT Reactor::RegisterHandle(HANDLE handle) {
  ::EnterCriticalSection(&cs_);
  HRESULT hr = DoRegisterHandle(handle);
  ::LeaveCriticalSection(&cs_);
  return hr;
}

HRESULT Reactor::DoRegisterHandle(HANDLE handle) {
  ASSERT1(handle);
  std::vector<RegistrationState*>::iterator it = handlers_.begin();
  for (; it != handlers_.end(); ++it) {
    if ((*it)->handle == handle) {
      break;
    }
  }
  if (it == handlers_.end()) {
    // The handle is not registered with the reactor anymore. Registering the
    // the handle again is not possible.
    return E_FAIL;
  }

  // Unregister and register the handle again. Unregistering is an non blocking
  // call.
  RegistrationState* state = *it;
  bool res = !!::UnregisterWaitEx(state->wait_handle, NULL);
  if (!res && ::GetLastError() != ERROR_IO_PENDING) {
    return HRESULTFromLastError();
  }
  if (!::RegisterWaitForSingleObject(&state->wait_handle,
                                     state->handle,
                                     &Reactor::Callback,
                                     state,
                                     INFINITE,
                                     state->flags)) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

HRESULT Reactor::UnregisterHandle(HANDLE handle) {
  ASSERT1(handle);
  if (!handle) {
    return E_INVALIDARG;
  }

  // Attempts to take the ownership of the registration state for the handle.
  // If taking the ownership does not succeed, it means the handle has already
  // been unregistered.
  scoped_ptr<RegistrationState> state(ReleaseHandlerState(handle));
  if (!state.get()) {
    return E_UNEXPECTED;
  }

  // Unregisters the wait handle from the thread pool. The call blocks waiting
  // for any pending callbacks to finish. No lock is being held while waiting
  // here. If there is no callback pending, the call will succeed right away.
  // Otherwise, if a callback has already started, the call waits for the
  // callback to complete.
  bool res = !!::UnregisterWaitEx(state->wait_handle, INVALID_HANDLE_VALUE);

  // Clear the registration state, as a defensive programming measure and
  // for debugging purposes.
  state->reactor          = NULL;
  state->handle           = NULL;
  state->wait_handle      = NULL;
  state->event_handler    = NULL;

  return res ? S_OK : HRESULTFromLastError();
}

void Reactor::DoCallback(RegistrationState* state) {
  ASSERT1(state);
  ASSERT1(state->event_handler);
  ASSERT1(state->handle);
  state->event_handler->HandleEvent(state->handle);
}

// Looks up the registration state for a handle and releases the ownership
// of it to the caller. As the clean up of the state can happen from multiple
// places, the transfer of ownership ensures the clean up happens once and
// only once.
Reactor::RegistrationState* Reactor::ReleaseHandlerState(HANDLE handle) {
  RegistrationState* registration_state = NULL;
  ::EnterCriticalSection(&cs_);
  std::vector<RegistrationState*>::iterator it = handlers_.begin();
  for (; it != handlers_.end(); ++it) {
    if ((*it)->handle == handle) {
      registration_state = *it;
      handlers_.erase(it);
      break;
    }
  }
  ::LeaveCriticalSection(&cs_);
  return registration_state;
}

}  // namespace omaha

