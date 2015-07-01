// Copyright 2011 Google Inc.
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

#include "omaha/ui/yes_no_dialog.h"
#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/window_utils.h"
#include "omaha/google_update/resource.h"
#include "omaha/ui/ui.h"

namespace omaha {

YesNoDialog::YesNoDialog(CMessageLoop* message_loop, HWND parent)
    :  message_loop_(message_loop),
       parent_(parent),
       yes_clicked_(false) {
  ASSERT1(message_loop);
  CORE_LOG(L3, (_T("[YesNoDialog::YesNoDialog]")));
}


YesNoDialog::~YesNoDialog() {
  CORE_LOG(L3, (_T("[YesNoDialog::~YesNoDialog]")));
  ASSERT1(!IsWindow());
}

HRESULT YesNoDialog::Initialize(const CString& yes_no_title,
                                const CString& yes_no_text) {
  ASSERT1(!IsWindow());

  if (!Create(parent_)) {
    CORE_LOG(LE, (_T("[Failed to create YesNoDialog]")));
    return GOOPDATE_E_UI_INTERNAL_ERROR;
  }

  VERIFY1(message_loop_->AddMessageFilter(this));

  VERIFY1(SetWindowText(yes_no_title));
  VERIFY1(::SetWindowText(GetDlgItem(IDC_YES_NO_TEXT), yes_no_text));

  CString yes;
  VERIFY1(yes.LoadString(IDS_YES));
  CString no;
  VERIFY1(no.LoadString(IDS_NO));

  VERIFY1(::SetWindowText(GetDlgItem(IDOK), yes));
  VERIFY1(::SetWindowText(GetDlgItem(IDCANCEL), no));

  HRESULT hr = WindowUtils::SetWindowIcon(m_hWnd, IDI_APP, address(hicon_));
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[Failed to SetWindowIcon][0x%x]"), hr));
  }

  // 9-pixel-high "Segoe UI".
  VERIFY1(default_font_.CreatePointFont(90, _T("Segoe UI")));
  SendMessageToDescendants(
      WM_SETFONT,
      reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  (EnableFlatButtons(m_hWnd));

  return S_OK;
}

HRESULT YesNoDialog::Show() {
  ASSERT1(IsWindow());
  ASSERT1(!IsWindowVisible());

  VERIFY1(CenterWindow(NULL));
  ShowWindow(SW_SHOWNORMAL);

  return S_OK;
}

LRESULT YesNoDialog::OnClickedButton(WORD notify_code,
                                     WORD id,
                                     HWND wnd_ctl,
                                     BOOL& handled) {   // NOLINT
  UNREFERENCED_PARAMETER(notify_code);
  UNREFERENCED_PARAMETER(wnd_ctl);

  CORE_LOG(L3, (_T("[YesNoDialog::OnClickedButton]")));
  ASSERT1(id == IDOK || id == IDCANCEL);

  #pragma warning(push)
  // C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled
  // by a case label.
  #pragma warning(disable : 4061)

  switch (id) {
    case IDOK:
      yes_clicked_ = true;
      break;

    case IDCANCEL:
      yes_clicked_ = false;
      break;

    default:
      ASSERT1(false);
      yes_clicked_ = false;
      break;
  }
  #pragma warning(pop)

  handled = true;
  SendMessage(WM_CLOSE, 0, 0);

  return 0;
}

LRESULT YesNoDialog::OnClose(UINT message,
                              WPARAM wparam,
                              LPARAM lparam,
                              BOOL& handled) {
  UNREFERENCED_PARAMETER(message);
  UNREFERENCED_PARAMETER(wparam);
  UNREFERENCED_PARAMETER(lparam);

  DestroyWindow();

  handled = TRUE;
  return 0;
}

LRESULT YesNoDialog::OnNCDestroy(UINT message,
                                 WPARAM wparam,
                                 LPARAM lparam,
                                 BOOL& handled) {
  UNREFERENCED_PARAMETER(message);
  UNREFERENCED_PARAMETER(wparam);
  UNREFERENCED_PARAMETER(lparam);

  CORE_LOG(L3, (_T("[YesNoDialog::OnNCDestroy]")));
  VERIFY1(message_loop_->RemoveMessageFilter(this));

  ::PostQuitMessage(0);

  handled = FALSE;  // Let ATL default processing handle the WM_NCDESTROY.
  return 0;
}

}  // namespace omaha

