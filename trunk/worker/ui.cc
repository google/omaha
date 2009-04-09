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


#include "omaha/worker/ui.h"

#include "base/basictypes.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/exception_barrier.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/system_info.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/ui_displayed_event.h"
#include "omaha/worker/ui_ctls.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

InstallStoppedWnd::InstallStoppedWnd(CMessageLoop* message_loop, HWND parent)
    : message_loop_(message_loop),
      parent_(parent) {
  CORE_LOG(L3, (_T("[InstallStoppedWnd::InstallStoppedWnd]")));
  ASSERT1(message_loop);
  ASSERT1(::IsWindow(parent));
}

InstallStoppedWnd::~InstallStoppedWnd() {
  CORE_LOG(L3, (_T("[InstallStoppedWnd::~InstallStoppedWnd]")));
  if (IsWindow()) {
    VERIFY1(DestroyWindow());
  }
}

LRESULT InstallStoppedWnd::OnInitDialog(UINT,
                                        WPARAM,
                                        LPARAM,
                                        BOOL& handled) {    // NOLINT
  VERIFY1(!::EnableWindow(parent_, false));
  VERIFY1(message_loop_->AddMessageFilter(this));
  handled = true;
  return 1;
}

LRESULT InstallStoppedWnd::OnClickButton(WORD,
                                         WORD id,
                                         HWND,
                                         BOOL& handled) {   // NOLINT
  ExceptionBarrier eb;
  CORE_LOG(L3, (_T("[InstallStoppedWnd::OnClickButton]")));
  ASSERT1(id == IDOK || id == IDCANCEL);
  VERIFY1(::PostMessage(parent_, WM_INSTALL_STOPPED, id, 0));
  handled = true;
  return 0;
}

LRESULT InstallStoppedWnd::OnDestroy(UINT,
                                     WPARAM,
                                     LPARAM,
                                     BOOL& handled) {  // NOLINT
  VERIFY1(message_loop_->RemoveMessageFilter(this));
  VERIFY1(::EnableWindow(parent_, true));
  handled = true;
  return 0;
}

// TODO(omaha): The instance member is being used for LoadString. This
// member is not initialized when we create ProgressWnd through the COM
// wrapper. Need to fix this.
ProgressWnd::ProgressWnd(CMessageLoop* message_loop, HWND parent)
    : message_loop_(message_loop),
      parent_(parent),
      thread_id_(::GetCurrentThreadId()),
      cur_state_(STATE_INIT),
      events_sink_(NULL),
      is_machine_(false),
      product_guid_(GUID_NULL) {
  ASSERT1(message_loop);
  CORE_LOG(L3, (_T("[ProgressWnd::ProgressWnd]")));
}

