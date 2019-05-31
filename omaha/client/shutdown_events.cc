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

#include "omaha/client/shutdown_events.h"

#include <atlsafe.h>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/reactor.h"
#include "omaha/base/shutdown_handler.h"
#include "omaha/base/synchronized.h"
#include "omaha/client/bundle_installer.h"

namespace omaha {

ShutdownEvents::ShutdownEvents(BundleInstaller* installer)
    : installer_(installer) {
  ASSERT1(installer);
}

ShutdownEvents::~ShutdownEvents() {
  CORE_LOG(L2, (_T("[ShutdownEvents::~ShutdownEvents]")));
  __mutexScope(lock_);

  CORE_LOG(L2, (_T("[Destroying shutdown handler]")));
  shutdown_handler_.reset();
  reactor_.reset();
}

HRESULT ShutdownEvents::InitializeShutdownHandler(bool is_machine) {
  CORE_LOG(L3, (_T("[InitializeShutdownHandler]")));

  reactor_.reset(new Reactor);
  shutdown_handler_.reset(new ShutdownHandler);
  return shutdown_handler_->Initialize(reactor_.get(), this, is_machine);
}

// Shutdown() is called from a thread in the OS threadpool. The PostMessage
// marshals the call over to the UI thread, which is where DoClose needs to be
// (and is) called from.
HRESULT ShutdownEvents::Shutdown() {
  CORE_LOG(L2, (_T("[Shutdown]")));

  __mutexScope(lock_);

  CORE_LOG(L2, (_T("[Shutdown][IsWindow: %d]"), installer_->IsWindow()));
  if (installer_->IsWindow()) {
    installer_->PostMessage(WM_CLOSE, 0, 0);
  }

  return S_OK;
}

HRESULT ShutdownEvents::CreateShutdownHandler(
    bool is_machine,
    BundleInstaller* installer,
    std::unique_ptr<ShutdownCallback>* shutdown_callback) {
  ASSERT1(installer);
  ASSERT1(shutdown_callback);

  auto shutdown_events = std::make_unique<ShutdownEvents>(installer);
  HRESULT hr = shutdown_events->InitializeShutdownHandler(is_machine);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[InitializeShutDownHandler failed][0x%08x]"), hr));
    return hr;
  }

  shutdown_callback->reset(shutdown_events.release());
  return S_OK;
}

}  // namespace omaha
