// Copyright 2008-2010 Google Inc.
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

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/synchronized.h"
#include "omaha/goopdate/job_observer.h"

namespace omaha {

JobObserverCOMDecorator::JobObserverCOMDecorator()
    : job_observer_(NULL),
      on_demand_events_(NULL),
      worker_job_thread_id_(::GetCurrentThreadId()) {
}

JobObserverCOMDecorator::~JobObserverCOMDecorator() {
  Uninitialize();
}

void JobObserverCOMDecorator::Initialize(IJobObserver* job_observer) {
  ASSERT1(job_observer);

  job_observer_ = job_observer;
  job_observer->SetEventSink(this);

  job_observer_.QueryInterface(&job_observer2_);
}

void JobObserverCOMDecorator::Uninitialize() {
}

void JobObserverCOMDecorator::OnCheckingForUpdate() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnCheckingForUpdate();
}

void JobObserverCOMDecorator::OnUpdateAvailable(const CString& app_id,
                                                const CString& app_name,
                                                const CString& version_string) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnUpdateAvailable(version_string);
}

void JobObserverCOMDecorator::OnWaitingToDownload(const CString& app_id,
                                                  const CString& app_name) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnWaitingToDownload();
}

void JobObserverCOMDecorator::OnDownloading(const CString& app_id,
                                            const CString& app_name,
                                            int time_remaining_ms,
                                            int pos) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnDownloading(time_remaining_ms, pos);
}

void JobObserverCOMDecorator::OnWaitingRetryDownload(const CString& app_id,
                                                     const CString& app_name,
                                                     time64 next_retry_time) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  UNREFERENCED_PARAMETER(next_retry_time);
}

void JobObserverCOMDecorator::OnWaitingToInstall(const CString& app_id,
                                                 const CString& app_name,
                                                 bool* can_start_install) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnWaitingToInstall();
  *can_start_install = true;
}

void JobObserverCOMDecorator::OnInstalling(const CString& app_id,
                                           const CString& app_name,
                                           int time_remaining_ms,
                                           int pos) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  UNREFERENCED_PARAMETER(time_remaining_ms);
  UNREFERENCED_PARAMETER(pos);
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (job_observer2_) {
    job_observer2_->OnInstalling2(time_remaining_ms, pos);
  } else {
    job_observer_->OnInstalling();
  }
}

void JobObserverCOMDecorator::OnPause() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnPause();
}

void JobObserverCOMDecorator::OnComplete(const ObserverCompletionInfo& info) {
  if (job_observer_) {
    ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

    LegacyCompletionCodes completion_code =
        static_cast<LegacyCompletionCodes>(info.completion_code);

    // The completion_text will be in the "lang" specified in ClientState or
    // Clients, or the current locale if "lang" is missing.
    job_observer_->OnComplete(completion_code, info.completion_text);
    job_observer_ = NULL;
  }

  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (on_demand_events) {
    on_demand_events->DoExit();
    SetEventSink(NULL);
  }

  // The message loop for OnDemand is on a separate thread, which needs to be
  // signaled to exit.
  ::PostQuitMessage(0);
}

void JobObserverCOMDecorator::SetEventSink(
    OnDemandEventsInterface* event_sink) {
  set_job_events(event_sink);
}

// TODO(omaha): Need to add a DoPause() to OnDemandEventsInterface. We never
// expect these to be used since Chrome never did.
STDMETHODIMP JobObserverCOMDecorator::DoPause() {
  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (!on_demand_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  return E_NOTIMPL;
}

// TODO(omaha): Need to add a DoResume() to OnDemandEventsInterface.
STDMETHODIMP JobObserverCOMDecorator::DoResume() {
  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (!on_demand_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  return E_NOTIMPL;
}

STDMETHODIMP JobObserverCOMDecorator::DoClose() {
  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (!on_demand_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  on_demand_events->DoClose();
  return S_OK;
}

// TODO(omaha): Reconcile IJobObserver::DoRestartBrowsers() with
// OnDemandEventsInterface::DoRestartBrowser().
STDMETHODIMP JobObserverCOMDecorator::DoRestartBrowsers() {
  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (!on_demand_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  return E_NOTIMPL;
}

STDMETHODIMP JobObserverCOMDecorator::DoReboot() {
  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (!on_demand_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  return E_NOTIMPL;
}

STDMETHODIMP JobObserverCOMDecorator::DoLaunchBrowser(const WCHAR* url) {
  UNREFERENCED_PARAMETER(url);
  OnDemandEventsInterface* on_demand_events =  GetOnDemandEvents();
  if (!on_demand_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  return E_NOTIMPL;
}

}  // namespace omaha