ProgressWnd::~ProgressWnd() {
  CORE_LOG(L3, (_T("[ProgressWnd::~ProgressWnd]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  ASSERT1(!IsWindow());
  cur_state_ = STATE_END;
}

HRESULT ProgressWnd::Initialize() {
  CORE_LOG(L3, (_T("[ProgressWnd::Initialize]")));
  cur_state_ = STATE_INIT;
  ASSERT1(thread_id_ == ::GetCurrentThreadId());

  // For the InitCommonControlsEx call to succeed on XP a manifest is
  // needed to declare "Microsoft.Windows.Common-Controls" as a dependent
  // assembly. Further work may be needed to ensure W2K compatibility.
  INITCOMMONCONTROLSEX init_ctrls = { sizeof(INITCOMMONCONTROLSEX), 0 };
  ASSERT1(init_ctrls.dwSize == sizeof(init_ctrls));
  init_ctrls.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
  if (!::InitCommonControlsEx(&init_ctrls)) {
    // In case of XP RTM and XP SP1, InitCommonControlsEx is failing,
    // however the UI initializes fine and works correctly. Because of this
    // we only log the failure and do not error out.
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR, (_T("[InitCommonControlsEx failed][0x%08x]"), hr));
    ASSERT1(hr == 0x80070582);
  }

  if (!Create(parent_)) {
    CORE_LOG(LEVEL_ERROR, (_T("[Failed to create the window]")));
    return GOOPDATE_E_UI_INTERNAL_ERROR;
  }
  VERIFY1(message_loop_->AddMessageFilter(this));
  return S_OK;
}

HRESULT ProgressWnd::GetWindowTitle(TCHAR* title, int size) {
  CORE_LOG(L3, (_T("[ProgressWnd::GetWindowTitle]")));
  ASSERT1(size);
  if (!size) return E_INVALIDARG;

  // The text is truncated if there is no enough space in the buffer.
  int result = GetWindowText(title, size);
  CORE_LOG(L4, (_T("[title=%s]"), title));
  return result ? S_OK : HRESULTFromLastError();
}

HRESULT ProgressWnd::SetWindowTitle(const TCHAR* title) {
  CORE_LOG(L3, (_T("[ProgressWnd::SetWindowTitle][%s]"), title));
  return SetWindowText(title) ? S_OK : HRESULTFromLastError();
}

LRESULT ProgressWnd::OnInitDialog(UINT,
                                  WPARAM,
                                  LPARAM,
                                  BOOL& handled) {    // NOLINT
  ExceptionBarrier eb;
  CORE_LOG(L3, (_T("[ProgressWnd::OnInitDialog]")));
  handled = true;
  ASSERT1(!product_name_.IsEmpty());
  CString s;
  s.FormatMessage(IDS_WINDOW_TITLE, product_name_);
  VERIFY1(SetWindowText(s));
  pause_resume_text_.reset(new StaticEx);
  pause_resume_text_->SubclassWindow(GetDlgItem(IDC_PAUSE_RESUME_TEXT));
  VERIFY1(CenterWindow(NULL));

  VERIFY1(s.LoadString(IDS_INITIALIZING));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));
  VERIFY1(SUCCEEDED(SetMarqueeMode(true)));
  VERIFY1(SUCCEEDED(ChangeControlState()));
  VERIFY1(SUCCEEDED(SetWindowIcon()));

  metrics_timer_.reset(new HighresTimer);
  return 1;  // Let the system set the focus.
}

LRESULT ProgressWnd::OnClose(UINT,
                             WPARAM,
                             LPARAM,
                             BOOL& handled) {         // NOLINT
  ExceptionBarrier eb;
  CORE_LOG(L3, (_T("[ProgressWnd::OnClose]")));

  ++metric_worker_ui_click_x;

  MaybeCloseWindow();
  handled = true;
  return 0;
}

HRESULT ProgressWnd::CloseWindow() {
  HRESULT hr = DestroyWindow() ? S_OK : HRESULTFromLastError();
  if (events_sink_) {
    events_sink_->DoClose();
  }
  return hr;
}

void ProgressWnd::MaybeRequestExitProcess() {
  CORE_LOG(L3, (_T("[ProgressWnd::MaybeRequestExitProcess]")));
  if (cur_state_ != STATE_COMPLETE_SUCCESS &&
      cur_state_ != STATE_COMPLETE_ERROR &&
      cur_state_ != STATE_COMPLETE_RESTART_BROWSER &&
      cur_state_ != STATE_COMPLETE_RESTART_ALL_BROWSERS &&
      cur_state_ != STATE_COMPLETE_REBOOT) {
    return;
  }

  RequestExitProcess();
}

void ProgressWnd::RequestExitProcess() {
  CORE_LOG(L3, (_T("[ProgressWnd::RequestExitProcess]")));
  ::PostQuitMessage(0);
}

