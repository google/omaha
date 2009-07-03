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

#include "omaha/common/shutdown_handler.h"

#include <atlsecurity.h>
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reactor.h"
#include "omaha/common/shutdown_callback.h"
#include "omaha/common/utils.h"

namespace omaha {

ShutdownHandler::ShutdownHandler()
  : shutdown_callback_(NULL) {
}

ShutdownHandler::~ShutdownHandler() {
  if (get(shutdown_event_)) {
    VERIFY1(SUCCEEDED(reactor_->UnregisterHandle(get(shutdown_event_))));
  }
}

HRESULT ShutdownHandler::Initialize(Reactor* reactor,
                                    ShutdownCallback* shutdown,
                                    bool is_machine) {
  ASSERT1(reactor);
  ASSERT1(shutdown);
  shutdown_callback_ = shutdown;
  reactor_ = reactor;
  is_machine_ = is_machine;

  NamedObjectAttributes attr;
  GetNamedObjectAttributes(kShutdownEvent, is_machine_, &attr);
  // Manual reset=true and signaled=false
  reset(shutdown_event_, ::CreateEvent(&attr.sa, true, false, attr.name));
  if (!shutdown_event_) {
    return HRESULTFromLastError();
  }

  HRESULT hr = reactor_->RegisterHandle(get(shutdown_event_), this, 0);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

void ShutdownHandler::HandleEvent(HANDLE handle) {
  if (handle == get(shutdown_event_)) {
    CORE_LOG(L1, (_T("[shutdown event is signaled]")));
  } else {
    ASSERT1(false);
  }
  ASSERT1(shutdown_callback_);
  shutdown_callback_->Shutdown();
}

}  // namespace omaha

