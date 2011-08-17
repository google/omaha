// Copyright 2010 Google Inc.
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

#include "omaha/goopdate/ondemand.h"
#include "omaha/base/debug.h"
#include "omaha/client/bundle_installer.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/lang.h"
#include "omaha/common/update3_utils.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/goopdate/job_observer.h"

namespace omaha {

namespace internal {

namespace {

HRESULT CreateJobObserverForOnDemand(DWORD job_observer_git_cookie,
                                     JobObserverCOMDecorator** job_observer) {
  ASSERT1(job_observer);
  *job_observer = NULL;

  CComGITPtr<IJobObserver> job_observer_git(job_observer_git_cookie);
  CComPtr<IJobObserver> ijob_observer;
  HRESULT hr = job_observer_git.CopyTo(&ijob_observer);
  ASSERT1(ijob_observer);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[job_observer_git.CopyTo failed][0x%x]"), hr));
    return hr;
  }

  CComObject<JobObserverCOMDecorator>* job_observer_com = NULL;
  hr = CComObject<JobObserverCOMDecorator>::CreateInstance(&job_observer_com);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[JobObserverCOMDecorator creation failed][0x%x]"), hr));
    return hr;
  }

  job_observer_com->Initialize(ijob_observer);
  job_observer_com->AddRef();
  *job_observer = job_observer_com;
  return S_OK;
}

}  // namespace

HRESULT DoOnDemand(bool is_machine, OnDemandParameters on_demand_params) {
  CComPtr<JobObserverCOMDecorator> job_observer;
  HRESULT hr = internal::CreateJobObserverForOnDemand(
      on_demand_params.job_observer_git_cookie, &job_observer);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateJobObserver failed][0x%x]"), hr));
    return hr;
  }

  return UpdateAppOnDemand(is_machine,
                           on_demand_params.app_id,
                           on_demand_params.is_update_check_only,
                           on_demand_params.session_id,
                           on_demand_params.impersonation_token,
                           on_demand_params.primary_token,
                           job_observer);
}

}  // namespace internal

}  // namespace omaha