bool ProgressWnd::MaybeCloseWindow() {
  if (cur_state_ != STATE_COMPLETE_SUCCESS &&
      cur_state_ != STATE_COMPLETE_ERROR &&
      cur_state_ != STATE_COMPLETE_RESTART_BROWSER &&
      cur_state_ != STATE_COMPLETE_RESTART_ALL_BROWSERS &&
      cur_state_ != STATE_COMPLETE_REBOOT) {
    // The UI is not in final state: ask the user to proceed with closing it.
    // A modal dialog opens and sends a message back to this window to
    // communicate the user decision.
    install_stopped_wnd_.reset(new InstallStoppedWnd(message_loop_, *this));
    HWND hwnd = install_stopped_wnd_->Create(*this);
    if (hwnd) {
      CString title;
      VERIFY1(title.LoadString(IDS_INSTALLATION_STOPPED_WINDOW_TITLE));
      VERIFY1(install_stopped_wnd_->SetWindowText(title));

      CString button_text;
      VERIFY1(button_text.LoadString(IDS_RESUME_INSTALLATION));
      VERIFY1(::SetWindowText(
          install_stopped_wnd_->GetDlgItem(IDOK), button_text));

      VERIFY1(button_text.LoadString(IDS_CANCEL_INSTALLATION));
      VERIFY1(::SetWindowText(
          install_stopped_wnd_->GetDlgItem(IDCANCEL), button_text));

      CString s;
      s.FormatMessage(IDS_INSTALL_STOPPED, product_name_);
      VERIFY1(::SetWindowText(
          install_stopped_wnd_->GetDlgItem(IDC_INSTALL_STOPPED_TEXT), s));

      VERIFY1(install_stopped_wnd_->CenterWindow(*this));
      VERIFY1(!install_stopped_wnd_->ShowWindow(SW_SHOWDEFAULT));
      return false;
    } else {
      ASSERT1(false);
    }
  }

  // The UI is in a final state or the dialog box failed to open: proceed with
  // closing the UI.
  VERIFY1(SUCCEEDED(CloseWindow()));
  return true;
}

LRESULT ProgressWnd::OnNCDestroy(UINT,
                                 WPARAM,
                                 LPARAM,
                                 BOOL& handled) {         // NOLINT
  ExceptionBarrier eb;
  CORE_LOG(L3, (_T("[ProgressWnd::OnNCDestroy]")));
  VERIFY1(message_loop_->RemoveMessageFilter(this));
  MaybeRequestExitProcess();
  handled = false;  // Let ATL default processing handle the WM_NCDESTROY.
  return 0;
}

LRESULT ProgressWnd::OnClickedButton(WORD,
                                     WORD id,
                                     HWND,
                                     BOOL& handled) {   // NOLINT
  ExceptionBarrier eb;
  CORE_LOG(L3, (_T("[ProgressWnd::OnClickedButton]")));
  ASSERT1(id == IDC_BUTTON1 || id == IDC_BUTTON2 || id == IDC_CLOSE);
  handled = true;
  DestroyWindow();

  if (events_sink_) {
#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)

    switch (id) {
      case IDC_BUTTON1:
        switch (cur_state_) {
          case STATE_COMPLETE_RESTART_BROWSER:
            ++metric_worker_ui_restart_browser_now_click;
            events_sink_->DoRestartBrowsers();
            break;
          case STATE_COMPLETE_RESTART_ALL_BROWSERS:
            ++metric_worker_ui_restart_all_browsers_now_click;
            events_sink_->DoRestartBrowsers();
            break;
          case STATE_COMPLETE_REBOOT:
            ++metric_worker_ui_reboot_now_click;
            events_sink_->DoReboot();
            break;
          default:
            ASSERT1(false);
        }
        break;
      case IDC_BUTTON2:
        switch (cur_state_) {
          case STATE_COMPLETE_RESTART_BROWSER:
          case STATE_COMPLETE_RESTART_ALL_BROWSERS:
          case STATE_COMPLETE_REBOOT:
            break;
          default:
            ASSERT1(false);
        }
        break;
      case IDC_CLOSE:
        switch (cur_state_) {
          case STATE_COMPLETE_SUCCESS:
          case STATE_COMPLETE_ERROR:
            break;
          default:
            ASSERT1(false);
        }
        break;
      default:
        ASSERT1(false);
    }

#pragma warning(pop)

    events_sink_->DoClose();
  }
  return 0;
}

