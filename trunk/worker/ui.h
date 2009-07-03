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

#ifndef OMAHA_WORKER_UI_H__
#define OMAHA_WORKER_UI_H__

#include <atlbase.h>
#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/wtl_atlapp_wrapper.h"
#include "omaha/goopdate/resource.h"
#include "omaha/worker/job_observer.h"
#include "omaha/worker/uilib/static_ex.h"

namespace omaha {

class HighresTimer;

// The message is used to communicate between InstallStoppedWnd and
// ProgressWnd.
const DWORD WM_INSTALL_STOPPED = WM_APP;

// Implements the "Installation Stopped" window. InstallStoppedWnd is
// modal relative to its parent. When InstallStoppedWnd is closed it sends
// a user message to its parent to notify which button the user has clicked on.
class InstallStoppedWnd
    : public CAxDialogImpl<InstallStoppedWnd>,
      public CMessageFilter {
  typedef CAxDialogImpl<InstallStoppedWnd> Base;
 public:

  static const int IDD = IDD_INSTALL_STOPPED;

  InstallStoppedWnd(CMessageLoop* message_loop, HWND parent);
  ~InstallStoppedWnd();

  // Closes the window, handling transition back to the parent window.
  HRESULT CloseWindow();

  BOOL PreTranslateMessage(MSG* msg) {
    return CWindow::IsDialogMessage(msg);
  }

  BEGIN_MSG_MAP(InstallStoppedWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    COMMAND_ID_HANDLER(IDOK, OnClickButton)
    COMMAND_ID_HANDLER(IDCANCEL, OnClickButton)
    CHAIN_MSG_MAP(Base)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  // All message handlers must be protected by exception barriers.
  LRESULT OnInitDialog(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);    // NOLINT
  LRESULT OnClickButton(WORD notify_code, WORD id, HWND wnd_ctl, BOOL& handled);  // NOLINT
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);       // NOLINT

  CMessageLoop* message_loop_;
  HWND parent_;

  DISALLOW_EVIL_CONSTRUCTORS(InstallStoppedWnd);
};

class ProgressWndEvents;

// Implements the UI progress window.
class ProgressWnd
    : public CAxDialogImpl<ProgressWnd>,
      public CMessageFilter,
      public JobObserver {
  typedef CAxDialogImpl<ProgressWnd> Base;
 public:
  static const int IDD = IDD_PROGRESS;

  ProgressWnd(CMessageLoop* message_loop, HWND parent);
  ~ProgressWnd();

  HRESULT Initialize();

  void SetVisible(bool visible) {
    ShowWindow(visible ? SW_SHOWNORMAL : SW_HIDE);
  }

  BOOL PreTranslateMessage(MSG* msg) {
    return CWindow::IsDialogMessage(msg);
  }

  HRESULT GetWindowTitle(TCHAR* title, int size);
  HRESULT SetWindowTitle(const TCHAR* title);

  void set_is_machine(bool is_machine) { is_machine_ = is_machine; }
  void set_language(const CString& language) { language_ = language; }
  void set_product_name(const CString& name) { product_name_ = name; }
  void set_product_guid(const GUID& guid) { product_guid_ = guid; }

  // These methods are called by the job to transition the UI from
  // one state to another. The methods are always executed by the thread
  // that created this window.
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
  virtual void SetEventSink(ProgressWndEvents* ev) { events_sink_ = ev; }
  virtual void Uninitialize() {}

  BEGIN_MSG_MAP(ProgressWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_NCDESTROY, OnNCDestroy)
    MESSAGE_HANDLER(WM_INSTALL_STOPPED, OnInstallStopped)
    NOTIFY_CODE_HANDLER(MK_LBUTTON, OnUrlClicked)
    COMMAND_HANDLER(IDC_BUTTON1, BN_CLICKED, OnClickedButton)
    COMMAND_HANDLER(IDC_BUTTON2, BN_CLICKED, OnClickedButton)
    COMMAND_HANDLER(IDC_CLOSE,   BN_CLICKED, OnClickedButton)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    CHAIN_MSG_MAP(Base)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  // All message handlers must be protected by exception barriers.
  LRESULT OnInitDialog(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);  // NOLINT
  LRESULT OnClose(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);       // NOLINT
  LRESULT OnNCDestroy(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);   // NOLINT
  LRESULT OnInstallStopped(UINT msg, WPARAM wparam,
                           LPARAM lparam, BOOL& handled);   // NOLINT
  LRESULT OnClickedButton(WORD notify_code, WORD id,
                          HWND wnd_ctl, BOOL& handled);     // NOLINT
  LRESULT OnCancel(WORD notify_code, WORD id, HWND wnd_ctl, BOOL& handled);     // NOLINT
  LRESULT OnUrlClicked(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);               // NOLINT

  // Helpers.
  HRESULT ChangeControlState();
  HRESULT SetMarqueeMode(bool is_marquee);
  HRESULT EnableClose(bool enable);
  HRESULT EnableSystemCloseButton(bool enable);
  HRESULT SetWindowIcon();
  HRESULT ShowGetHelpLink(HRESULT error_code);

  // Returns true if the window is closed.
  bool MaybeCloseWindow();

  // Closes the Installation Stopped window if present. Returns true if closed.
  bool CloseInstallStoppedWindow();

  // Closes the window.
  HRESULT CloseWindow();

  void MaybeRequestExitProcess();
  void RequestExitProcess();

  // The states are used as indexes in zero-based arrays so they should
  // start at 0.
  enum States {
    STATE_INIT = 0,
    STATE_CHECKING_FOR_UPDATE,
    STATE_WAITING_TO_DOWNLOAD,
    STATE_DOWNLOADING,
    STATE_WAITING_TO_INSTALL,
    STATE_INSTALLING,
    STATE_PAUSED,
    STATE_COMPLETE_SUCCESS,
    STATE_COMPLETE_ERROR,
    STATE_COMPLETE_RESTART_BROWSER,
    STATE_COMPLETE_RESTART_ALL_BROWSERS,
    STATE_COMPLETE_REBOOT,
    STATE_END,
  };

#pragma warning(disable : 4510 4610)
// C4510: default constructor could not be generated
// C4610: struct can never be instantiated - user defined constructor required
  struct ControlAttributes {
    const bool is_visible_;
    const bool is_enabled_;
    const bool is_button_;
    const bool is_default_;
  };

  struct ControlState {
    const int id_;
    const ControlAttributes attr_[ProgressWnd::STATE_END + 1];
  };
#pragma warning(default : 4510 4610)

  static const ControlState ctls_[];

  CMessageLoop* message_loop_;
  HWND parent_;
  DWORD thread_id_;

  scoped_ptr<HighresTimer> metrics_timer_;

  // Due to a repaint issue in StaticEx we prefer to manage their lifetime
  // very aggressively so we contain them by reference instead of value.
  scoped_ptr<StaticEx> pause_resume_text_;
  scoped_ptr<StaticEx> complete_text_;
  scoped_ptr<StaticEx> get_help_text_;

  States cur_state_;
  bool is_close_enabled_;

  ProgressWndEvents* events_sink_;

  bool is_machine_;
  CString language_;
  CString product_name_;
  GUID product_guid_;

  // The speed by which the progress bar moves in marquee mode.
  static const int kMarqueeModeUpdatesMs = 75;

  scoped_ptr<InstallStoppedWnd> install_stopped_wnd_;
  scoped_hicon  hicon_;   // Handle to large icon to show when ALT-TAB

  friend class UITest;
  DISALLOW_EVIL_CONSTRUCTORS(ProgressWnd);
};

}  // namespace omaha

#endif  // OMAHA_WORKER_UI_H__

