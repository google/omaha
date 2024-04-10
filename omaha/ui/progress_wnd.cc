// Copyright 2007-2010 Google Inc.
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


#include "omaha/ui/progress_wnd.h"
#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/non_localized_resource.h"
#include "omaha/ui/ui_ctls.h"
#include "omaha/ui/ui_metrics.h"

namespace omaha {

namespace {

// The current UI is only able to show one completion type. If apps in the
// bundle have different completion type, then we need to decide which
// one should be shown to the user. The following array lists the types
// from low priority to high priority. The completion type with highest
// priority will be shown to the user.
const CompletionCodes kCompletionCodesActionPriority[] = {
  COMPLETION_CODE_EXIT_SILENTLY,
  COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
  COMPLETION_CODE_SUCCESS,
  COMPLETION_CODE_LAUNCH_COMMAND,
  COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
  COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
  COMPLETION_CODE_RESTART_BROWSER,
  COMPLETION_CODE_RESTART_ALL_BROWSERS,
  COMPLETION_CODE_REBOOT_NOTICE_ONLY,
  COMPLETION_CODE_REBOOT,
  COMPLETION_CODE_ERROR,
  COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL,
};

// kCompletionCodesActionPriority should have all the values in enumeration
// CompletionCodes. The enumeration value starts from 1 so the array size
// should match the last value in the enumeration.
COMPILE_ASSERT(arraysize(kCompletionCodesActionPriority) ==
    COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL,
    CompletionCodesActionPriority_missing_completion_code);

int GetActionPriority(CompletionCodes code) {
  for (int i = 0; i < arraysize(kCompletionCodesActionPriority); ++i) {
    if (kCompletionCodesActionPriority[i] == code) {
      return i;
    }
  }

  ASSERT1(false);
  return -1;
}

bool AreAllAppsCanceled(const std::vector<AppCompletionInfo>& apps_info) {
  for (size_t i = 0; i < apps_info.size(); ++i) {
    if (!apps_info[i].is_canceled) {
      return false;
    }
  }

  return true;
}

}   // namespace

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
    VERIFY_SUCCEEDED(CloseWindow());
  }
}

// Enables the parent window and destroys this window.
// Enabling the parent window before destroying this one causes the parent
// window to get the focus and avoids a visible momentary lack of focus if we
// instead call SetFocus for the parent after the window is destroyed.
HRESULT InstallStoppedWnd::CloseWindow() {
  ASSERT1(IsWindow());
  VERIFY1(::EnableWindow(parent_, true));

  return DestroyWindow() ? S_OK : HRESULTFromLastError();
}