// Called when esc key is hit.
LRESULT ProgressWnd::OnCancel(WORD, WORD id,
                              HWND, BOOL& handled) {    // NOLINT
  ExceptionBarrier eb;
  VERIFY1(id == IDCANCEL);

  ++metric_worker_ui_esc_key_total;
  MaybeCloseWindow();
  handled = true;
  return 0;
}

LRESULT ProgressWnd::OnUrlClicked(int,
                                  LPNMHDR params,
                                  BOOL& handled) {      // NOLINT
  CORE_LOG(L3, (_T("[ProgressWnd::OnUrlClicked]")));
  ASSERT1(params);
  handled = true;

  if (IDC_GET_HELP_TEXT == params->idFrom) {
    ++metric_worker_ui_get_help_click;
  }

  NM_STATICEX* notification = reinterpret_cast<NM_STATICEX*>(params);
  events_sink_->DoLaunchBrowser(notification->action);

  return 1;
}

LRESULT ProgressWnd::OnInstallStopped(UINT msg,
                                      WPARAM wparam,
                                      LPARAM,
                                      BOOL& handled) {  // NOLINT
  CORE_LOG(L3, (_T("[ProgressWnd::OnInstallStopped]")));
  UNREFERENCED_PARAMETER(msg);

  install_stopped_wnd_.reset();

  ASSERT1(msg == WM_INSTALL_STOPPED);
  ASSERT1(wparam == IDOK || wparam == IDCANCEL);
  switch (wparam) {
    case IDOK:
      // TODO(omaha): Implement "Resume" here.
      break;
    case IDCANCEL:
      // The user has decided to cancel.
      metric_worker_ui_cancel_ms.AddSample(metrics_timer_->GetElapsedMs());
      ++metric_worker_ui_cancels;
      DestroyWindow();
      if (events_sink_) {
        events_sink_->DoClose();
      }
      break;
    default:
      ASSERT1(false);
      break;
  }

  handled = true;
  return 0;
}

void ProgressWnd::OnCheckingForUpdate() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnCheckingForUpdate]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_CHECKING_FOR_UPDATE;

  CString s;
  VERIFY1(s.LoadString(IDS_WAITING_TO_CONNECT));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(SUCCEEDED(SetMarqueeMode(true)));
  VERIFY1(SUCCEEDED(ChangeControlState()));
}

void ProgressWnd::OnUpdateAvailable(const TCHAR* version_string) {
  UNREFERENCED_PARAMETER(version_string);
  CORE_LOG(L3, (_T("[ProgressWnd::OnUpdateAvailable][%s]"), version_string));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }
}

void ProgressWnd::OnWaitingToDownload() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnWaitingToDownload]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_WAITING_TO_DOWNLOAD;

  CString s;
  s.FormatMessage(IDS_WAITING_TO_DOWNLOAD, product_name_);
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(SUCCEEDED(SetMarqueeMode(true)));
  VERIFY1(SUCCEEDED(ChangeControlState()));
}

void ProgressWnd::OnDownloading(int time_remaining_ms, int pos) {
  CORE_LOG(L5, (_T("[ProgressWnd::OnDownloading][pos=%d]"), pos));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  ASSERT1(time_remaining_ms >=0);
  ASSERT1(0 <= pos && pos <= 100);

  time_remaining_ms;  // unreferenced formal parameter

  cur_state_ = STATE_DOWNLOADING;

  CString s;
  s.FormatMessage(IDS_DOWNLOADING, product_name_);
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(s.LoadString(IDS_PAUSE));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_PAUSE_RESUME_TEXT), s));

  // TODO(omaha): implement time left.

  // When the network is connecting keep the marquee moving, otherwise
  // the user has no indication something is still going on.
  // TODO(omaha): when resuming an incomplete download this will not work.
  VERIFY1(SUCCEEDED(SetMarqueeMode(pos == 0)));
  ::SendMessage(GetDlgItem(IDC_PROGRESS), PBM_SETPOS, pos, 0);
  VERIFY1(SUCCEEDED(ChangeControlState()));
}

