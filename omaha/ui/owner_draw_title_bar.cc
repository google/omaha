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
// Classes that help creating a owner-drawn Titlebar.

#include "omaha/ui/owner_draw_title_bar.h"

#include <vector>

#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/client/resource.h"

namespace omaha {

namespace {

// This function returns |true| if the system is in high contrast mode.
bool IsHighContrastOn() {
  HIGHCONTRAST hc = {0};
  hc.cbSize = sizeof(HIGHCONTRAST);

  if (!::SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &hc, 0)) {
    return false;
  }

  return hc.dwFlags & HCF_HIGHCONTRASTON;
}

// This function returns the system color corresponding to
// |high_contrast_color_index| if the system is in high contrast mode.
// Otherwise, it returns the |normal_color|.
COLORREF GetColor(COLORREF normal_color, int high_contrast_color_index) {
  return IsHighContrastOn() ? ::GetSysColor(high_contrast_color_index) :
                              normal_color;
}

// This function returns the system color brush corresponding to
// |high_contrast_color_index| if the system is in high contrast mode.
// Otherwise, it returns the |normal_brush|.
HBRUSH GetColorBrush(const CBrush& normal_brush,
                     int high_contrast_color_index) {
  return IsHighContrastOn() ? ::GetSysColorBrush(high_contrast_color_index) :
                              HBRUSH{normal_brush};
}

}  // namespace

CaptionButton::CaptionButton()
    : bk_color_(RGB(0, 0, 0)),
      foreground_brush_(::CreateSolidBrush(kCaptionForegroundColor)),
      frame_brush_(::CreateSolidBrush(kCaptionFrameColor)),
      is_tracking_mouse_events_(false),
      is_mouse_hovering_(false) {
}

CaptionButton::~CaptionButton() {
}

LRESULT CaptionButton::OnCreate(UINT,
                                WPARAM,
                                LPARAM,
                                BOOL& handled) {  // NOLINT
  handled = FALSE;

  VERIFY1(tool_tip_window_.Create(m_hWnd));
  ASSERT1(tool_tip_window_.IsWindow());
  ASSERT1(!tool_tip_text_.IsEmpty());

  tool_tip_window_.SetDelayTime(TTDT_AUTOMATIC, 2000);

  tool_tip_window_.Activate(TRUE);
  tool_tip_window_.AddTool(m_hWnd, tool_tip_text_.GetString());

  return 0;
}

LRESULT CaptionButton::OnMouseMessage(UINT msg,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      BOOL& handled) {  // NOLINT
  handled = FALSE;

  if (tool_tip_window_.IsWindow()) {
    MSG relay_msg = {m_hWnd, msg, wparam, lparam};
    tool_tip_window_.RelayEvent(&relay_msg);
  }

  return 1;
}

LRESULT CaptionButton::OnEraseBkgnd(UINT,
                                    WPARAM,
                                    LPARAM,
                                    BOOL& handled) {  // NOLINT
  // The background and foreground are both rendered in DrawItem().
  handled = TRUE;
  return 1;
}

LRESULT CaptionButton::OnMouseMove(UINT,
                                   WPARAM,
                                   LPARAM,
                                   BOOL& handled) {  // NOLINT
  handled = FALSE;

  if (!is_tracking_mouse_events_) {
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_HOVER | TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    tme.dwHoverTime = 1;

    is_tracking_mouse_events_ = !!_TrackMouseEvent(&tme);
  }

  return 0;
}

LRESULT CaptionButton::OnMouseHover(UINT,
                                    WPARAM,
                                    LPARAM,
                                    BOOL& handled) {  // NOLINT
  handled = FALSE;

  if (!is_mouse_hovering_) {
    is_mouse_hovering_ = true;

    Invalidate(FALSE);
    VERIFY1(UpdateWindow());
  }

  return 0;
}

LRESULT CaptionButton::OnMouseLeave(UINT,
                                    WPARAM,
                                    LPARAM,
                                    BOOL& handled) {  // NOLINT
  handled = FALSE;

  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_CANCEL | TME_HOVER | TME_LEAVE;
  tme.hwndTrack = m_hWnd;

  VERIFY1(_TrackMouseEvent(&tme));

  is_tracking_mouse_events_ = false;
  is_mouse_hovering_ = false;

  Invalidate(FALSE);
  VERIFY1(UpdateWindow());

  return 0;
}