// Disables the parent window.
LRESULT InstallStoppedWnd::OnInitDialog(UINT,
                                        WPARAM,
                                        LPARAM,
                                        BOOL& handled) {    // NOLINT
  VERIFY1(!::EnableWindow(parent_, false));
  VERIFY1(message_loop_->AddMessageFilter(this));

  // 9-pixel-high "Segoe UI".
  VERIFY1(default_font_.CreatePointFont(90, _T("Segoe UI")));
  SendMessageToDescendants(
      WM_SETFONT,
      reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  (EnableFlatButtons(m_hWnd));

  handled = true;
  return 1;
}

// By letting the parent destroy this window, the parent to manage the entire
// lifetime of this window and avoid creating a synchronization problem by
// changing the value of IsInstallStoppedWindowPresent() during the middle of
// one of the parent's methods.
LRESULT InstallStoppedWnd::OnClickButton(WORD,
                                         WORD id,
                                         HWND,
                                         BOOL& handled) {   // NOLINT
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
  handled = true;
  return 0;
}

ProgressWnd::ProgressWnd(CMessageLoop* message_loop, HWND parent)
    : CompleteWnd(IDD_PROGRESS,
                  ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS,
                  message_loop,
                  parent),
      cur_state_(STATE_INIT),
      events_sink_(NULL),
      is_canceled_(false) {
  CORE_LOG(L3, (_T("[ProgressWnd::ProgressWnd]")));
}

ProgressWnd::~ProgressWnd() {
  CORE_LOG(L3, (_T("[ProgressWnd::~ProgressWnd]")));
  ASSERT1(!IsWindow());
  cur_state_ = STATE_END;
}

void ProgressWnd::SetEventSink(ProgressWndEvents* ev) {
  events_sink_ = ev;
  CompleteWnd::SetEventSink(events_sink_);
}

LRESULT ProgressWnd::OnInitDialog(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param,
                                  BOOL& handled) {  // NOLINT
  CORE_LOG(L3, (_T("[ProgressWnd::OnInitDialog]")));
  UNREFERENCED_PARAMETER(message);
  UNREFERENCED_PARAMETER(w_param);
  UNREFERENCED_PARAMETER(l_param);
  UNREFERENCED_PARAMETER(handled);

  InitializeDialog();
  GetWindowText(CStrBuf(base_window_title_, 256), 256);

  VERIFY_SUCCEEDED(SetMarqueeMode(true));

  CString state_text;
  VERIFY1(state_text.LoadString(IDS_INITIALIZING));
  DisplayNewState(state_text);
  VERIFY_SUCCEEDED(ChangeControlState());

  metrics_timer_.reset(new HighresTimer);

  return 1;  // Let the system set the focus.
}

// If closing is disabled, does not close the window.
// If in a completion state, the window is closed.
// Otherwise, the InstallStoppedWnd is displayed and the window is closed only
// if the user decides to cancel.
bool ProgressWnd::MaybeCloseWindow() {
  if (!is_close_enabled()) {
    return false;
  }

  if (cur_state_ != STATE_COMPLETE_SUCCESS &&
      cur_state_ != STATE_COMPLETE_ERROR &&
      cur_state_ != STATE_COMPLETE_RESTART_BROWSER &&
      cur_state_ != STATE_COMPLETE_RESTART_ALL_BROWSERS &&
      cur_state_ != STATE_COMPLETE_REBOOT) {
    // The UI is not in final state: ask the user to proceed with closing it.
    // A modal dialog opens and sends a message back to this window to
    // communicate the user decision.
    install_stopped_wnd_.reset(new InstallStoppedWnd(message_loop(), *this));
    HWND hwnd = install_stopped_wnd_->Create(*this);
    ASSERT1(hwnd);
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
      VERIFY1(s.LoadString(IDS_INSTALL_STOPPED));
      VERIFY1(::SetWindowText(
          install_stopped_wnd_->GetDlgItem(IDC_INSTALL_STOPPED_TEXT), s));

      VERIFY1(install_stopped_wnd_->CenterWindow(*this));
      VERIFY1(!install_stopped_wnd_->ShowWindow(SW_SHOWDEFAULT));
      return false;
    }
  }

  VERIFY_SUCCEEDED(CloseWindow());
  return true;
}

LRESULT ProgressWnd::OnClickedButton(WORD notify_code,
                                     WORD id,
                                     HWND wnd_ctl,
                                     BOOL& handled) {   // NOLINT
  CORE_LOG(L3, (_T("[ProgressWnd::OnClickedButton]")));
  ASSERT1(id == IDC_BUTTON1 || id == IDC_BUTTON2 || id == IDC_CLOSE);
  ASSERT1(events_sink_);

#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)

  switch (id) {
    case IDC_BUTTON1:
      // TODO(omaha): Consider doing something if the callbacks fail.
      switch (cur_state_) {
        case STATE_COMPLETE_RESTART_BROWSER:
          ++metric_worker_ui_restart_browser_now_click;
          VERIFY1(events_sink_->DoRestartBrowser(false, post_install_urls_));
          break;
        case STATE_COMPLETE_RESTART_ALL_BROWSERS:
          ++metric_worker_ui_restart_all_browsers_now_click;
          VERIFY1(events_sink_->DoRestartBrowser(true, post_install_urls_));
          break;
        case STATE_COMPLETE_REBOOT:
          ++metric_worker_ui_reboot_now_click;
          VERIFY1(events_sink_->DoReboot());
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
          return CompleteWnd::OnClickedButton(notify_code,
                                              id,
                                              wnd_ctl,
                                              handled);
          break;
        default:
          ASSERT1(false);
      }
      break;
    default:
      ASSERT1(false);
  }
#pragma warning(pop)

  // TODO(omaha3): In closing the Window here, we assume that none of the above
  // code does anything that might delay the UI response. This should be true
  // since we won't actually be restarting browsers, etc. from the UI.
  handled = true;
  VERIFY_SUCCEEDED(CloseWindow());

  return 0;
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
  // TODO(omaha): Swap the meaning of IDOK and IDCANCEL. IDCANCEL gets passed
  // when the user hits the esc key. Successive esc presses result in the window
  // disappearing. Instead, we would like the default (set in the .rc files) and
  // esc key option to both resume. Changing this requires swapping all uses
  // in this file as well as in the IDD_INSTALL_STOPPED definition.
  // Maybe use different constants internally too since these values are used
  // by different classes and ProgressWnd should not need to know how
  // InstallStoppedWnd is implemented.
  // It's possible this will also fix arrow key problem (http://b/1338787).
  switch (wparam) {
    case IDOK:
      // TODO(omaha): Implement "Resume" here.
      break;
    case IDCANCEL:
      HandleCancelRequest();
      break;
    default:
      ASSERT1(false);
      break;
  }

  handled = true;
  return 0;
}

