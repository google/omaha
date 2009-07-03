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

#include "omaha/worker/job_observer.h"
#include "omaha/common/error.h"
#include "omaha/common/sta_call.h"
#include "omaha/worker/com_wrapper_shutdown_handler.h"
#include "omaha/worker/ui.h"

namespace omaha {

JobObserverCallMethodDecorator::JobObserverCallMethodDecorator(
    ProgressWnd* job_observer)
      : job_observer_(job_observer),
        progress_wnd_events_(NULL),
        ui_thread_id_(::GetCurrentThreadId()),
        worker_job_thread_id_(0),
        sta_(0) {
  ASSERT1(job_observer_);
}

HRESULT JobObserverCallMethodDecorator::Initialize() {
  job_observer_->SetEventSink(this);
  HRESULT hr = sta_.result();
  if (FAILED(hr)) {
    return hr;
  }

  return job_observer_->Initialize();
}

void JobObserverCallMethodDecorator::OnShow() {
  worker_job_thread_id_ = ::GetCurrentThreadId();

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_, &ProgressWnd::OnShow);
  } else  {
    job_observer_->OnShow();
  }
}

void JobObserverCallMethodDecorator::OnCheckingForUpdate() {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_, &ProgressWnd::OnCheckingForUpdate);
  } else  {
    job_observer_->OnCheckingForUpdate();
  }
}

void JobObserverCallMethodDecorator::OnUpdateAvailable(
    const TCHAR* version_string) {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_,
               &ProgressWnd::OnUpdateAvailable,
               version_string);
  } else  {
    job_observer_->OnUpdateAvailable(version_string);
  }
}

void JobObserverCallMethodDecorator::OnWaitingToDownload() {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_, &ProgressWnd::OnWaitingToDownload);
  } else {
    job_observer_->OnWaitingToDownload();
  }
}

void JobObserverCallMethodDecorator::OnDownloading(int time_remaining_ms,
                                                   int pos) {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_,
               &ProgressWnd::OnDownloading,
               time_remaining_ms,
               pos);
  } else {
    job_observer_->OnDownloading(time_remaining_ms, pos);
  }
}

void JobObserverCallMethodDecorator::OnWaitingToInstall() {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_, &ProgressWnd::OnWaitingToInstall);
  } else {
    job_observer_->OnWaitingToInstall();
  }
}

void JobObserverCallMethodDecorator::OnInstalling() {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_, &ProgressWnd::OnInstalling);
  } else {
    job_observer_->OnInstalling();
  }
}

void JobObserverCallMethodDecorator::OnPause() {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_, &ProgressWnd::OnPause);
  } else {
    job_observer_->OnPause();
  }
}

void JobObserverCallMethodDecorator::OnComplete(CompletionCodes code,
                                                const TCHAR* text,
                                                DWORD error_code) {
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());
  if (!job_observer_) {
    // This method has been called twice.
    return;
  }

  if (ui_thread_id_ != ::GetCurrentThreadId()) {
    CallMethod(job_observer_,
               &ProgressWnd::OnComplete,
               code,
               text,
               error_code);
  } else {
    job_observer_->OnComplete(code, text, error_code);
  }

  // Unhook the observer. We do not unhook the progress_wnd_events_, because the
  // UI can still call restart browsers or close.
  job_observer_ = NULL;
}

void JobObserverCallMethodDecorator::SetEventSink(
    ProgressWndEvents* event_sink) {
  ASSERT1(event_sink);
  progress_wnd_events_ = event_sink;
}


void JobObserverCallMethodDecorator::DoPause() {
  ASSERT1(progress_wnd_events_);
  progress_wnd_events_->DoPause();
}

void JobObserverCallMethodDecorator::DoResume() {
  ASSERT1(progress_wnd_events_);
  progress_wnd_events_->DoResume();
}

void JobObserverCallMethodDecorator::DoClose() {
  if (progress_wnd_events_) {
    progress_wnd_events_->DoClose();
  }
}

void JobObserverCallMethodDecorator::DoRestartBrowsers() {
  ASSERT1(progress_wnd_events_);
  progress_wnd_events_->DoRestartBrowsers();
}

