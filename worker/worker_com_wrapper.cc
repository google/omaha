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
#include "omaha/worker/worker_com_wrapper.h"

#include <atlbase.h>
#include <atlstr.h>
#include <atlapp.h>
#include <atlsecurity.h>
#include "omaha/common/error.h"
#include "omaha/common/exception_barrier.h"
#include "omaha/common/scope_guard.h"
#include "omaha/goopdate/google_update.h"
#include "omaha/worker/com_wrapper_shutdown_handler.h"
#include "omaha/worker/worker.h"

namespace omaha {

HRESULT OnDemandCOMClass::FinalConstruct() {
  CORE_LOG(L2, (_T("[OnDemandCOMClass::FinalConstruct]")));

  GoogleUpdate* google_update = static_cast<GoogleUpdate*>(_pAtlModule);
  worker_ = google_update->worker();
  ASSERT1(worker_);

  HRESULT hr = worker_->InitializeThreadPool();
  if (FAILED(hr)) {
    return hr;
  }

  // For update checks on machine applications where the user is not an
  // administrator, the COM server will shutdown automatically after it returns
  // from a COM method invocation. Two reasons for this policy:
  // * Only update checks on a machine app can be done as a non-admin user. To
  //   call Update requires an elevated instance of the COM server. So this COM
  //   server is useless after the UpdateCheck call.
  // * Listening on the shutdown handler requires admin privileges.
  bool shutdown_after_invocation = worker_->is_machine() && !::IsUserAnAdmin();
  worker_->set_shutdown_callback(new WorkerComWrapperShutdownCallBack(
                                         shutdown_after_invocation));

  return shutdown_after_invocation ?
             S_OK :
             worker_->InitializeShutDownHandler(worker_->shutdown_callback());
}

void OnDemandCOMClass::FinalRelease() {
  CORE_LOG(L2, (_T("[OnDemandCOMClass::FinalRelease]")));
  worker_ = NULL;
}

void OnDemandCOMClass::AddRefIgnoreShutdownEvent() const {
  CORE_LOG(L2, (_T("[OnDemandCOMClass::AddRefIgnoreShutdownEvent]")));
  ASSERT1(worker_);
  WorkerComWrapperShutdownCallBack* callback = worker_->shutdown_callback();
  ASSERT1(callback);
  callback->AddRefIgnoreShutdown();
}

void OnDemandCOMClass::ResetStateOnError() const {
  CORE_LOG(L2, (_T("[OnDemandCOMClass::ResetStateOnError]")));
  ASSERT1(worker_);
  WorkerComWrapperShutdownCallBack* callback = worker_->shutdown_callback();
  ASSERT1(callback);
  callback->ReleaseIgnoreShutdown();
}

HRESULT OnDemandCOMClass::DoOnDemand(bool is_update_check_only,
                                     const WCHAR* guid,
                                     IJobObserver* observer) {
  CORE_LOG(L2, (_T("[OnDemandCOMClass::DoOnDemand][%d][%s][%d]"),
                is_update_check_only, guid, observer));
  // The exception barrier is needed, because any exceptions that are thrown
  // in this method will get caught by the COM run time. We compile with
  // exceptions off, and do not expect to throw any exceptions. This barrier
  // will treat an exception in this method as a unhandled exception.
  ExceptionBarrier barrier;

  ASSERT1(guid);
  ASSERT1(observer);
  if (!guid || StringToGuid(guid) == GUID_NULL || !observer) {
    return E_INVALIDARG;
  }

  AddRefIgnoreShutdownEvent();
  // Ensure that the reset method is called in all error cases.
  ScopeGuard guard = MakeObjGuard(*this, &OnDemandCOMClass::ResetStateOnError);

  HRESULT hr = worker_->DoOnDemand(guid,
                                   CString(),
                                   observer,
                                   is_update_check_only);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[DoOnDemand failed][0x%08x]"), hr));
    return hr;
  }

  // Dismiss the scope guard, since the DoOnDemand succeeded and the worker job
  // thread is now responsible for the cleanup.
  guard.Dismiss();
  return S_OK;
}

STDMETHODIMP OnDemandCOMClass::Update(const WCHAR* guid,
                                      IJobObserver* observer) {
  return DoOnDemand(false, guid, observer);
}

STDMETHODIMP OnDemandCOMClass::CheckForUpdate(const WCHAR* guid,
                                              IJobObserver* observer) {
  return DoOnDemand(true, guid, observer);
}

}  // namespace omaha

