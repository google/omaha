// Copyright 2010 Google Inc.
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

#ifndef OMAHA_UI_SPLASH_SCREEN_H_
#define OMAHA_UI_SPLASH_SCREEN_H_

#include <windows.h>

#include "omaha/base/smart_handle.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/thread.h"
#include "omaha/base/wtl_atlapp_wrapper.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "omaha/ui/owner_draw_title_bar.h"

namespace omaha {

// Displays a splash screen while lengthy operations occur. Before the lengthy
// operation, the caller creates an instance of this class and calls Show()
// function on it. The Show() function then creates a thread to display the
// splash screen. The caller must call Dismiss() function to hide and destroy
// the splash screen once the lengthy operation is done.
class SplashScreen
    : public CAxDialogImpl<SplashScreen>,
      public OwnerDrawTitleBar,
      public CustomDlgColors,
      public Runnable {
 public:
  explicit SplashScreen(const CString& bundle_name);

  // The desctructor waits up to 60 seconds for the message loop thread to exit
  // if it is running.
  virtual ~SplashScreen();

  // The dialog resource ID as required by CAxDialogImpl.
  const int IDD;

  // Spawns a thread which creates and shows the window.
  void Show();

  // Closes the window gradually if the window is visible.
  void Dismiss();

  // Runnable interface method.
  virtual void Run();

  BEGIN_MSG_MAP(SplashScreen)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    CHAIN_MSG_MAP(OwnerDrawTitleBar)
    CHAIN_MSG_MAP(CustomDlgColors)
  END_MSG_MAP()

 private:
  friend class SplashScreenTest;

  // States that help to determine the splash screen life cycle and
  // allowed operations at each stage.
  enum WindowState {
    STATE_CREATED,
    STATE_INITIALIZED,
    STATE_SHOW_NORMAL,
    STATE_FADING,
    STATE_CLOSED,
  };

  // Creates the window and adjusts its size and appearance.
  HRESULT Initialize();

  void InitProgressBar();
  void EnableSystemButtons(bool enable);

  void SwitchToState(WindowState new_state);

  // Posts a WM_CLOSE message to close the window if the window is valid.
  void Close();

  // Message and command handlers.
  LRESULT OnTimer(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);     // NOLINT(runtime/references)
  LRESULT OnClose(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);     // NOLINT(runtime/references)
  LRESULT OnDestroy(UINT msg,
                    WPARAM wparam,
                    LPARAM lparam,
                    BOOL& handled);   // NOLINT(runtime/references)

  LLock lock_;    // Lock for access synchronization of this object.
  Thread thread_;  // Thread that creates the window and runs the message loop.
  WindowState state_;   // State of the object.

  // Indicates whether timer for fading effect has been created.
  bool timer_created_;

  int alpha_index_;   // Array index of current alpha blending value.

  CString text_;  // Message text shows on the window.
  CFont default_font_;
  CFont font_;
  CString caption_;  // Dialog title.

  // Handle to large icon to show when ALT-TAB
  scoped_hicon hicon_;
  CustomProgressBarCtrl progress_bar_;

  DISALLOW_COPY_AND_ASSIGN(SplashScreen);
};

}  // namespace omaha

#endif  // OMAHA_UI_SPLASH_SCREEN_H_

