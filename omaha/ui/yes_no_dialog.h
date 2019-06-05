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

#ifndef OMAHA_UI_YES_NO_DIALOG_H_
#define OMAHA_UI_YES_NO_DIALOG_H_

#include <windows.h>

#include "omaha/base/wtl_atlapp_wrapper.h"
#include "omaha/client/resource.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "omaha/ui/owner_draw_title_bar.h"

namespace omaha {

class YesNoDialog
    : public CAxDialogImpl<YesNoDialog>,
      public OwnerDrawTitleBar,
      public CustomDlgColors,
      public CMessageFilter {
  typedef CAxDialogImpl<YesNoDialog> Base;

 public:
  static const int IDD = IDD_YES_NO;

  YesNoDialog(CMessageLoop* message_loop, HWND parent);
  ~YesNoDialog();

  HRESULT Initialize(const CString& yes_no_title,
                     const CString& yes_no_text);
  HRESULT Show();

  bool yes_clicked() const {
    return yes_clicked_;
  }

  // CMessageFilter interface method.
  BOOL PreTranslateMessage(MSG* msg) {
    return CWindow::IsDialogMessage(msg);
  }

  BEGIN_MSG_MAP(YesNoDialog)
    COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedButton)
    COMMAND_ID_HANDLER(IDCANCEL, OnClickedButton)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_NCDESTROY, OnNCDestroy)
    CHAIN_MSG_MAP(Base)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  // Message and command handlers.
  LRESULT OnClickedButton(WORD notify_code,
                          WORD id,
                          HWND wnd_ctl,
                          BOOL& handled);  // NOLINT(runtime/references)
  LRESULT OnClose(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT(runtime/references)
  LRESULT OnNCDestroy(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT(runtime/references)

  CMessageLoop* message_loop_;
  HWND parent_;
  bool yes_clicked_;

  // Handle to large icon to show when ALT-TAB.
  scoped_hicon hicon_;

  CFont default_font_;

  DISALLOW_COPY_AND_ASSIGN(YesNoDialog);
};

}  // namespace omaha

#endif  // OMAHA_UI_YES_NO_DIALOG_H_