void ProgressWnd::HandleCancelRequest() {
  CString s;
  VERIFY1(s.LoadString(IDS_CANCELING));
  DisplayNewState(s);

  if (is_canceled_) {
    return;
  }
  is_canceled_ = true;

  // The user has decided to cancel.
  metric_worker_ui_cancel_ms.AddSample(metrics_timer_->GetElapsedMs());
  ++metric_worker_ui_cancels;

  if (events_sink_) {
    events_sink_->DoCancel();
  }
}

void ProgressWnd::OnCheckingForUpdate() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnCheckingForUpdate]")));
  ASSERT1(thread_id() == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_CHECKING_FOR_UPDATE;

  CString s;
  VERIFY1(s.LoadString(IDS_WAITING_TO_CONNECT));
  DisplayNewState(s);

  VERIFY_SUCCEEDED(ChangeControlState());
}

void ProgressWnd::OnUpdateAvailable(const CString& app_id,
                                    const CString& app_name,
                                    const CString& version_string) {
  CORE_LOG(L3, (_T("[ProgressWnd::OnUpdateAvailable][%s][%s]"),
                app_name, version_string));
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  UNREFERENCED_PARAMETER(version_string);

  ASSERT1(thread_id() == ::GetCurrentThreadId());

  if (!IsWindow()) {
    return;
  }
}

void ProgressWnd::OnWaitingToDownload(const CString& app_id,
                                      const CString& app_name) {
  CORE_LOG(L3, (_T("[ProgressWnd::OnWaitingToDownload][%s]"), app_name));
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);

  ASSERT1(thread_id() == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_WAITING_TO_DOWNLOAD;

  CString s;
  VERIFY1(s.LoadString(IDS_WAITING_TO_DOWNLOAD));
  DisplayNewState(s);

  VERIFY_SUCCEEDED(ChangeControlState());
}

// May be called repeatedly during download.
void ProgressWnd::OnDownloading(const CString& app_id,
                                const CString& app_name,
                                int time_remaining_ms,
                                int pos) {
  CORE_LOG(L5, (_T("[ProgressWnd::OnDownloading][%s][remaining ms=%d][pos=%d]"),
                app_name, time_remaining_ms, pos));
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);

  ASSERT1(thread_id() == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  ASSERT1(0 <= pos && pos <= 100);

  cur_state_ = STATE_DOWNLOADING;

// This resource is not included in the resource files since it's not used.
#if 0
    VERIFY1(s.LoadString(IDS_PAUSE));
    VERIFY1(::SetWindowText(GetDlgItem(IDC_PAUSE_RESUME_TEXT), s));
#endif

  CString s;

  int time_remaining_sec = CeilingDivide(time_remaining_ms, kMsPerSec);
  if (time_remaining_ms < 0) {
    VERIFY1(s.LoadString(IDS_DOWNLOADING));
  } else if (time_remaining_ms == 0) {
    VERIFY1(s.LoadString(IDS_DOWNLOADING_COMPLETED));
  } else if (time_remaining_sec < kSecPerMin) {
    // Less than one minute remaining.
    s.FormatMessage(IDS_DOWNLOADING_SHORT, time_remaining_sec);
  } else if (time_remaining_sec < kSecondsPerHour) {
    // Less than one hour remaining.
    int time_remaining_minute = CeilingDivide(time_remaining_sec, kSecPerMin);
    s.FormatMessage(IDS_DOWNLOADING_LONG, time_remaining_minute);
  } else {
    int time_remaining_hour = CeilingDivide(time_remaining_sec,
                                            kSecondsPerHour);
    s.FormatMessage(IDS_DOWNLOADING_VERY_LONG, time_remaining_hour);
  }

  // Reduces flicker by only updating the control if the text has changed.
  CString orig_text;
  if (!GetDlgItemText(IDC_INSTALLER_STATE_TEXT, orig_text) || s != orig_text) {
    DisplayNewState(s);
  }

  VERIFY_SUCCEEDED(ChangeControlState());

  // When the network is connecting keep the marquee moving, otherwise
  // the user has no indication something is still going on.
  // TODO(omaha): when resuming an incomplete download this will not work.
  VERIFY_SUCCEEDED(SetMarqueeMode(pos == 0));
  if (pos > 0) {
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }
}

