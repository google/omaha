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


#ifndef OMAHA_WORKER_I_JOB_OBSERVER_MOCK_H_
#define OMAHA_WORKER_I_JOB_OBSERVER_MOCK_H_

#pragma once
#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "goopdate\google_update_idl.h"  // NOLINT
#include "omaha/worker/job_observer_mock.h"

namespace omaha {

// A basic implementation of the IJobObserver interface for use by unit tests.
// Wraps JobObserverMock.
class IJobObserverMock
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IJobObserver {
 public:
  BEGIN_COM_MAP(IJobObserverMock)
    COM_INTERFACE_ENTRY(IJobObserver)
  END_COM_MAP()

  virtual ~IJobObserverMock() {
  }

  // IJobObserver implementation.
  STDMETHOD(OnShow)() {
    job_observer_mock.OnShow();
    return S_OK;
  }

  STDMETHOD(OnCheckingForUpdate)() {
    job_observer_mock.OnCheckingForUpdate();
    return S_OK;
  }

  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    UNREFERENCED_PARAMETER(version_string);
    return S_OK;
  }

  STDMETHOD(OnWaitingToDownload)() {
    job_observer_mock.OnWaitingToDownload();
    return S_OK;
  }

  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    job_observer_mock.OnDownloading(time_remaining_ms, pos);
    return S_OK;
  }

  STDMETHOD(OnWaitingToInstall)() {
    job_observer_mock.OnWaitingToInstall();
    return S_OK;
  }

  STDMETHOD(OnInstalling)() {
    job_observer_mock.OnInstalling();
    return S_OK;
  }

  STDMETHOD(OnPause)() {
    job_observer_mock.OnPause();
    return S_OK;
  }

  // The COM API does not have an error_code parameter. Pass unexpected error.
  STDMETHOD(OnComplete)(CompletionCodes code, const TCHAR* text) {
    job_observer_mock.OnComplete(code, text, static_cast<DWORD>(E_UNEXPECTED));
    ::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);
    return S_OK;
  }

  // JobObserverMock uses a different sink type, so cannot pass event_sink.
  // For now, there are not tests that need the sink, so do nothing.
  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    UNREFERENCED_PARAMETER(event_sink);
    return S_OK;
  }

  // Stores data about events that tests can check.
  JobObserverMock job_observer_mock;
};

}  // namespace omaha

#endif  // OMAHA_WORKER_I_JOB_OBSERVER_MOCK_H_
