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

#ifndef OMAHA_UI_UI_H_
#define OMAHA_UI_UI_H_

#include <atlbase.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/wtl_atlapp_wrapper.h"
// TODO(omaha3): Depending on how separate the UI is, we may want to separate
// the UI-specific resources, especially the dialogs. Can we handle bidi
// independent of languages? This would allow simple replacement of Omaha's
// UI without impacting the core or error messages.
#include "omaha/client/resource.h"
#include "omaha/ui/owner_draw_title_bar.h"

namespace omaha {

class HighresTimer;

class OmahaWndEvents {
 public:
  virtual ~OmahaWndEvents() {}
  virtual void DoClose() = 0;
  virtual void DoExit() = 0;
};

// This class turns off visual styles for all direct child windows under
// hwnd_parent that have the BS_FLAT style. BS_FLAT requires that visual styles
// be turned off.
class EnableFlatButtons {
 public:
  explicit EnableFlatButtons(HWND hwnd_parent);

 private:
  static BOOL CALLBACK EnableFlatButtonsProc(HWND hwnd, LPARAM lParam);

  DISALLOW_COPY_AND_ASSIGN(EnableFlatButtons);
};

// Implements the UI progress window.
class OmahaWnd
    : public CAxDialogImpl<OmahaWnd>,
      public OwnerDrawTitleBar,
      public CustomDlgColors,
      public CMessageFilter {
  typedef CAxDialogImpl<OmahaWnd> Base;
 public:
  virtual ~OmahaWnd();

  // The dialog resource ID as required by CAxDialogImpl.
  const int IDD;

  virtual HRESULT Initialize();

  // CMessageFilter interface method.
  BOOL PreTranslateMessage(MSG* msg) {
    return CWindow::IsDialogMessage(msg);
  }

  void SetEventSink(OmahaWndEvents* ev) { events_sink_ = ev; }

  // TODO(omaha3): Move these to constructor. They are fundamental to the UI.
  // TODO(omaha3): Move is_machine_ to CompleteWnd if we do not use UI displayed
  // events. There it will be used to create the URL if we do not move that too.
  void set_is_machine(bool is_machine) { is_machine_ = is_machine; }
  void set_bundle_name(const CString& name) { bundle_name_ = name; }

  virtual void Show();

  BEGIN_MSG_MAP(OmahaWnd)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_NCDESTROY, OnNCDestroy)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    CHAIN_MSG_MAP(Base)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 protected:
  struct ControlAttributes {
    const bool is_ignore_entry_;
    const bool is_visible_;
    const bool is_enabled_;
    const bool is_button_;
    const bool is_default_;
  };

  OmahaWnd(int dialog_id, CMessageLoop* message_loop, HWND parent);

  // Message and command handlers.
  LRESULT OnClose(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);       // NOLINT
  LRESULT OnNCDestroy(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);   // NOLINT
  LRESULT OnCancel(WORD notify_code, WORD id, HWND wnd_ctl, BOOL& handled);     // NOLINT

  // Handles requests to close the window. Returns true if the window is closed.
  virtual bool MaybeCloseWindow() = 0;

  // Handles entering the completion mode.
  // Returns whether to continue with OnComplete operations.
  bool OnComplete();

  // TODO(omaha3): May need to be public to implement
  // COMPLETION_CODE_SUCCESS_CLOSE_UI outside the UI.
  // Closes the window.
  HRESULT CloseWindow();
  void InitializeDialog();

  HRESULT EnableClose(bool enable);
  HRESULT EnableSystemCloseButton(bool enable);

  void SetControlAttributes(int control_id,
                            const ControlAttributes& attributes);

  void SetVisible(bool visible) {
    ShowWindow(visible ? SW_SHOWNORMAL : SW_HIDE);
  }

  DWORD thread_id() { return thread_id_; }
  CMessageLoop* message_loop() { return message_loop_; }
  bool is_complete() { return is_complete_; }
  bool is_close_enabled() { return is_close_enabled_; }
  bool is_machine() { return is_machine_; }
  const CString& bundle_name() { return bundle_name_; }

  static const ControlAttributes kVisibleTextAttributes;
  static const ControlAttributes kDefaultActiveButtonAttributes;
  static const ControlAttributes kDisabledButtonAttributes;
  static const ControlAttributes kNonDefaultActiveButtonAttributes;
  static const ControlAttributes kVisibleImageAttributes;
  static const ControlAttributes kDisabledNonButtonAttributes;

 private:
  HRESULT SetWindowIcon();

  void MaybeRequestExitProcess();
  void RequestExitProcess();

  CMessageLoop* message_loop_;
  HWND parent_;
  DWORD thread_id_;

  bool is_complete_;
  bool is_close_enabled_;

  OmahaWndEvents* events_sink_;

  bool is_machine_;
  CString bundle_name_;


  // Handle to large icon to show when ALT-TAB
  scoped_hicon hicon_;

  CFont default_font_;
  CFont font_;
  CFont error_font_;

  CustomProgressBarCtrl progress_bar_;

  DISALLOW_EVIL_CONSTRUCTORS(OmahaWnd);
};

// Registers the specified common control classes from the common control DLL.
// Calls are cumulative, meaning control_classes are added to existing classes.
// UIs that use common controls should call this method to ensure that the UI
// supports visual styles.
HRESULT InitializeCommonControls(DWORD control_classes);

}  // namespace omaha

#endif  // OMAHA_UI_UI_H_