void ProgressWnd::OnWaitingToInstall() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnWaitingToInstall]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_WAITING_TO_INSTALL;

  CString s;
  s.FormatMessage(IDS_WAITING_TO_INSTALL, product_name_);
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(SUCCEEDED(SetMarqueeMode(true)));
  VERIFY1(SUCCEEDED(ChangeControlState()));
}

void ProgressWnd::OnInstalling() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnInstalling]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_INSTALLING;

  // Close the 'Install Stop' window if it is on the screen. The user can't
  // cancel an install anyway.
  MaybeCloseInstallStoppedWindow();

  CString s;
  s.FormatMessage(IDS_INSTALLING, product_name_);
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(SUCCEEDED(EnableSystemCloseButton(false)));
  VERIFY1(SUCCEEDED(SetMarqueeMode(true)));
  VERIFY1(SUCCEEDED(ChangeControlState()));
}

void ProgressWnd::OnPause() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnPause]")));
  ASSERT(false, (_T("These strings are not translated or in .rc files.")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_PAUSED;

  CString s;
  s.FormatMessage(IDS_DOWNLOAD_PAUSED, product_name_);
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(s.LoadString(IDS_RESUME));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_PAUSE_RESUME_TEXT), s));

  // TODO(omaha): implement time left.

  VERIFY1(SUCCEEDED(ChangeControlState()));
}

void ProgressWnd::OnShow() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnShow]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  if (!IsWindow() || IsWindowVisible()) {
    return;
  }

  CenterWindow(NULL);
  SetVisible(true);

  if (!::SetForegroundWindow(*this)) {
    CORE_LOG(LW, (_T("[::SetForegroundWindow failed %d]"), ::GetLastError()));
  }

  UIDisplayedEventManager::SignalEvent(is_machine_);
}

// The 'error_code' can contain an HRESULT or an installer error.
void ProgressWnd::OnComplete(CompletionCodes code,
                             const TCHAR* text,
                             DWORD error_code) {
  CORE_LOG(L3, (_T("[ProgressWnd::OnComplete]")
                _T("[code=%d][error_code=0x%08x]"), code, error_code));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());

  if (!IsWindow()) {
    RequestExitProcess();
    return;
  }

  // It is possible for the OnComplete callback to be called multiple times.
  // Subclassing the control multiple times results in a crash, therefore
  // unsubclass the control if the control has been created and subclassed
  // before.
  if (complete_text_ != NULL) {
    complete_text_->UnsubclassWindow(true);
    complete_text_.reset(NULL);
  }

  // Close the 'Install Stop' window if it is on the screen.
  MaybeCloseInstallStoppedWindow();

  CString s;
  switch (code) {
    case COMPLETION_CODE_SUCCESS_CLOSE_UI:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      VERIFY1(SUCCEEDED(CloseWindow()));
      return;

    case COMPLETION_CODE_SUCCESS:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      VERIFY1(s.LoadString(IDS_CLOSE));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_CLOSE), s));
      complete_text_.reset(new StaticEx);
      complete_text_->SubclassWindow(GetDlgItem(IDC_COMPLETE_TEXT));
      ASSERT1(text);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), text));
      break;
    case COMPLETION_CODE_ERROR:
      cur_state_ = STATE_COMPLETE_ERROR;
      VERIFY1(s.LoadString(IDS_CLOSE));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_CLOSE), s));
      complete_text_.reset(new StaticEx);
      complete_text_->SubclassWindow(GetDlgItem(IDC_ERROR_TEXT));
      ASSERT1(text);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_ERROR_TEXT), text));
      VERIFY1(SUCCEEDED(ShowGetHelpLink(error_code)));
      break;
    case COMPLETION_CODE_RESTART_ALL_BROWSERS:
      cur_state_ = STATE_COMPLETE_RESTART_ALL_BROWSERS;
      VERIFY1(s.LoadString(IDS_RESTART_ALL_BROWSERS_NOW));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON1), s));
      VERIFY1(s.LoadString(IDS_RESTART_ALL_BROWSERS_LATER));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON2), s));
      s.FormatMessage(IDS_TEXT_RESTART_ALL_BROWSERS, product_name_);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      ++metric_worker_ui_restart_all_browsers_buttons_displayed;
      break;
    case COMPLETION_CODE_RESTART_BROWSER:
      cur_state_ = STATE_COMPLETE_RESTART_BROWSER;
      VERIFY1(s.LoadString(IDS_RESTART_BROWSER_NOW));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON1), s));
      VERIFY1(s.LoadString(IDS_RESTART_BROWSER_LATER));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON2), s));
      s.FormatMessage(IDS_TEXT_RESTART_BROWSER, product_name_);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      ++metric_worker_ui_restart_browser_buttons_displayed;
      break;
    case COMPLETION_CODE_REBOOT:
      ASSERT(false, (_T("The button actions are not implemented.")));
      cur_state_ = STATE_COMPLETE_REBOOT;
      VERIFY1(s.LoadString(IDS_RESTART_NOW));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON1), s));
      VERIFY1(s.LoadString(IDS_RESTART_LATER));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON2), s));
      s.FormatMessage(IDS_TEXT_REBOOT, product_name_);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      ++metric_worker_ui_reboot_buttons_displayed;
      break;
    case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      VERIFY1(s.LoadString(IDS_CLOSE));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_CLOSE), s));
      s.FormatMessage(IDS_TEXT_RESTART_ALL_BROWSERS, product_name_);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      break;
    case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      VERIFY1(s.LoadString(IDS_CLOSE));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_CLOSE), s));
      s.FormatMessage(IDS_TEXT_REBOOT, product_name_);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      break;
    case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      VERIFY1(s.LoadString(IDS_CLOSE));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_CLOSE), s));
      s.FormatMessage(IDS_TEXT_RESTART_BROWSER, product_name_);
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      break;
    case COMPLETION_CODE_RUN_COMMAND:
    default:
      ASSERT1(false);
      break;
  }

  VERIFY1(SUCCEEDED(EnableSystemCloseButton(true)));
  VERIFY1(SUCCEEDED(ChangeControlState()));
}