void CaptionButton::DrawItem(LPDRAWITEMSTRUCT draw_item_struct) {
  CDCHandle dc(draw_item_struct->hDC);

  CRect button_rect;
  VERIFY1(GetClientRect(&button_rect));

  COLORREF bk_color(is_mouse_hovering_ ? GetColor(kCaptionBkHover,
                                                  COLOR_ACTIVECAPTION) :
                                         GetColor(bk_color_, COLOR_WINDOW));
  dc.FillSolidRect(&button_rect, bk_color);

  int rgn_width = button_rect.Width() * 12 / 31;
  int rgn_height = button_rect.Height() * 12 / 31;
  CRgn rgn(GetButtonRgn(rgn_width, rgn_height));

  // Center the button in the outer button rect.
  rgn.OffsetRgn((button_rect.Width() - rgn_width) / 2,
                (button_rect.Height() - rgn_height) / 2);

  VERIFY1(dc.FillRgn(rgn, GetColorBrush(foreground_brush_, COLOR_WINDOWTEXT)));

  const UINT button_state(draw_item_struct->itemState);
  if (button_state & ODS_FOCUS && button_state & ODS_SELECTED) {
    VERIFY1(dc.FrameRect(&button_rect, frame_brush_));
  }
}

COLORREF CaptionButton::bk_color() const {
  return bk_color_;
}

void CaptionButton::set_bk_color(COLORREF bk_color) {
  bk_color_ = bk_color;
}

CString CaptionButton::tool_tip_text() const {
  return tool_tip_text_;
}

void CaptionButton::set_tool_tip_text(const CString& tool_tip_text) {
  tool_tip_text_ = tool_tip_text;
}

CloseButton::CloseButton() {
  CString tool_tip_text;
  VERIFY1(tool_tip_text.LoadString(IDS_CLOSE_BUTTON));
  set_tool_tip_text(tool_tip_text);
}

HRGN CloseButton::GetButtonRgn(int rgn_width, int rgn_height) {
  // A single 4x4 rectangular center and criss-crossing 2x2 overlapping
  // rectangles form the close button.
  int square_side = std::min(rgn_width, rgn_height) / 2 * 2;
  int center_point = square_side / 2;

  CRect center_rect(0, 0, 4, 4);
  center_rect.OffsetRect(center_point - 2, center_point - 2);
  CRgnHandle rgn(::CreateRectRgnIndirect(&center_rect));

  for (int i = 0; i <= square_side - 2; i++) {
    CRgn rgn_nw_to_se(::CreateRectRgn(i, i, i + 2, i + 2));
    VERIFY1(rgn.CombineRgn(rgn_nw_to_se, RGN_OR) != RGN_ERROR);

    CRgn rgn_sw_to_ne(::CreateRectRgn(i,
                                      square_side - i - 2,
                                      i + 2,
                                      square_side - i));
    VERIFY1(rgn.CombineRgn(rgn_sw_to_ne, RGN_OR) != RGN_ERROR);
  }

  rgn.OffsetRgn((rgn_width - square_side) / 2, (rgn_height - square_side) / 2);
  return rgn;
}

MinimizeButton::MinimizeButton() {
  CString tool_tip_text;
  VERIFY1(tool_tip_text.LoadString(IDS_MINIMIZE_BUTTON));
  set_tool_tip_text(tool_tip_text);
}

HRGN MinimizeButton::GetButtonRgn(int rgn_width, int rgn_height) {
  // The Minimize button is a single rectangle.
  CRect minimize_button_rect(0, 0, rgn_width, 2);
  int center_point = rgn_height / 2;
  minimize_button_rect.OffsetRect(0, center_point - 1);

  return ::CreateRectRgnIndirect(&minimize_button_rect);
}

MaximizeButton::MaximizeButton() {
  CString tool_tip_text;
  VERIFY1(tool_tip_text.LoadString(IDS_MAXIMIZE_BUTTON));
  set_tool_tip_text(tool_tip_text);
}