void ProgressWnd::OnWaitingRetryDownload(const CString& app_id,
                                         const CString& app_name,
                                         time64 next_retry_time) {
  CORE_LOG(L5, (_T("[ProgressWnd::OnWaitingRetryDownload][%s][retry at:%llu]"),
                app_name, next_retry_time));
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);

  ASSERT1(thread_id() == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  time64 now = GetCurrent100NSTime();
  if (now < next_retry_time) {
    CString s;
    int retry_time_in_sec =
        static_cast<int>(CeilingDivide(next_retry_time - now, kSecsTo100ns));
    s.FormatMessage(IDS_DOWNLOAD_RETRY, retry_time_in_sec);
    DisplayNewState(s);
    VERIFY_SUCCEEDED(ChangeControlState());
  }
}

void ProgressWnd::OnWaitingToInstall(const CString& app_id,
                                     const CString& app_name,
                                     bool* can_start_install) {
  CORE_LOG(L3, (_T("[ProgressWnd::OnWaitingToInstall][%s]"), app_name));
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);

  ASSERT1(thread_id() == ::GetCurrentThreadId());
  ASSERT1(can_start_install);
  if (!IsWindow()) {
    return;
  }

  if (STATE_WAITING_TO_INSTALL != cur_state_) {
    cur_state_ = STATE_WAITING_TO_INSTALL;

    CString s;
    VERIFY1(s.LoadString(IDS_WAITING_TO_INSTALL));
    DisplayNewState(s);

    VERIFY_SUCCEEDED(ChangeControlState());
  }

  // If we want to instead close the window and start install, call
  // CloseInstallStoppedWindow() and return *can_start_install = true.
  *can_start_install = !IsInstallStoppedWindowPresent();
}

// May be called repeatedly during install.
void ProgressWnd::OnInstalling(const CString& app_id,
                               const CString& app_name,
                               int time_remaining_ms,
                               int pos) {
  CORE_LOG(L5, (_T("[ProgressWnd::OnInstalling][%s][remaining ms=%d][pos=%d]"),
                app_name, time_remaining_ms, pos));
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app_name);
  UNREFERENCED_PARAMETER(time_remaining_ms);

  ASSERT1(thread_id() == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  // TODO(omaha3): This can now occur because installs are not gated.
  // ASSERT1(!IsInstallStoppedWindowPresent());

  if (STATE_INSTALLING != cur_state_) {
    cur_state_ = STATE_INSTALLING;

    CString s;
    VERIFY1(s.LoadString(IDS_INSTALLING));
    DisplayNewState(s);

    VERIFY_SUCCEEDED(ChangeControlState());
  }

  VERIFY_SUCCEEDED(SetMarqueeMode(pos <= 0));
  if (pos > 0) {
    SendDlgItemMessage(IDC_PROGRESS, PBM_SETPOS, pos, 0);
  }
}

// TODO(omaha): Should this message display the app name or bundle name? Is the
// entire bundle paused?
void ProgressWnd::OnPause() {
  CORE_LOG(L3, (_T("[ProgressWnd::OnPause]")));
  ASSERT(false, (_T("These strings are not in the .rc files.")));
  ASSERT1(thread_id() == ::GetCurrentThreadId());
  if (!IsWindow()) {
    return;
  }

  cur_state_ = STATE_PAUSED;

// These resources are not included in resource files since they are not used.
#if 0
  CString s;
  s.FormatMessage(IDS_DOWNLOAD_PAUSED, bundle_name());
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), s));

  VERIFY1(s.LoadString(IDS_RESUME));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_PAUSE_RESUME_TEXT), s));
#endif

  // TODO(omaha): implement time left.

  VERIFY_SUCCEEDED(ChangeControlState());
}

