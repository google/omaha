// Copyright 2013 Google Inc.
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
//
// owner_draw_title_bar.h : Allow for a owner-drawn Title Bar. Currently, this
// Title Bar class creates a Title Bar that has the specified background color,
// no icon or caption, with owner-drawn minimize/maximize/close buttons.
//
// Steps:
// * Derive your ATL window class from OwnerDrawTitleBar.
// * Add a CHAIN_MSG_MAP in your ATL message map to OwnerDrawTitleBar.
//   OwnerDrawTitleBar will handle WM_WINDOWPOSCHANGED in the chained msg map.
// * Call CreateOwnerDrawTitleBar() from OnCreate/OnInitDialog.

#ifndef OMAHA_UI_OWNER_DRAW_TITLE_BAR_H_
#define OMAHA_UI_OWNER_DRAW_TITLE_BAR_H_

#include <windows.h>

// atlapp.h needs to be included before the other ATL/WTL headers. Omaha uses a
// wrapper for atlapp.h which is wtl_atlapp_wrapper.h. Lint regards
// wtl_atlapp_wrapper.h as a project-specific header and is unhappy with the
// header order. Hence all the NOLINT for the atl/wtl headers.
#include "omaha/base/wtl_atlapp_wrapper.h"
#include <atlbase.h>   // NOLINT
#include <atlctrls.h>  // NOLINT
#include <atlframe.h>  // NOLINT
#include <atlstr.h>    // NOLINT
#include <atltypes.h>  // NOLINT
#include <atlwin.h>    // NOLINT

#include "base\basictypes.h"

