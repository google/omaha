// Copyright 2008-2009 Google Inc.
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
// The WorkerComWrapperShutdownCallBack implements the shutdown
// handler for OnDemandUpdates. The class posts a WM_QUIT message
// to the main thread. In case of the OnDemandUpdates this
// main thread loop is running a message loop provided by
// CAtlExeModuleT::RunMessageLoop().

#include "omaha/worker/com_wrapper_shutdown_handler.h"

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/synchronized.h"
#include "omaha/goopdate/google_update.h"

namespace omaha {

int WorkerComWrapperShutdownCallBack::ReleaseIgnoreShutdown() {
  int release_count = InternalRelease();
  if (!release_count && shutdown_on_final_release_) {
    VERIFY1(SUCCEEDED(Shutdown()));
  }

  return release_count;
}

HRESULT WorkerComWrapperShutdownCallBack::Shutdown() {
  if (ShouldIgnoreShutdown()) {
    return S_OK;
  }

  GoogleUpdate* google_update = static_cast<GoogleUpdate*>(_pAtlModule);
  ASSERT1(google_update);
  if (!::PostThreadMessage(google_update->m_dwMainThreadID, WM_QUIT, 0, 0)) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[PostThreadMessage failed][0x%08x]"), hr));
    return hr;
  }
  return S_OK;
}

}  // namespace omaha