void ProgressWnd::DeterminePostInstallUrls(const ObserverCompletionInfo& info) {
  ASSERT1(post_install_urls_.empty());
  post_install_urls_.clear();

  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    const AppCompletionInfo& app_info = info.apps_info[i];
    if (!app_info.post_install_url.IsEmpty() &&
        (app_info.completion_code == COMPLETION_CODE_RESTART_ALL_BROWSERS ||
         app_info.completion_code == COMPLETION_CODE_RESTART_BROWSER)) {
      post_install_urls_.push_back(app_info.post_install_url);
    }
  }
  ASSERT1(!post_install_urls_.empty());
}

// TODO(omaha): We can eliminate this function is we have a better UI that can
// show completion status for each app in the bundle.
//
// Overall completion code is determined by apps' completion codes and bundle
// completion code. If bundle installation fails or installation completed after
// a cancel is attempted, returns bundle completion code.
// Otherwise the app's completion code that has the greatest priority is
// returned.
CompletionCodes ProgressWnd::GetBundleOverallCompletionCode(
    const ObserverCompletionInfo& info) const {
  if (info.completion_code == COMPLETION_CODE_ERROR ||
      info.completion_code == COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL) {
    return info.completion_code;
  }

  ASSERT1(info.completion_code == COMPLETION_CODE_SUCCESS);

  CompletionCodes overall_completion_code =
      kCompletionCodesActionPriority[0];
  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    if (GetActionPriority(overall_completion_code) <
        GetActionPriority(info.apps_info[i].completion_code)) {
      overall_completion_code = info.apps_info[i].completion_code;
    }
  }

  return overall_completion_code;
}

// TODO(omaha3): How should we display the restart browser and reboot messages
// when multiple apps are being installed, some of which may have failed? Should
// we use the app name or bundle name?
void ProgressWnd::OnComplete(const ObserverCompletionInfo& observer_info) {
  CORE_LOG(L3, (_T("[ProgressWnd::OnComplete][%s]"), observer_info.ToString()));
  ASSERT1(thread_id() == ::GetCurrentThreadId());

  if (!CompleteWnd::OnComplete()) {
    return;
  }

  // Close the 'Install Stop' window if it is on the screen.
  // TODO(omaha3): This had been before all main dialog UI. Make sure looks OK.
  CloseInstallStoppedWindow();

  // TODO(omaha3): Do we want to avoid launching commands during an interactive
  // /ua update? If so, we'll need to handle that somehow. Using the observer
  // handles the silent update and install cases as well as the OnDemand case.
  bool launch_commands_succeeded = LaunchCommandLines(observer_info,
                                                      is_machine());

  CString s;
  CompletionCodes overall_completion_code =
      GetBundleOverallCompletionCode(observer_info);
  CORE_LOG(L3, (_T("[overall completion code: %d]"), overall_completion_code));
  switch (overall_completion_code) {
    case COMPLETION_CODE_SUCCESS:
    case COMPLETION_CODE_LAUNCH_COMMAND:
    case COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL:
      cur_state_ = STATE_COMPLETE_SUCCESS;

      // TODO(omaha): Do not inherit from CompleteWnd once we have the new
      // bundle-supporting UI. Among other things, calling
      // DisplayCompletionDialog causes second call to OmahaWnd::OnComplete().
      CompleteWnd::DisplayCompletionDialog(true,
                                           observer_info.completion_text,
                                           observer_info.help_url);
      break;
    case COMPLETION_CODE_ERROR:
      // If all apps are canceled, no need to display any dialog.
      if (AreAllAppsCanceled(observer_info.apps_info)) {
        VERIFY_SUCCEEDED(CloseWindow());
        return;
      } else {
        cur_state_ = STATE_COMPLETE_ERROR;
        CompleteWnd::DisplayCompletionDialog(false,
                                             observer_info.completion_text,
                                             observer_info.help_url);
      }
      break;
    case COMPLETION_CODE_RESTART_ALL_BROWSERS:
      cur_state_ = STATE_COMPLETE_RESTART_ALL_BROWSERS;
      VERIFY1(s.LoadString(IDS_RESTART_NOW));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON1), s));
      VERIFY1(s.LoadString(IDS_RESTART_LATER));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON2), s));
      s.FormatMessage(IDS_TEXT_RESTART_ALL_BROWSERS, bundle_name());
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      DeterminePostInstallUrls(observer_info);
      ++metric_worker_ui_restart_all_browsers_buttons_displayed;
      break;
    case COMPLETION_CODE_RESTART_BROWSER:
      cur_state_ = STATE_COMPLETE_RESTART_BROWSER;
      VERIFY1(s.LoadString(IDS_RESTART_NOW));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON1), s));
      VERIFY1(s.LoadString(IDS_RESTART_LATER));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON2), s));
      s.FormatMessage(IDS_TEXT_RESTART_BROWSER, bundle_name());
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      DeterminePostInstallUrls(observer_info);
      ++metric_worker_ui_restart_browser_buttons_displayed;
      break;
    case COMPLETION_CODE_REBOOT:
      ASSERT(false, (_T("The button actions are not implemented.")));
      cur_state_ = STATE_COMPLETE_REBOOT;
      VERIFY1(s.LoadString(IDS_RESTART_NOW));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON1), s));
      VERIFY1(s.LoadString(IDS_RESTART_LATER));
      VERIFY1(::SetWindowText(GetDlgItem(IDC_BUTTON2), s));
      s.FormatMessage(IDS_TEXT_RESTART_COMPUTER, bundle_name());
      VERIFY1(::SetWindowText(GetDlgItem(IDC_COMPLETE_TEXT), s));
      ++metric_worker_ui_reboot_buttons_displayed;
      break;
    // TODO(omaha3): We may be able to eliminate these by having the caller
    // specify the appropriate success text. That is the only difference from
    // the COMPLETION_CODE_SUCCESS case. Alternatively, we can make a decision
    // in this class based on, for example, whether the browser is supported.
    case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      s.FormatMessage(IDS_TEXT_RESTART_ALL_BROWSERS, bundle_name());
      CompleteWnd::DisplayCompletionDialog(true, s, observer_info.help_url);
      break;
    case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      s.FormatMessage(IDS_TEXT_RESTART_COMPUTER, bundle_name());
      CompleteWnd::DisplayCompletionDialog(true, s, observer_info.help_url);
      break;
    case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      s.FormatMessage(IDS_TEXT_RESTART_BROWSER, bundle_name());
      CompleteWnd::DisplayCompletionDialog(true, s, observer_info.help_url);
      break;
    case COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      if (launch_commands_succeeded) {
        VERIFY_SUCCEEDED(CloseWindow());
        return;
      }

      CompleteWnd::DisplayCompletionDialog(true,
                                           observer_info.completion_text,
                                           observer_info.help_url);
      break;
    case COMPLETION_CODE_EXIT_SILENTLY:
      cur_state_ = STATE_COMPLETE_SUCCESS;
      VERIFY_SUCCEEDED(CloseWindow());
      return;
    default:
      ASSERT1(false);
      break;
  }

  VERIFY_SUCCEEDED(ChangeControlState());
}