void JobObserverCallMethodDecorator::DoReboot() {
  ASSERT1(progress_wnd_events_);
  progress_wnd_events_->DoReboot();
}

void JobObserverCallMethodDecorator::DoLaunchBrowser(const CString& url) {
  ASSERT1(progress_wnd_events_);
  progress_wnd_events_->DoLaunchBrowser(url);
}

JobObserverCOMDecorator::JobObserverCOMDecorator()
    : job_observer_(NULL),
      progress_wnd_events_(NULL),
      worker_job_thread_id_(0) {
}

JobObserverCOMDecorator::~JobObserverCOMDecorator() {
  Uninitialize();
}

void JobObserverCOMDecorator::Initialize(
    IJobObserver* job_observer,
    WorkerComWrapperShutdownCallBack* call_back) {
  ASSERT1(call_back);
  ASSERT1(job_observer);
  shutdown_callback_ = call_back;
  job_observer_ = job_observer;
  job_observer->SetEventSink(this);
}

void JobObserverCOMDecorator::Uninitialize() {
}

void JobObserverCOMDecorator::OnShow() {
  ASSERT1(job_observer_);
  worker_job_thread_id_ = ::GetCurrentThreadId();

  job_observer_->OnShow();
}

void JobObserverCOMDecorator::OnCheckingForUpdate() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnCheckingForUpdate();
}

void JobObserverCOMDecorator::OnUpdateAvailable(const TCHAR* version_string) {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnUpdateAvailable(version_string);
}

void JobObserverCOMDecorator::OnWaitingToDownload() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnWaitingToDownload();
}

void JobObserverCOMDecorator::OnDownloading(int time_remaining_ms, int pos) {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnDownloading(time_remaining_ms, pos);
}

void JobObserverCOMDecorator::OnWaitingToInstall() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnWaitingToInstall();
}

void JobObserverCOMDecorator::OnInstalling() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnInstalling();
}

void JobObserverCOMDecorator::OnPause() {
  ASSERT1(job_observer_);
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  job_observer_->OnPause();
}

void JobObserverCOMDecorator::OnComplete(CompletionCodes code,
                                         const TCHAR* text,
                                         DWORD) {
  UNREFERENCED_PARAMETER(text);
  if (!job_observer_) {
    return;
  }
  ASSERT1(worker_job_thread_id_ == ::GetCurrentThreadId());

  SetEventSink(NULL);

  // We do not want to send Omaha strings to the application, so pass an empty
  // string instead.
  job_observer_->OnComplete(code, L"");
  job_observer_ = NULL;

  shutdown_callback_->ReleaseIgnoreShutdown();
  shutdown_callback_ = NULL;
}

void JobObserverCOMDecorator::SetEventSink(ProgressWndEvents* event_sink) {
  set_progress_wnd_events(event_sink);
}


STDMETHODIMP JobObserverCOMDecorator::DoPause() {
  ProgressWndEvents* progress_events(progress_wnd_events());
  if (!progress_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  progress_events->DoPause();
  return S_OK;
}

STDMETHODIMP JobObserverCOMDecorator::DoResume() {
  ProgressWndEvents* progress_events(progress_wnd_events());
  if (!progress_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  progress_events->DoResume();
  return S_OK;
}

STDMETHODIMP JobObserverCOMDecorator::DoClose() {
  ProgressWndEvents* progress_events(progress_wnd_events());
  if (!progress_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  progress_events->DoClose();
  return S_OK;
}

STDMETHODIMP JobObserverCOMDecorator::DoRestartBrowsers() {
  ProgressWndEvents* progress_events(progress_wnd_events());
  if (!progress_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  progress_events->DoRestartBrowsers();
  return S_OK;
}

STDMETHODIMP JobObserverCOMDecorator::DoReboot() {
  ProgressWndEvents* progress_events(progress_wnd_events());
  if (!progress_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  progress_events->DoReboot();
  return S_OK;
}

STDMETHODIMP JobObserverCOMDecorator::DoLaunchBrowser(const WCHAR* url) {
  ProgressWndEvents* progress_events(progress_wnd_events());
  if (!progress_events) {
    return GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL;
  }

  progress_events->DoLaunchBrowser(url);
  return S_OK;
}

}  // namespace omaha

