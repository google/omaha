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


#ifndef OMAHA_TOOLS_SRC_PERFORMONDEMAND_PERFORMONDEMAND_H_
#define OMAHA_TOOLS_SRC_PERFORMONDEMAND_PERFORMONDEMAND_H_

#pragma once
#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "goopdate\google_update_idl.h"  // NOLINT

// Keep this list syncronized with qa\client\lib\on_demand_lib.py
#define ON_COMPLETE_SUCCESS                           0x00000001
#define ON_COMPLETE_SUCCESS_CLOSE_UI                  0x00000002
#define ON_COMPLETE_ERROR                             0x00000004
#define ON_COMPLETE_RESTART_ALL_BROWSERS              0x00000008
#define ON_COMPLETE_REBOOT                            0x00000010
#define ON_SHOW                                       0x00000020
#define ON_CHECKING_FOR_UPDATES                       0x00000040
#define ON_UPDATE_AVAILABLE                           0x00000080
#define ON_WAITING_TO_DOWNLOAD                        0x00000100
#define ON_DOWNLOADING                                0x00000200
#define ON_WAITING_TO_INSTALL                         0x00000400
#define ON_INSTALLING                                 0x00000800
#define ON_PAUSE                                      0x00001000
#define SET_EVENT_SINK                                0x00002000
#define ON_COMPLETE_RESTART_BROWSER                   0x00004000
#define ON_COMPLETE_RESTART_ALL_BROWSERS_NOTICE_ONLY  0x00008000
#define ON_COMPLETE_REBOOT_NOTICE_ONLY                0x00010000
#define ON_COMPLETE_RESTART_BROWSER_NOTICE_ONLY       0x00020000
#define ON_COMPLETE_RUN_COMMAND                       0x00040000

class JobObserver
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IJobObserver {
 public:
  BEGIN_COM_MAP(JobObserver)
    COM_INTERFACE_ENTRY(IJobObserver)
  END_COM_MAP()

  // Each interaction enables a bit in observed, which is eventually returned as
  // a return code.
  int observed;

  // Similar to observed, misbehave_modes_ and close_modes_ take on bits from
  // the list of all events.  For example, if close_modes_ | ON_DOWNLOADING
  // is true, then when ON_DOWNLOADING is called, DoClose will be called.
  int misbehave_modes_;
  int close_modes_;
  bool do_closed_called;

  JobObserver()
    : observed(0), misbehave_modes_(0), close_modes_(0),
      do_closed_called(false) {
    wprintf(L"JobObserver\n");
  }
  virtual ~JobObserver() {
    wprintf(L"~JobObserver\n");
  }

  void Reset() {
    observed = 0;
    misbehave_modes_ = 0;
    close_modes_ = 0;
    do_closed_called = false;
  }

  void AddMisbehaveMode(int event_code) {
    misbehave_modes_ |= event_code;
  }

  void AddCloseMode(int event_code) {
    close_modes_ |= event_code;
  }

  HRESULT HandleEvent(int event_code) {
    observed |= event_code;

    if ((event_code & close_modes_) && !do_closed_called) {
      wprintf(L"Calling DoClose()\n");
      do_closed_called = true;
      event_sink_->DoClose();
    }

    if (event_code & misbehave_modes_) {
      wprintf(L"Misbehaving\n");
      return E_FAIL;
    } else {
      return S_OK;
    }
  }

  // JobObserver implementation.
  STDMETHOD(OnShow)() {
    wprintf(L"OnShow\n");
    return HandleEvent(ON_SHOW);
  }
  STDMETHOD(OnCheckingForUpdate)() {
    wprintf(L"OnCheckingForUpdate\n");
    return HandleEvent(ON_CHECKING_FOR_UPDATES);
  }
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    wprintf(L"OnUpdateAvailable [%s]\n", version_string);
    return HandleEvent(ON_UPDATE_AVAILABLE);
  }
  STDMETHOD(OnWaitingToDownload)() {
    wprintf(L"OnWaitingToDownload\n");
    return HandleEvent(ON_WAITING_TO_INSTALL);
  }
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    wprintf(L"OnDownloading [%d][%d]\n", time_remaining_ms, pos);
    return HandleEvent(ON_DOWNLOADING);
  }
  STDMETHOD(OnWaitingToInstall)() {
    wprintf(L"OnWaitingToInstall\n");
    return HandleEvent(ON_WAITING_TO_INSTALL);
  }
  STDMETHOD(OnInstalling)() {
    wprintf(L"OnInstalling\n");
    return HandleEvent(ON_INSTALLING);
  }
  STDMETHOD(OnPause)() {
    wprintf(L"OnPause\n");
    return HandleEvent(ON_PAUSE);
  }
  STDMETHOD(OnComplete)(CompletionCodes code, const TCHAR* text) {
    wprintf(L"OnComplete [%d][%s]\n", code, text);
    int event_code = 0;
    switch (code) {
      case COMPLETION_CODE_SUCCESS:
        event_code |= ON_COMPLETE_SUCCESS;
        break;
      case COMPLETION_CODE_SUCCESS_CLOSE_UI:
        event_code |= ON_COMPLETE_SUCCESS_CLOSE_UI;
        break;
      case COMPLETION_CODE_ERROR:
        event_code |= ON_COMPLETE_ERROR;
        break;
      case COMPLETION_CODE_RESTART_ALL_BROWSERS:
        event_code |= ON_COMPLETE_RESTART_ALL_BROWSERS;
        break;
      case COMPLETION_CODE_REBOOT:
        event_code |= ON_COMPLETE_REBOOT;
        break;
      case COMPLETION_CODE_RESTART_BROWSER:
        event_code |= ON_COMPLETE_RESTART_BROWSER;
        break;
      case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
        event_code |= ON_COMPLETE_RESTART_ALL_BROWSERS_NOTICE_ONLY;
        break;
      case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
        event_code |= ON_COMPLETE_REBOOT_NOTICE_ONLY;
        break;
      case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
        event_code |= ON_COMPLETE_RESTART_BROWSER_NOTICE_ONLY;
        break;
      case COMPLETION_CODE_RUN_COMMAND:
        event_code |= ON_COMPLETE_RUN_COMMAND;
        break;
      default:
        break;
    }
    ::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);
    return HandleEvent(event_code);
  }
  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    wprintf(L"SetEventSink [%d]\n", event_sink);
    event_sink_ = event_sink;
    return HandleEvent(SET_EVENT_SINK);
  }

  CComPtr<IProgressWndEvents> event_sink_;
};

#endif  // OMAHA_TOOLS_SRC_PERFORMONDEMAND_PERFORMONDEMAND_H_
