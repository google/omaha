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


#include "omaha/ui/ui.h"
#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/system_info.h"
#include "omaha/base/window_utils.h"
#include "omaha/client/client_utils.h"
#include "omaha/google_update/resource.h"   // For the IDI_APP
#include "omaha/ui/ui_displayed_event.h"
#include "omaha/ui/ui_metrics.h"

namespace omaha {

const OmahaWnd::ControlAttributes OmahaWnd::kVisibleTextAttributes =
    { true,  true,  false, false };
const OmahaWnd::ControlAttributes OmahaWnd::kDefaultActiveButtonAttributes =
    { true,  true,  true,  true  };
const OmahaWnd::ControlAttributes OmahaWnd::kNonDefaultActiveButtonAttributes =
    { true,  true,  true,  false };
const OmahaWnd::ControlAttributes OmahaWnd::kVisibleImageAttributes =
    { true,  false, false, false };
const OmahaWnd::ControlAttributes OmahaWnd::kDisabledNonButtonAttributes =
    { false, false, false, false };

OmahaWnd::OmahaWnd(int dialog_id, CMessageLoop* message_loop, HWND parent)
    : IDD(dialog_id),
      message_loop_(message_loop),
      parent_(parent),
      thread_id_(::GetCurrentThreadId()),
      is_complete_(false),
      is_close_enabled_(true),
      events_sink_(NULL),
      is_machine_(false) {
  ASSERT1(message_loop);
  CORE_LOG(L3, (_T("[OmahaWnd::OmahaWnd]")));
}

OmahaWnd::~OmahaWnd() {
  CORE_LOG(L3, (_T("[OmahaWnd::~OmahaWnd]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());
  ASSERT1(!IsWindow());
}

HRESULT OmahaWnd::Initialize() {
  CORE_LOG(L3, (_T("[OmahaWnd::Initialize]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());

  if (!Create(parent_)) {
    CORE_LOG(LEVEL_ERROR, (_T("[Failed to create the window]")));
    return GOOPDATE_E_UI_INTERNAL_ERROR;
  }
  VERIFY1(message_loop_->AddMessageFilter(this));

  return S_OK;
}

void OmahaWnd::InitializeDialog() {    // NOLINT
  CORE_LOG(L3, (_T("[OmahaWnd::InitializeDialog]")));

  VERIFY1(SetWindowText(client_utils::GetInstallerDisplayName(bundle_name_)));

  VERIFY1(CenterWindow(NULL));
  VERIFY1(SUCCEEDED(WindowUtils::SetWindowIcon(m_hWnd,
                                               IDI_APP,
                                               address(hicon_))));
}

LRESULT OmahaWnd::OnClose(UINT,
                          WPARAM,
                          LPARAM,
                          BOOL& handled) {         // NOLINT
  CORE_LOG(L3, (_T("[OmahaWnd::OnClose]")));

  ++metric_worker_ui_click_x;

  MaybeCloseWindow();
  handled = true;
  return 0;
}

HRESULT OmahaWnd::CloseWindow() {
  HRESULT hr = DestroyWindow() ? S_OK : HRESULTFromLastError();
  if (events_sink_) {
    events_sink_->DoClose();
  }
  return hr;
}

void OmahaWnd::MaybeRequestExitProcess() {
  CORE_LOG(L3, (_T("[OmahaWnd::MaybeRequestExitProcess]")));
  if (!is_complete_) {
    return;
  }

  RequestExitProcess();
}

void OmahaWnd::RequestExitProcess() {
  CORE_LOG(L3, (_T("[OmahaWnd::RequestExitProcess]")));

  if (events_sink_) {
    events_sink_->DoExit();
  }
}

LRESULT OmahaWnd::OnNCDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {  // NOLINT
  CORE_LOG(L3, (_T("[OmahaWnd::OnNCDestroy]")));
  VERIFY1(message_loop_->RemoveMessageFilter(this));
  MaybeRequestExitProcess();
  handled = false;  // Let ATL default processing handle the WM_NCDESTROY.
  return 0;
}

// Called when esc key is hit.
// If close is disabled, does nothing because we don't want the window to close.
LRESULT OmahaWnd::OnCancel(WORD, WORD id, HWND, BOOL& handled) {  // NOLINT
  VERIFY1(id == IDCANCEL);

  if (!is_close_enabled_) {
    return 0;
  }

  ++metric_worker_ui_esc_key_total;
  MaybeCloseWindow();
  handled = true;
  return 0;
}

void OmahaWnd::Show() {
  CORE_LOG(L3, (_T("[OmahaWnd::Show]")));
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

bool OmahaWnd::OnComplete() {
  CORE_LOG(L3, (_T("[OmahaWnd::OnComplete]")));
  ASSERT1(thread_id_ == ::GetCurrentThreadId());

  if (!IsWindow()) {
    RequestExitProcess();
    return false;
  }

  is_complete_ = true;

  VERIFY1(SUCCEEDED(EnableClose(true)));

  return true;
}

void OmahaWnd::SetControlAttributes(int control_id,
                                    const ControlAttributes& attributes) {
  HWND hwnd = GetDlgItem(control_id);
  ASSERT1(hwnd);
  ::ShowWindow(hwnd, attributes.is_visible_ ? SW_SHOW : SW_HIDE);
  ::EnableWindow(hwnd, attributes.is_enabled_ ? true : false);
  if (attributes.is_button_ && attributes.is_default_) {
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

HRESULT OmahaWnd::EnableClose(bool enable) {
  is_close_enabled_ = enable;
  return EnableSystemCloseButton(is_close_enabled_);
}

HRESULT OmahaWnd::EnableSystemCloseButton(bool enable) {
  HMENU menu = ::GetSystemMenu(*this, false);
  ASSERT1(menu);
  uint32 flags = MF_BYCOMMAND;
  flags |= enable ? MF_ENABLED : MF_GRAYED;
  VERIFY1(::EnableMenuItem(menu, SC_CLOSE, flags) != -1);
  return S_OK;
}

// For the InitCommonControlsEx call to succeed on XP, a manifest is needed to
// declare "Microsoft.Windows.Common-Controls" as a dependent assembly.
// Further work may be needed to ensure W2K compatibility.
HRESULT InitializeCommonControls(DWORD control_classes) {
  INITCOMMONCONTROLSEX init_ctrls = { sizeof(INITCOMMONCONTROLSEX), 0 };
  ASSERT1(init_ctrls.dwSize == sizeof(init_ctrls));
  init_ctrls.dwICC = control_classes;
  if (!::InitCommonControlsEx(&init_ctrls)) {
    // In the case of XP RTM and XP SP1, InitCommonControlsEx is failing but
    // the UI initializes fine and works correctly. Because of this we only log
    // the failure and do not error out.
    const DWORD error = ::GetLastError();
    CORE_LOG(LEVEL_ERROR, (_T("[InitCommonControlsEx failed][%u]"), error));
    ASSERT1(ERROR_CLASS_ALREADY_EXISTS == error);
    if (ERROR_CLASS_ALREADY_EXISTS != error) {
      return HRESULT_FROM_WIN32(error);
    }
  }

  return S_OK;
}

}  // namespace omaha
