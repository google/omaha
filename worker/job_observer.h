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

#ifndef OMAHA_WORKER_JOB_OBSERVER_H__
#define OMAHA_WORKER_JOB_OBSERVER_H__

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/sta.h"

namespace omaha {

class ProgressWnd;
class WorkerComWrapperShutdownCallBack;

// The UI generates events when interesting things happens with it,
// mostly due to the user clicking on controls.
class ProgressWndEvents {
 public:
  virtual ~ProgressWndEvents() {}
  virtual void DoPause() = 0;
  virtual void DoResume() = 0;
  virtual void DoClose() = 0;
  virtual void DoRestartBrowsers() = 0;
  virtual void DoReboot() = 0;
  virtual void DoLaunchBrowser(const CString& url) = 0;
};

class JobObserver {
 public:
  virtual ~JobObserver() {}
  virtual void OnShow() = 0;
  virtual void OnCheckingForUpdate() = 0;
  virtual void OnUpdateAvailable(const TCHAR* version_string) = 0;
  virtual void OnWaitingToDownload() = 0;
  virtual void OnDownloading(int time_remaining_ms, int pos) = 0;
  virtual void OnWaitingToInstall() = 0;
  virtual void OnInstalling() = 0;
  virtual void OnPause() = 0;
  virtual void OnComplete(CompletionCodes code,
                          const TCHAR* text,
                          DWORD error_code) = 0;
  virtual void SetEventSink(ProgressWndEvents* event_sink) = 0;
  virtual void Uninitialize() = 0;
};

// Class that delegates Job progress calls to the UI, and delegates UI events
// to the Job. Handles the case where UI thread is different than the calling
// thread.
class JobObserverCallMethodDecorator
    : public JobObserver,
      public ProgressWndEvents {
 public:
  explicit JobObserverCallMethodDecorator(ProgressWnd* observer);
  virtual ~JobObserverCallMethodDecorator() {}
  HRESULT Initialize();

  // JobObserver implementation.
  virtual void OnShow();
  virtual void OnCheckingForUpdate();
  virtual void OnUpdateAvailable(const TCHAR* version_string);
  virtual void OnWaitingToDownload();
  virtual void OnDownloading(int time_remaining_ms, int pos);
  virtual void OnWaitingToInstall();
  virtual void OnInstalling();
  virtual void OnPause();
  virtual void OnComplete(CompletionCodes code,
                          const TCHAR* text,
                          DWORD error_code);
  virtual void SetEventSink(ProgressWndEvents* event_sink);
  virtual void Uninitialize() {}

  // ProgressWndEvents implementation.
  virtual void DoPause();
  virtual void DoResume();
  virtual void DoClose();
  virtual void DoRestartBrowsers();
  virtual void DoReboot();
  virtual void DoLaunchBrowser(const CString& url);

 private:
  scoped_sta sta_;
  ProgressWnd* job_observer_;
  ProgressWndEvents* progress_wnd_events_;
  DWORD ui_thread_id_;
  DWORD worker_job_thread_id_;
};

class JobObserverCOMDecorator
  : public CComObjectRootEx<CComMultiThreadModel>,
    public JobObserver,
    public IProgressWndEvents {
 public:
  BEGIN_COM_MAP(JobObserverCOMDecorator)
    COM_INTERFACE_ENTRY(IProgressWndEvents)
  END_COM_MAP()

  JobObserverCOMDecorator();
  virtual ~JobObserverCOMDecorator();
  void Initialize(IJobObserver* job_observer,
                  WorkerComWrapperShutdownCallBack* call_back);

  // JobObserver implementation.
  virtual void OnShow();
  virtual void OnCheckingForUpdate();
  virtual void OnUpdateAvailable(const TCHAR* version_string);
  virtual void OnWaitingToDownload();
  virtual void OnDownloading(int time_remaining_ms, int pos);
  virtual void OnWaitingToInstall();
  virtual void OnInstalling();
  virtual void OnPause();
  virtual void OnComplete(CompletionCodes code,
                          const TCHAR* text,
                          DWORD error_code);
  virtual void SetEventSink(ProgressWndEvents* event_sink);
  void Uninitialize();

  // IProgressWndEvents.
  STDMETHOD(DoPause)();
  STDMETHOD(DoResume)();
  STDMETHOD(DoClose)();
  STDMETHOD(DoRestartBrowsers)();
  STDMETHOD(DoReboot)();
  STDMETHOD(DoLaunchBrowser)(const WCHAR* url);

 private:
  // The DoXXX() methods can be called from multiple COM threads since the
  // code is running in an MTA. Write access to progress_wnd_events_ member
  // must be atomic.
  // The OnXXX() calls on the other hand are always called from a single
  // thread, so no synchronization is needed.
  ProgressWndEvents* progress_wnd_events() {
    return progress_wnd_events_;
  }

  void set_progress_wnd_events(ProgressWndEvents* progress_wnd_events) {
    // InterlockedExchangePointer is broken due to ATL defining a function with
    // the same name in the global namespace and hiding the Win32 API.
    // InterlockedExchange introduces a full memory barrier.
    ::InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&progress_wnd_events_),
        reinterpret_cast<LONG>(progress_wnd_events));
  }

  CComPtr<IJobObserver> job_observer_;
  ProgressWndEvents* volatile progress_wnd_events_;
  WorkerComWrapperShutdownCallBack* shutdown_callback_;
  DWORD thread_id_;
  DWORD worker_job_thread_id_;
};

}  // namespace omaha

#endif  // OMAHA_WORKER_JOB_OBSERVER_H__