HRGN MaximizeButton::GetButtonRgn(int rgn_width, int rgn_height) {
  // Overlapping outer and inner rectangles form the maximize button.
  const RECT maximize_button_rects[] = {
    {0, 0, rgn_width, rgn_height}, {1, 2, rgn_width - 1, rgn_height - 1}
  };

  CRgnHandle rgn(::CreateRectRgnIndirect(&maximize_button_rects[0]));
  CRgn rgn_temp(::CreateRectRgnIndirect(&maximize_button_rects[1]));
  VERIFY1(rgn.CombineRgn(rgn_temp, RGN_DIFF) != RGN_ERROR);
  return rgn;
}

OwnerDrawTitleBarWindow::OwnerDrawTitleBarWindow()
    : current_drag_position_(-1, -1),
      bk_color_(RGB(0, 0, 0)) {
}

OwnerDrawTitleBarWindow::~OwnerDrawTitleBarWindow() {
}

LRESULT OwnerDrawTitleBarWindow::OnCreate(UINT,
                                          WPARAM,
                                          LPARAM,
                                          BOOL& handled) {  // NOLINT
  handled = FALSE;

  CreateCaptionButtons();
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnDestroy(UINT,
                                           WPARAM,
                                           LPARAM,
                                           BOOL& handled) {  // NOLINT
  handled = FALSE;

  if (close_button_.IsWindow()) {
    VERIFY1(close_button_.DestroyWindow());
  }

  if (minimize_button_.IsWindow()) {
    VERIFY1(minimize_button_.DestroyWindow());
  }

  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMouseMove(UINT,
                                             WPARAM wparam,
                                             LPARAM,
                                             BOOL& handled) {  // NOLINT
  handled = FALSE;
  if (current_drag_position_.x == -1 || wparam != MK_LBUTTON) {
    return 0;
  }

  CPoint pt;
  VERIFY1(::GetCursorPos(&pt));
  int dx = pt.x - current_drag_position_.x;
  int dy = pt.y - current_drag_position_.y;
  current_drag_position_ = pt;

  MoveWindowToDragPosition(GetParent(), dx, dy);

  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnLButtonDown(UINT,
                                               WPARAM,
                                               LPARAM,
                                               BOOL& handled) {  // NOLINT
  handled = FALSE;

  ::GetCursorPos(&current_drag_position_);
  SetCapture();
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnLButtonUp(UINT,
                                             WPARAM,
                                             LPARAM,
                                             BOOL& handled) {  // NOLINT
  handled = FALSE;

  current_drag_position_.x = -1;
  current_drag_position_.y = -1;
  ReleaseCapture();

  // Reset the parent to be the active window.
  VERIFY1(::SetActiveWindow(GetParent()));
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnEraseBkgnd(UINT,
                                              WPARAM wparam,
                                              LPARAM,
                                              BOOL& handled) {  // NOLINT
  handled = TRUE;

  CDC dc(reinterpret_cast<HDC>(wparam));
  CRect rect;
  VERIFY1(GetClientRect(&rect));

  dc.FillSolidRect(&rect, GetColor(bk_color_, COLOR_WINDOW));
  return 1;
}

LRESULT OwnerDrawTitleBarWindow::OnClose(WORD,
                                         WORD,
                                         HWND,
                                         BOOL& handled) {  // NOLINT
  handled = FALSE;

  VERIFY1(::PostMessage(GetParent(),
                        WM_SYSCOMMAND,
                        MAKEWPARAM(SC_CLOSE, 0),
                        0));
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMaximize(WORD,
                                            WORD,
                                            HWND,
                                            BOOL& handled) {  // NOLINT
  handled = FALSE;

  VERIFY1(::PostMessage(GetParent(),
                        WM_SYSCOMMAND,
                        MAKEWPARAM(SC_CLOSE, 0),
                        0));
  return 0;
}

LRESULT OwnerDrawTitleBarWindow::OnMinimize(WORD,
                                            WORD,
                                            HWND,
                                            BOOL& handled) {  // NOLINT
  handled = FALSE;

  VERIFY1(::PostMessage(GetParent(),
                        WM_SYSCOMMAND,
                        MAKEWPARAM(SC_MINIMIZE, 0),
                        0));
  return 0;
}

void OwnerDrawTitleBarWindow::CreateCaptionButtons() {
  close_button_.set_bk_color(bk_color_);
  minimize_button_.set_bk_color(bk_color_);

  CRect button_rect(0,
                    0,
                    ::GetSystemMetrics(SM_CXSIZE),
                    ::GetSystemMetrics(SM_CYSIZE));

  VERIFY1(minimize_button_.Create(m_hWnd,
                                  button_rect,
                                  NULL,
                                  WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                  0,
                                  kButtonMinimize));

  VERIFY1(close_button_.Create(m_hWnd,
                               button_rect,
                               NULL,
                               WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                               0,
                               kButtonClose));

  // Lay out the buttons.
  RecalcLayout();
}

// This method handles four major button states:
// * Button window does not exist. Nothing to do in this case.
// * Corresponding menu (item) does not exist. Hide button.
// * Corresponding menu item disabled. Disable and Show button.
// * Corresponding menu item enabled. Enable and Show button.
// TODO(omaha3): It may make sense to add detection for
// WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX, although these styles are
// limiting in that they do not allow for a "disabled" state.
void OwnerDrawTitleBarWindow::UpdateButtonState(const CMenuHandle& menu,
                                                UINT button_sc_id,
                                                const int button_margin,
                                                CaptionButton* button,
                                                CRect* button_rect) {
  ASSERT1(button);
  ASSERT1(button_rect);

  if (!button->IsWindow()) {
    return;
  }

  int state(-1);
  if (!menu.IsNull() && menu.IsMenu()) {
    state = menu.GetMenuState(button_sc_id, MF_BYCOMMAND);
  }

  if (state == -1) {
    button->ShowWindow(SW_HIDE);
    return;
  }

  button->EnableWindow(!(state & (MF_GRAYED | MF_DISABLED)));
  VERIFY1(button->SetWindowPos(NULL, button_rect, SWP_NOZORDER |
                                                  SWP_SHOWWINDOW));
  button_rect->OffsetRect(-button_rect->Width() - button_margin, 0);
}

// (Re)compute and lay out the Title Bar.
void OwnerDrawTitleBarWindow::RecalcLayout() {
  CRect title_bar_rect;
  VERIFY1(GetClientRect(&title_bar_rect));

  const int button_margin = title_bar_rect.Height() / 5;
  title_bar_rect.DeflateRect(button_margin, button_margin);

  // width == height.
  int button_height(title_bar_rect.Height());
  int button_width(button_height);

  // Position controls from the Close button to the Minimize button.
  CRect button_rect(title_bar_rect.right - button_width,
                    title_bar_rect.top,
                    title_bar_rect.right,
                    title_bar_rect.bottom);

  CMenuHandle menu(::GetSystemMenu(GetParent(), FALSE));
  UpdateButtonState(menu,
                    SC_CLOSE,
                    button_margin,
                    &close_button_,
                    &button_rect);
  UpdateButtonState(menu,
                    SC_MINIMIZE,
                    button_margin,
                    &minimize_button_,
                    &button_rect);
}

void OwnerDrawTitleBarWindow::MoveWindowToDragPosition(HWND hwnd,
                                                       int dx,
                                                       int dy) {
  CRect rect;
  VERIFY1(::GetWindowRect(hwnd, &rect));

  rect.OffsetRect(dx, dy);
  VERIFY1(::SetWindowPos(hwnd,
                         NULL,
                         rect.left,
                         rect.top,
                         0,
                         0,
                         SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE));
}

COLORREF OwnerDrawTitleBarWindow::bk_color() const {
  return bk_color_;
}

void OwnerDrawTitleBarWindow::set_bk_color(COLORREF bk_color) {
  bk_color_ = bk_color;
}

OwnerDrawTitleBar::OwnerDrawTitleBar() {
}

OwnerDrawTitleBar::~OwnerDrawTitleBar() {
}

void OwnerDrawTitleBar::CreateOwnerDrawTitleBar(HWND parent_hwnd,
                                                HWND title_bar_spacer_hwnd,
                                                COLORREF bk_color) {
  ASSERT1(parent_hwnd);

  CRect title_bar_client_rect(ComputeTitleBarClientRect(parent_hwnd,
                                                        title_bar_spacer_hwnd));

  // This title bar is a child window and occupies the top portion of the parent
  // dialog box window. DS_MODALFRAME and WS_BORDER are incompatible with this
  // title bar. WS_DLGFRAME is recommended as well.
  LONG parent_style = ::GetWindowLong(parent_hwnd, GWL_STYLE);
  ASSERT1(!(parent_style & DS_MODALFRAME));
  ASSERT1(!(parent_style & WS_BORDER));
  ASSERT1(parent_style & WS_DLGFRAME);

  title_bar_window_.set_bk_color(bk_color);
  VERIFY1(title_bar_window_.Create(parent_hwnd,
                                   title_bar_client_rect,
                                   NULL,
                                   WS_VISIBLE | WS_CHILD |
                                   WS_CLIPSIBLINGS | WS_CLIPCHILDREN));
}

void OwnerDrawTitleBar::RecalcLayout() {
  ASSERT1(title_bar_window_.IsWindow());
  title_bar_window_.RecalcLayout();
}

CRect OwnerDrawTitleBar::ComputeTitleBarClientRect(HWND parent_hwnd,
                                                   HWND title_bar_spacer_hwnd) {
  ASSERT1(parent_hwnd);

  CRect parent_client_rect;
  VERIFY1(::GetClientRect(parent_hwnd, &parent_client_rect));
  CRect title_bar_client_rect(parent_client_rect);

  CRect title_bar_spacer_client_rect;
  VERIFY1(::GetClientRect(title_bar_spacer_hwnd,
                          &title_bar_spacer_client_rect));
  const int title_bar_height(title_bar_spacer_client_rect.Height());

  title_bar_client_rect.bottom = title_bar_client_rect.top + title_bar_height;

  return title_bar_client_rect;
}

CustomDlgColors::CustomDlgColors()
    : text_color_(RGB(0xFF, 0xFF, 0xFF)),
      bk_color_(RGB(0, 0, 0)) {
}

CustomDlgColors::~CustomDlgColors() {
}

void CustomDlgColors::SetCustomDlgColors(COLORREF text_color,
                                         COLORREF bk_color) {
  text_color_ = text_color;
  bk_color_ = bk_color;

  ASSERT1(bk_brush_.IsNull());
  VERIFY1(bk_brush_.CreateSolidBrush(bk_color_));
}

LRESULT CustomDlgColors::OnCtrlColor(UINT,
                                     WPARAM wparam,
                                     LPARAM,
                                     BOOL& handled) {  // NOLINT
  handled = TRUE;

  CDCHandle dc(reinterpret_cast<HDC>(wparam));
  SetBkColor(dc, GetColor(bk_color_, COLOR_WINDOW));
  SetTextColor(dc, GetColor(text_color_, COLOR_WINDOWTEXT));

  return reinterpret_cast<LRESULT>(GetColorBrush(bk_brush_, COLOR_WINDOW));
}

CustomProgressBarCtrl::CustomProgressBarCtrl()
    : kMinPosition(0),
      kMaxPosition(100),
      kMarqueeWidth(20),
      kMarqueeTimerId(111),
      is_marquee_mode_(false),
      current_position_(kMinPosition),
      bar_color_light_(kProgressBarLightColor),
      bar_color_dark_(kProgressBarDarkColor),
      empty_fill_color_(kProgressEmptyFillColor),
      empty_frame_brush_(::CreateSolidBrush(kProgressEmptyFrameColor)) {
}

CustomProgressBarCtrl::~CustomProgressBarCtrl() {
}

LRESULT CustomProgressBarCtrl::OnEraseBkgnd(UINT,
                                            WPARAM,
                                            LPARAM,
                                            BOOL& handled) {  // NOLINT
  // The background and foreground are both rendered in OnPaint().
  handled = TRUE;
  return 1;
}

void CustomProgressBarCtrl::GradientFill(HDC dc,
                                         const RECT& rect,
                                         COLORREF top_color,
                                         COLORREF bottom_color) {
  TRIVERTEX tri_vertex[] = {
    {
      rect.left,
      rect.top,
      static_cast<COLOR16>(GetRValue(top_color) << 8),
      static_cast<COLOR16>(GetGValue(top_color) << 8),
      static_cast<COLOR16>(GetBValue(top_color) << 8),
      0
    },
    {  // NOLINT
      rect.right,
      rect.bottom,
      static_cast<COLOR16>(GetRValue(bottom_color) << 8),
      static_cast<COLOR16>(GetGValue(bottom_color) << 8),
      static_cast<COLOR16>(GetBValue(bottom_color) << 8),
      0
    },
  };

  GRADIENT_RECT gradient_rect = {0, 1};

  ::GradientFill(dc, tri_vertex, 2 , &gradient_rect, 1, GRADIENT_FILL_RECT_V);
}

LRESULT CustomProgressBarCtrl::OnPaint(UINT,
                                       WPARAM,
                                       LPARAM,
                                       BOOL& handled) {  // NOLINT
  handled = TRUE;

  CRect client_rect;
  VERIFY1(GetClientRect(&client_rect));

  CRect progress_bar_rect(client_rect);

  const int kBarWidth(kMaxPosition - kMinPosition);
  LONG bar_rect_right(client_rect.left +
      client_rect.Width() * (current_position_ - kMinPosition) / kBarWidth);
  progress_bar_rect.right = std::min(bar_rect_right, client_rect.right);

  if (GetStyle() & PBS_MARQUEE) {
    LONG bar_rect_left(
        bar_rect_right - client_rect.Width() * kMarqueeWidth / kBarWidth);
    progress_bar_rect.left = std::max(bar_rect_left, client_rect.left);
    ASSERT1(progress_bar_rect.left <= progress_bar_rect.right);
  }

  CRgn rgn(::CreateRectRgnIndirect(&client_rect));
  CRgn rgn_temp(::CreateRectRgnIndirect(&progress_bar_rect));
  VERIFY1(rgn.CombineRgn(rgn_temp, RGN_DIFF) != RGN_ERROR);

  // Eliminating flicker by using a memory device context.
  CPaintDC dcPaint(m_hWnd);
  CMemoryDC dc(dcPaint, client_rect);

  // FillRgn appears to have a bug with RTL/mirroring where it does not paint
  // the first pixel of the rightmost rectangle with the 'rgn' created above.
  // Since the region is rectangles, instead of using FillRgn, this code gets
  // all the rectangles in the 'rgn' and fills them by hand.
  int rgndata_size(rgn.GetRegionData(NULL, 0));
  ASSERT1(rgndata_size);
  std::vector<uint8> rgndata_buff(rgndata_size);
  RGNDATA& rgndata(*reinterpret_cast<RGNDATA*>(&rgndata_buff[0]));

  if (rgn.GetRegionData(&rgndata, rgndata_size)) {
    for (DWORD count = 0; count < rgndata.rdh.nCount; count++) {
      CRect r(reinterpret_cast<RECT*>(rgndata.Buffer + count * sizeof(RECT)));
      CRect bottom_edge_rect(r);

      // Have a 2-pixel bottom edge.
      r.DeflateRect(0, 0, 0, 2);
      bottom_edge_rect.top = r.bottom;

      CBrushHandle bottom_edge_brush(
        reinterpret_cast<HBRUSH>(GetParent().SendMessage(
          WM_CTLCOLORSTATIC,
          reinterpret_cast<WPARAM>(dc.m_hDC),
          reinterpret_cast<LPARAM>(m_hWnd))));
      dc.FillRect(&bottom_edge_rect, bottom_edge_brush);

      dc.FrameRect(r, empty_frame_brush_);
      r.DeflateRect(1, 1);
      dc.FillSolidRect(r, GetColor(empty_fill_color_, COLOR_WINDOWTEXT));
    }
  }

  if (progress_bar_rect.IsRectEmpty()) {
    return 0;
  }

  // Have a 2-pixel bottom shadow with a gradient fill.
  CRect shadow_rect(progress_bar_rect);
  shadow_rect.top = shadow_rect.bottom - 2;
  GradientFill(dc,
               shadow_rect,
               kProgressShadowDarkColor,
               kProgressShadowLightColor);

  // Have a 1-pixel left highlight.
  CRect left_highlight_rect(progress_bar_rect);
  left_highlight_rect.right = left_highlight_rect.left + 1;
  dc.FillSolidRect(left_highlight_rect, kProgressLeftHighlightColor);

  // Adjust progress bar rectangle to accommodate the highlight and shadow.
  // Then draw the outer and inner frames. Then fill in the bar.
  progress_bar_rect.DeflateRect(1, 0, 0, 2);
  GradientFill(dc,
               progress_bar_rect,
               kProgressOuterFrameLight,
               kProgressOuterFrameDark);

  progress_bar_rect.DeflateRect(1, 1);
  GradientFill(dc,
               progress_bar_rect,
               kProgressInnerFrameLight,
               kProgressInnerFrameDark);

  progress_bar_rect.DeflateRect(1, 1);
  GradientFill(dc,
               progress_bar_rect,
               GetColor(bar_color_light_, COLOR_WINDOW),
               GetColor(bar_color_dark_, COLOR_WINDOW));

  return 0;
}

LRESULT CustomProgressBarCtrl::OnTimer(UINT,
                                       WPARAM event_id,
                                       LPARAM,
                                       BOOL& handled) {  // NOLINT
  handled = TRUE;

  // We only handle the timer with ID kMarqueeTimerId.
  if (event_id != kMarqueeTimerId) {
    handled = FALSE;
    return 0;
  }

  ::SendMessage(m_hWnd, PBM_SETPOS, 0, 0L);
  return 0;
}

LRESULT CustomProgressBarCtrl::OnSetPos(UINT,
                                        WPARAM new_position,
                                        LPARAM,
                                        BOOL& handled) {  // NOLINT
  // To allow accessibility to show the correct progress values, we pass
  // PBM_SETPOS to the underlying Win32 control.
  handled = FALSE;

  int old_position = current_position_;

  if (GetStyle() & PBS_MARQUEE) {
    current_position_++;
    if (current_position_ >= (kMaxPosition + kMarqueeWidth)) {
      current_position_ = kMinPosition;
    }
  } else {
    current_position_ = std::min(static_cast<int>(new_position), kMaxPosition);
  }

  if (current_position_ < kMinPosition) {
    current_position_ = kMinPosition;
  }

  VERIFY1(RedrawWindow());

  return old_position;
}

// Calling WM_SETBARCOLOR will convert the progress bar into a solid colored
// bar. i.e., no gradient.
LRESULT CustomProgressBarCtrl::OnSetBarColor(UINT,
                                             WPARAM,
                                             LPARAM bar_color,
                                             BOOL& handled) {  // NOLINT
  handled = TRUE;

  COLORREF old_bar_color = bar_color_light_;
  bar_color_light_ = bar_color_dark_ = static_cast<COLORREF>(bar_color);

  VERIFY1(RedrawWindow());

  return old_bar_color;
}

LRESULT CustomProgressBarCtrl::OnSetBkColor(UINT,
                                            WPARAM,
                                            LPARAM empty_fill_color,
                                            BOOL& handled) {  // NOLINT
  handled = TRUE;

  COLORREF old_empty_fill_color = kProgressEmptyFillColor;
  empty_fill_color_ = static_cast<COLORREF>(empty_fill_color);

  VERIFY1(RedrawWindow());

  return old_empty_fill_color;
}

LRESULT CustomProgressBarCtrl::OnSetMarquee(UINT,
                                            WPARAM is_set_marquee,
                                            LPARAM update_msec,
                                            BOOL& handled) {  // NOLINT
  // To allow accessibility to show the correct progress values, we pass
  // PBM_SETMARQUEE to the underlying Win32 control.
  handled = FALSE;

  if (is_set_marquee && !is_marquee_mode_) {
    current_position_ = kMinPosition;
    VERIFY1(SetTimer(kMarqueeTimerId, static_cast<UINT>(update_msec)));
    is_marquee_mode_ = true;
  } else if (!is_set_marquee && is_marquee_mode_) {
    VERIFY1(KillTimer(kMarqueeTimerId));
    is_marquee_mode_ = false;
  }

  VERIFY1(RedrawWindow());

  return is_set_marquee;
}

}  // namespace omaha
