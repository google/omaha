// Copyright 2006-2009 Google Inc.
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
// static_ex.h : This class extends static control functionality to display
// formatted text and hyper-links
//
// Currently it supports the following formatting options:
//   bold         - <b>bold</b>
//   italic       - <i>italic</i>
//   underscore   - <u>underlined</u>
//   color        - <color=ff0000>red</color>
//   size         - <size=14>14 points text</size>
//   hyperlink    - <a=http://www.google.com>click here</a>
// formatting options could be nested (except hyperlink)
//
// Some fonts (including Tahoma) often overhang one pixel (for example in "W")
// so StaticEx is created with default 1 pixel margin on the left and right,
// use set_margins() to overwrite default values if you need to.


#ifndef OMAHA_WORKER_UILIB_STATIC_EX_H_
#define OMAHA_WORKER_UILIB_STATIC_EX_H_

#include <windows.h>
#include <atlbase.h>
#include <atlwin.h>
#include <vector>
#include "omaha/worker/uilib/node.h"
#include "omaha/worker/uilib/static_line.h"


// extension of NMHDR to provide StaticEx specific info in notification message
struct NM_STATICEX {
  NMHDR header;
  const TCHAR* action;
};

class StaticEx : public CWindowImpl<StaticEx> {
 public:
  DECLARE_WND_SUPERCLASS(NULL, _T("STATIC"))

  StaticEx();
  virtual ~StaticEx();

  void set_margins(const RECT& rect);
  void set_margins(int left, int top, int right, int bottom);
  RECT margins() const { return margins_; }

  void set_background_color(COLORREF back_color);
  COLORREF background_color() const { return background_color_; }
  void ResetBackgroundColor() { use_background_color_ = false; }

  // set ellipsis style (DT_END_ELLIPSIS | DT_WORD_ELLIPSIS |DT_PATH_ELLIPSIS)
  // elipsis are supported only in a single line control, calling this function
  // with not 0 argument will set control style to SS_LEFTNOWORDWRAP
  void set_ellipsis(int ellipsis);
  int ellipsis() const { return ellipsis_; }

  static const int kBorderNone;
  static const int kBorderLeft;
  static const int kBorderTop;
  static const int kBorderRight;
  static const int kBorderBottom;
  static const int kBorderAll;

  // use constants above to set border, you can combine them using "|"
  void set_border(int border);
  int border() const { return border_; }

  void set_border_color(COLORREF border_color);
  COLORREF border_color() const { return border_color_; }

  // this function doesn't change how control is shown
  // it just calculates minimum control height to fit the text given
  // the control width. if width is 0 it will use current control width
  int GetMinimumHeight(int width);

  BEGIN_MSG_MAP(StaticEx)
    MESSAGE_HANDLER(WM_SETTEXT, OnSetText)
    MESSAGE_HANDLER(kGetTextMessage, OnGetText)
    MESSAGE_HANDLER(kGetTextLengthMessage, OnGetTextLength)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
    MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
  END_MSG_MAP()

  LRESULT OnSetText(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);         // NOLINT
  // OnGetText and OnGetTextLength work with full text including formatting tags
  // to get readable text (without formatting info) call GetWindowText or
  // send WM_GETTEXT
  LRESULT OnGetText(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);         // NOLINT
  LRESULT OnGetTextLength(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);   // NOLINT

  LRESULT OnPaint(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);           // NOLINT
  LRESULT OnEraseBkgnd(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);      // NOLINT
  LRESULT OnLButtonDown(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);     // NOLINT
  LRESULT OnSetCursor(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);       // NOLINT

  BOOL SubclassWindow(HWND hWnd);
  HWND UnsubclassWindow(BOOL bForce = FALSE);

 private:
  void Reset();
  void ParseText();
  CString GetReadableText();
  int FindOpenBracket(const TCHAR* string);
  void EraseNodes();
  void EraseLines(std::vector<StaticLine*>* lines);
  HFONT default_font() const { return default_font_; }

  void PrePaint(HDC dc, std::vector<StaticLine*>* lines,
                const std::vector<Node*>& nodes, RECT rect, DWORD style,
                int ellipsis);
  void Paint(HDC hdc, const std::vector<StaticLine*>& lines, RECT rect,
             DWORD style, int ellipsis);
  void DrawBorder(HDC hdc, const CRect& rect);
  HCURSOR GetHandCursor();

  CString               text_;

  CRect                 margins_;
  COLORREF              background_color_;
  bool                  use_background_color_;
  int                   ellipsis_;
  int                   border_;
  COLORREF              border_color_;

  std::vector<Node*>         nodes_;
  std::vector<StaticLine*>   lines_;

  HFONT                 default_font_;

  static HCURSOR hand_cursor_;

  static const UINT kGetTextMessage       = WM_APP + 1;
  static const UINT kGetTextLengthMessage = WM_APP + 2;

  DISALLOW_EVIL_CONSTRUCTORS(StaticEx);
};

#endif  // OMAHA_WORKER_UILIB_STATIC_EX_H_