HRESULT ProgressWnd::ChangeControlState() {
  for (size_t i = 0; i != arraysize(ProgressWnd::ctls_); ++i) {
    const ControlState& ctl_state = ctls_[i];
    HWND hwnd = GetDlgItem(ctls_[i].id_);
    ASSERT1(hwnd);
    const ControlAttributes& attr = ctl_state.attr_[cur_state_];
    ::ShowWindow(hwnd, attr.is_visible_ ? SW_SHOW : SW_HIDE);
    ::EnableWindow(hwnd, attr.is_enabled_ ? true : false);
    if (attr.is_button_ && attr.is_default_) {
      // We ask the dialog manager to give the default push button the focus, so
      // for instance the <Enter> key works as expected.
      GotoDlgCtrl(hwnd);
      LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
      if (style) {
        style |= BS_DEFPUSHBUTTON;
        ::SetWindowLong(hwnd, GWL_STYLE, style);
      }
    }
  }
  return S_OK;
}

HRESULT ProgressWnd::SetMarqueeMode(bool is_marquee) {
  if (!SystemInfo::IsRunningOnXPOrLater()) {
    // Marquee is not supported on OSes below XP.
    return S_OK;
  }

  HWND progress_bar = GetDlgItem(IDC_PROGRESS);
  if (!progress_bar) {
    return GOOPDATE_E_UI_INTERNAL_ERROR;
  }

  LONG style = ::GetWindowLong(progress_bar, GWL_STYLE);
  if (!style) {
    return HRESULTFromLastError();
  }

  if (is_marquee) {
    if (style & PBS_MARQUEE) {
      return S_OK;
    }

    style |= PBS_MARQUEE;
    style = ::SetWindowLong(progress_bar, GWL_STYLE, style);
    if (!style) {
      return HRESULTFromLastError();
    }

    bool result = ::SendMessage(progress_bar, PBM_SETMARQUEE,
                                is_marquee, kMarqueeModeUpdatesMs) != 0;
    return result ? S_OK : GOOPDATE_E_UI_INTERNAL_ERROR;
  } else {
    if (!(style & PBS_MARQUEE)) {
      return S_OK;
    }

    style &= ~PBS_MARQUEE;
    style = ::SetWindowLong(progress_bar, GWL_STYLE, style);
    if (!style) {
      return HRESULTFromLastError();
    }
    return S_OK;
  }
}