namespace omaha {

class CaptionButton : public CWindowImpl<CaptionButton, CButton>,
                      public COwnerDraw<CaptionButton> {
 public:
  // This class uses DECLARE_WND_CLASS_EX because DECLARE_WND_CLASS includes the
  // CS_DBLCLKS style which we want to avoid.
  DECLARE_WND_CLASS_EX(_T("CaptionButton"),
                       CS_HREDRAW | CS_VREDRAW,
                       COLOR_WINDOW)

  BEGIN_MSG_MAP(CaptionButton)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    MESSAGE_HANDLER(WM_MOUSEHOVER, OnMouseHover)
    MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    CHAIN_MSG_MAP_ALT(COwnerDraw<CaptionButton>, 1)
    DEFAULT_REFLECTION_HANDLER()
  END_MSG_MAP()

  CaptionButton();
  virtual ~CaptionButton();
  LRESULT OnCreate(UINT msg,
                   WPARAM wparam,
                   LPARAM lparam,
                   BOOL& handled);  // NOLINT
  LRESULT OnMouseMessage(UINT msg,
                         WPARAM wparam,
                         LPARAM lparam,
                         BOOL& handled);  // NOLINT
  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnMouseMove(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnMouseHover(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnMouseLeave(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT

  void DrawItem(LPDRAWITEMSTRUCT draw_item_struct);

  COLORREF bk_color() const;
  void set_bk_color(COLORREF bk_color);

  CString tool_tip_text() const;
  void set_tool_tip_text(const CString& tool_tip_text);

 private:
  // Derived classes override this method and return desired button design.
  virtual HRGN GetButtonRgn(int rgn_width, int rgn_height) = 0;

  COLORREF bk_color_;
  CBrush foreground_brush_;
  CBrush frame_brush_;

  CToolTipCtrl tool_tip_window_;
  CString tool_tip_text_;
  bool is_tracking_mouse_events_;
  bool is_mouse_hovering_;

  DISALLOW_COPY_AND_ASSIGN(CaptionButton);
};

class CloseButton : public CaptionButton {
 public:
  CloseButton();

 private:
  virtual HRGN GetButtonRgn(int rgn_width, int rgn_height);

  DISALLOW_COPY_AND_ASSIGN(CloseButton);
};

class MinimizeButton : public CaptionButton {
 public:
  MinimizeButton();

 private:
  virtual HRGN GetButtonRgn(int rgn_width, int rgn_height);

  DISALLOW_COPY_AND_ASSIGN(MinimizeButton);
};

class MaximizeButton : public CaptionButton {
 public:
  MaximizeButton();

 private:
  virtual HRGN GetButtonRgn(int rgn_width, int rgn_height);

  DISALLOW_COPY_AND_ASSIGN(MaximizeButton);
};

class OwnerDrawTitleBarWindow : public CWindowImpl<OwnerDrawTitleBarWindow> {
 public:
  // This class uses DECLARE_WND_CLASS_EX because DECLARE_WND_CLASS includes the
  // CS_DBLCLKS style which we want to avoid.
  DECLARE_WND_CLASS_EX(_T("OwnerDrawTitleBarWindow"),
                       CS_HREDRAW | CS_VREDRAW,
                       COLOR_WINDOW)

  BEGIN_MSG_MAP(OwnerDrawTitleBarWindow)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
    MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    COMMAND_ID_HANDLER(kButtonClose, OnClose)
    COMMAND_ID_HANDLER(kButtonMaximize, OnMaximize)
    COMMAND_ID_HANDLER(kButtonMinimize, OnMinimize)
    REFLECT_NOTIFICATIONS()
  END_MSG_MAP()

  OwnerDrawTitleBarWindow();
  virtual ~OwnerDrawTitleBarWindow();

  LRESULT OnCreate(UINT msg,
                   WPARAM wparam,
                   LPARAM lparam,
                   BOOL& handled);  // NOLINT
  LRESULT OnDestroy(UINT msg,
                    WPARAM wparam,
                    LPARAM lparam,
                    BOOL& handled);  // NOLINT
  LRESULT OnMouseMove(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnLButtonDown(UINT msg,
                        WPARAM wparam,
                        LPARAM lparam,
                        BOOL& handled);  // NOLINT
  LRESULT OnLButtonUp(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT
  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnClose(WORD notify_code,
                  WORD id,
                  HWND hwnd_ctrl,
                  BOOL& handled);  // NOLINT
  LRESULT OnMaximize(WORD notify_code,
                     WORD id,
                     HWND hwnd_ctrl,
                     BOOL& handled);  // NOLINT
  LRESULT OnMinimize(WORD notify_code,
                     WORD id,
                     HWND hwnd_ctrl,
                     BOOL& handled);  // NOLINT

  void RecalcLayout();

  COLORREF bk_color() const;
  void set_bk_color(COLORREF bk_color);

 private:
  void CreateCaptionButtons();
  void UpdateButtonState(const CMenuHandle& menu,
                         UINT button_sc_id,
                         const int button_margin,
                         CaptionButton* button,
                         CRect* button_rect);
  void MoveWindowToDragPosition(HWND hwnd, int dx, int dy);

  enum ButtonIds {
    kButtonClose = 1,
    kButtonMaximize,
    kButtonMinimize,
  };

  CPoint current_drag_position_;
  COLORREF bk_color_;

  CloseButton close_button_;
  MinimizeButton minimize_button_;

  DISALLOW_COPY_AND_ASSIGN(OwnerDrawTitleBarWindow);
};

class OwnerDrawTitleBar {
 public:
  BEGIN_MSG_MAP(OwnerDrawTitleBar)
  END_MSG_MAP()

  OwnerDrawTitleBar();
  virtual ~OwnerDrawTitleBar();

  void CreateOwnerDrawTitleBar(HWND parent_hwnd,
                               HWND title_bar_spacer_hwnd,
                               COLORREF bk_color);

  // Call this method after modifying the system menu. The caption buttons will
  // then be laid out in accordance with the new state of the corresponding
  // system menu items.
  void RecalcLayout();

 private:
  CRect ComputeTitleBarClientRect(HWND parent_hwnd, HWND title_bar_spacer_hwnd);

  OwnerDrawTitleBarWindow title_bar_window_;

  DISALLOW_COPY_AND_ASSIGN(OwnerDrawTitleBar);
};

// Allow for custom text color and custom background color for dialog elements.
//
// Steps:
// * Derive your ATL dialog class from CustomDlgColors.
// * Add a CHAIN_MSG_MAP in your ATL message map to CustomDlgColors.
//   CustomDlgColors will handle WM_CTLCOLOR{XXX} in the chained msg map.
// * Call SetCustomDlgColors() from OnInitDialog.
class CustomDlgColors {
 public:
  BEGIN_MSG_MAP(CustomDlgColors)
    MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtrlColor)
    MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtrlColor)
  END_MSG_MAP()

  CustomDlgColors();
  virtual ~CustomDlgColors();

  void SetCustomDlgColors(COLORREF text_color, COLORREF bk_color);

  LRESULT OnCtrlColor(UINT msg,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& handled);  // NOLINT

 private:
  COLORREF text_color_;
  COLORREF bk_color_;
  CBrush bk_brush_;

  DISALLOW_COPY_AND_ASSIGN(CustomDlgColors);
};

class CustomProgressBarCtrl : public CWindowImpl<CustomProgressBarCtrl> {
 public:
  CustomProgressBarCtrl();
  virtual ~CustomProgressBarCtrl();

  BEGIN_MSG_MAP(CustomProgressBarCtrl)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
    MESSAGE_HANDLER(PBM_SETPOS, OnSetPos)
    MESSAGE_HANDLER(PBM_SETMARQUEE, OnSetMarquee)
    MESSAGE_HANDLER(PBM_SETBARCOLOR, OnSetBarColor)
    MESSAGE_HANDLER(PBM_SETBKCOLOR, OnSetBkColor)
  END_MSG_MAP()

  LRESULT OnEraseBkgnd(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnPaint(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnTimer(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT

  LRESULT OnSetPos(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnSetMarquee(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnSetBarColor(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT
  LRESULT OnSetBkColor(UINT msg,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);  // NOLINT

 private:
  void GradientFill(HDC dc,
                    const RECT& rect,
                    COLORREF top_color,
                    COLORREF bottom_color);

  const int kMinPosition;
  const int kMaxPosition;
  const int kMarqueeWidth;
  const UINT_PTR kMarqueeTimerId;

  bool is_marquee_mode_;
  int current_position_;

  COLORREF  bar_color_light_;
  COLORREF bar_color_dark_;
  COLORREF empty_fill_color_;
  CBrush empty_frame_brush_;

  DISALLOW_COPY_AND_ASSIGN(CustomProgressBarCtrl);
};

}  // namespace omaha

#endif  // OMAHA_UI_OWNER_DRAW_TITLE_BAR_H_