HRESULT ProgressWnd::ChangeControlState() {
  for (size_t i = 0; i != arraysize(ctls_); ++i) {
    const ControlState& ctl_state = ctls_[i];
    SetControlAttributes(ctl_state.id_, ctl_state.attr_[cur_state_]);
  }
  return S_OK;
}

HRESULT ProgressWnd::SetMarqueeMode(bool is_marquee) {
  CWindow progress_bar = GetDlgItem(IDC_PROGRESS);
  LONG_PTR style = progress_bar.GetWindowLongPtr(GWL_STYLE);
  if (is_marquee) {
    style |= PBS_MARQUEE;
  } else {
    style &= ~PBS_MARQUEE;
  }
  progress_bar.SetWindowLongPtr(GWL_STYLE, style);
  progress_bar.SendMessage(PBM_SETMARQUEE, !!is_marquee, 0);

  return S_OK;
}

void ProgressWnd::DisplayNewState(const CString& state) {
  VERIFY1(::SetWindowText(GetDlgItem(IDC_INSTALLER_STATE_TEXT), state));

  CString title;
  title.FormatMessage(IDS_APPLICATION_NAME_CONCATENATION,
                      state,
                      base_window_title_);
  SetWindowText(title);
}

bool ProgressWnd::IsInstallStoppedWindowPresent() {
  return install_stopped_wnd_.get() && install_stopped_wnd_->IsWindow();
}

bool ProgressWnd::CloseInstallStoppedWindow() {
  if (IsInstallStoppedWindowPresent()) {
    VERIFY_SUCCEEDED(install_stopped_wnd_->CloseWindow());
    install_stopped_wnd_.reset();
    return true;
  } else {
    return false;
  }
}

}  // namespace omaha