HRESULT ProgressWnd::EnableSystemCloseButton(bool enable) {
  HMENU menu = ::GetSystemMenu(*this, false);
  ASSERT1(menu);
  uint32 flags = MF_BYCOMMAND;
  flags |= enable ? MF_ENABLED : MF_GRAYED;
  VERIFY1(::EnableMenuItem(menu, SC_CLOSE, flags) != -1);
  return S_OK;
}

// The system displays the system large icon in the ALT+TAB dialog box.
// We do not need any small icon in the window caption. However, setting
// ICON_BIG has the side effect of the window displaying a scaled down
// version of it in the window caption. We could not find any way to
// hide that icon, including setting the icon to NULL or handling WM_GETICON
// message.
HRESULT ProgressWnd::SetWindowIcon() {
  const int cx = ::GetSystemMetrics(SM_CXICON);
  const int cy = ::GetSystemMetrics(SM_CYICON);
  HINSTANCE exe_instance = reinterpret_cast<HINSTANCE>(kExeLoadingAddress);
  reset(hicon_,
        reinterpret_cast<HICON>(::LoadImage(exe_instance,
                                            MAKEINTRESOURCE(IDI_APP),
                                            IMAGE_ICON,
                                            cx,
                                            cy,
                                            LR_DEFAULTCOLOR)));
  if (!hicon_) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR, (_T("[LoadImage failed 0x%08x]"), hr));
    return hr;
  }
  VERIFY1(SendMessage(WM_SETICON,
                      ICON_BIG,
                      reinterpret_cast<LPARAM>(get(hicon_))) == NULL);
  return S_OK;
}

HRESULT ProgressWnd::ShowGetHelpLink(HRESULT error_code) {
  // When running elevated and ProcessLauncherClass is not registered, the
  // browser launch from the link will fail. Don't display a link that will not
  // work.
  // TODO(omaha): Determine if ProcessLauncherClass is registered. Maybe move
  // this code to the Worker.
  if (vista_util::IsVistaOrLater() && vista_util::IsUserAdmin()) {
    return S_OK;
  }

  // Do not display the link if the error already has a link.
  if (GOOPDATE_E_OS_NOT_SUPPORTED == error_code) {
    return S_OK;
  }

  const TCHAR* const kHelpLinkSourceId = _T("gethelp");
  CString url;
  HRESULT hr = goopdate_utils::BuildHttpGetString(
      kUrlMoreInformation,
      error_code,
      0,   // extra code 1
      0,   // extra code 2
      GuidToString(product_guid_),
      GetVersionString(),
      is_machine_,
      language_,
      kHelpLinkSourceId,
      &url);
  if (FAILED(hr)) {
    return hr;
  }

  const TCHAR* const kLinkFormat = _T("<a=%s>%s</a>");
  CString display_text;
  VERIFY1(display_text.LoadString(IDS_HELP_ME_FIX_THIS_TEXT));
  CString link_string;
  link_string.Format(kLinkFormat, url, display_text);

  get_help_text_.reset(new StaticEx);
  get_help_text_->SubclassWindow(GetDlgItem(IDC_GET_HELP_TEXT));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_GET_HELP_TEXT), link_string));

  ++metric_worker_ui_get_help_displayed;
  return S_OK;
}

bool ProgressWnd::MaybeCloseInstallStoppedWindow() {
  if (install_stopped_wnd_.get() && install_stopped_wnd_->IsWindow()) {
    VERIFY1(install_stopped_wnd_->DestroyWindow());
    install_stopped_wnd_.reset();
    return true;
  } else {
    return false;
  }
}

}  // namespace omaha

