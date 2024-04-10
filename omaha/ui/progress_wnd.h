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

#ifndef OMAHA_UI_PROGRESS_WND_H_
#define OMAHA_UI_PROGRESS_WND_H_

#include <atlbase.h>
#include <memory>
#include <vector>

#include "omaha/base/time.h"
#include "omaha/base/wtl_atlapp_wrapper.h"
#include "omaha/client/install_progress_observer.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "omaha/ui/complete_wnd.h"
#include "omaha/ui/owner_draw_title_bar.h"

namespace omaha {

class HighresTimer;

// The message is used to communicate between InstallStoppedWnd and
// ProgressWnd.
const DWORD WM_INSTALL_STOPPED = WM_APP;

class ProgressWndEvents : public CompleteWndEvents {
 public:
  // Restarts the browser(s) and returns whether the browser was successfully
  // restarted.
  // If restart_all_browsers is true, all known browsers will be shutdown.
  // Otherwise only one type of browser will be shutdown. The concrete class
  // is expected to know which browser to shutdown in that case.
  // After that, only one type of browser will be restarted with the given URLs.
  //
  // Major browsers that support multi-tab differ how to open multiple URLs:
  // IExplorer (8.0): opens each URL in a separate window.
  // Firefox (3.6): by default if there is Firefox instance running, the URLs
  //          will be opened in tabs in the same window. Otherwise each URL will
  //          have its own window. Since we shutdown browser(s) first, it is
  //          more likely to result in multiple windows.
  // Chrome (5.0): opens each URL in a separate tab in the same window.
  virtual bool DoRestartBrowser(bool restart_all_browsers,
                                const std::vector<CString>& urls) = 0;

  // Initiates a reboot and returns whether it was iniated successfully.
  virtual bool DoReboot() = 0;

  // Indicates that current operation is canceled.
  virtual void DoCancel() = 0;
};

// Implements the "Installation Stopped" window. InstallStoppedWnd is
// modal relative to its parent. When InstallStoppedWnd is closed it sends
// a user message to its parent to notify which button the user has clicked on.
class InstallStoppedWnd
    : public CAxDialogImpl<InstallStoppedWnd>,
      public OwnerDrawTitleBar,
      public CustomDlgColors,
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
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  LRESULT OnInitDialog(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);    // NOLINT
  LRESULT OnClickButton(WORD notify_code, WORD id, HWND wnd_ctl, BOOL& handled);  // NOLINT
  LRESULT OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);       // NOLINT

  CMessageLoop* message_loop_;
  HWND parent_;

  CFont default_font_;

  DISALLOW_COPY_AND_ASSIGN(InstallStoppedWnd);
};


// Implements the UI progress window.
class ProgressWnd
    : public CompleteWnd,
      public InstallProgressObserver {
 public:
  ProgressWnd(CMessageLoop* message_loop, HWND parent);
  ~ProgressWnd() override;

  void SetEventSink(ProgressWndEvents* ev);

  // InstallProgressObserver methods.
  // TODO(omaha3): Update this comment.
  // These methods are called by the job to transition the UI from
  // one state to another. The methods are always executed by the thread
  // that created this window.
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

  BEGIN_MSG_MAP(ProgressWnd)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_INSTALL_STOPPED, OnInstallStopped)
    COMMAND_HANDLER(IDC_BUTTON1, BN_CLICKED, OnClickedButton)
    COMMAND_HANDLER(IDC_BUTTON2, BN_CLICKED, OnClickedButton)
    COMMAND_HANDLER(IDC_CLOSE,   BN_CLICKED, OnClickedButton)
    CHAIN_MSG_MAP(CompleteWnd)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  LRESULT OnInitDialog(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);  // NOLINT
  LRESULT OnInstallStopped(UINT msg,
                           WPARAM wparam,
                           LPARAM lparam,
                           BOOL& handled);  // NOLINT
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);   // NOLINT

  // Handles requests to close the window. Returns true if the window is closed.
  virtual bool MaybeCloseWindow();

  // Helpers.
  HRESULT ChangeControlState();
  HRESULT SetMarqueeMode(bool is_marquee);
  void DisplayNewState(const CString& state);

  bool IsInstallStoppedWindowPresent();

  void HandleCancelRequest();

  // Closes the Installation Stopped window if present. Returns true if closed.
  bool CloseInstallStoppedWindow();

  void DeterminePostInstallUrls(const ObserverCompletionInfo& info);
  CompletionCodes GetBundleOverallCompletionCode(
      const ObserverCompletionInfo& info) const;

  // TODO(omaha3): These states are used to control the UI elements. Otherwise,
  // we could have just a single "complete" state and track restart/reboot state
  // separately.
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
    // TODO(omaha): Collapse these two into one state. Controls are the same.
    // Add another variable to remember what to do in this state.
    STATE_COMPLETE_RESTART_BROWSER,
    STATE_COMPLETE_RESTART_ALL_BROWSERS,
    STATE_COMPLETE_REBOOT,
    STATE_END,
  };

  std::unique_ptr<HighresTimer> metrics_timer_;

  States cur_state_;

  std::unique_ptr<InstallStoppedWnd> install_stopped_wnd_;

  ProgressWndEvents* events_sink_;
  std::vector<CString> post_install_urls_;
  bool is_canceled_;
  bool is_chrome_appid_;

  CString base_window_title_;

  struct ControlState {
    const int id_;
    const ControlAttributes attr_[ProgressWnd::STATE_END + 1];
  };

  static const ControlState ctls_[];

  // The speed by which the progress bar moves in marquee mode.
  static const int kMarqueeModeUpdatesMs = 75;

  friend class UITest;
  DISALLOW_COPY_AND_ASSIGN(ProgressWnd);
};

}  // namespace omaha

#endif  // OMAHA_UI_PROGRESS_WND_H_
