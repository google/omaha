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

// TODO(omaha3): Rename the files and class to ondemand_com_decorator.* and
// OnDemandCOMDecorator, respectively.

#ifndef OMAHA_GOOPDATE_JOB_OBSERVER_H_
#define OMAHA_GOOPDATE_JOB_OBSERVER_H_

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>

#include "omaha/base/utils.h"
#include "omaha/client/install_apps.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

class JobObserverCOMDecorator
  : public CComObjectRootEx<CComMultiThreadModel>,
    public OnDemandObserver,
    public IProgressWndEvents {
 public:
  BEGIN_COM_MAP(JobObserverCOMDecorator)
    COM_INTERFACE_ENTRY(IProgressWndEvents)
  END_COM_MAP()

  JobObserverCOMDecorator();
  virtual ~JobObserverCOMDecorator();
  void Initialize(IJobObserver* job_observer);

  // OnDemandObserver implementation.
  virtual void OnCheckingForUpdate();
  virtual void OnUpdateAvailable(const CString& app_id,
                                 const CString& app_name,
                                 const CString& version_string);
  virtual void OnWaitingToDownload(const CString& app_id,
                                   const CString& app_name);
  virtual void OnDownloading(const CString& app_id,
                             const CString& app_name,
                             int time_remaining_ms,
                             int pos);
  virtual void OnWaitingRetryDownload(const CString& app_id,
                                      const CString& app_name,
                                      time64 next_retry_time);
  virtual void OnWaitingToInstall(const CString& app_id,
                                  const CString& app_name,
                                  bool* can_start_install);
  virtual void OnInstalling(const CString& app_id,
                            const CString& app_name,
                            int time_remaining_ms,
                            int pos);
  virtual void OnPause();
  virtual void OnComplete(const ObserverCompletionInfo& observer_info);
  virtual void SetEventSink(OnDemandEventsInterface* event_sink);

  void Uninitialize();

  // IProgressWndEvents.
  STDMETHOD(DoPause)();
  STDMETHOD(DoResume)();
  STDMETHOD(DoClose)();
  STDMETHOD(DoRestartBrowsers)();
  STDMETHOD(DoReboot)();
  STDMETHOD(DoLaunchBrowser)(const WCHAR* url);

 private:
  OnDemandEventsInterface* GetOnDemandEvents() {
    return on_demand_events_;
  }

  // Since the code is running in an MTA, write access to
  // on_demand_events_ must be atomic.
  void set_job_events(OnDemandEventsInterface* on_demand_events) {
    // InterlockedExchangePointer is broken due to ATL defining a function with
    // the same name in the global namespace and hiding the Win32 API.
    // InterlockedExchange introduces a full memory barrier.
    interlocked_exchange_pointer(&on_demand_events_, on_demand_events);
  }

  CComPtr<IJobObserver> job_observer_;
  CComPtr<IJobObserver2> job_observer2_;
  OnDemandEventsInterface* volatile on_demand_events_;
  DWORD thread_id_;
  DWORD worker_job_thread_id_;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_JOB_OBSERVER_H_
