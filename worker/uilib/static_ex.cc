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

// TODO(omaha): need to handle WM_GETTEXT
// TODO(omaha): need to handle WM_SIZE
// TODO(omaha): nice to have transparent mode

#include "omaha/worker/uilib/static_ex.h"

#include <shellapi.h>
#include <strsafe.h>
#include "omaha/worker/uilib/node_state.h"

const int StaticEx::kBorderNone    = 0;
const int StaticEx::kBorderLeft    = 1;
const int StaticEx::kBorderTop     = 2;
const int StaticEx::kBorderRight   = 4;
const int StaticEx::kBorderBottom  = 8;
const int StaticEx::kBorderAll     = kBorderLeft | kBorderTop | kBorderRight |
                                     kBorderBottom;

HCURSOR StaticEx::hand_cursor_ = NULL;

StaticEx::StaticEx()
    : margins_(1, 0, 1, 0),  // see comment in h file
      background_color_(0xffffff),
      use_background_color_(false),
      ellipsis_(0),
      border_(kBorderNone),
      border_color_(0),
      default_font_(NULL) {
}

StaticEx::~StaticEx() {
  EraseNodes();
  EraseLines(&lines_);
}

void StaticEx::Reset() {
  text_.Empty();
  EraseNodes();
  EraseLines(&lines_);
  default_font_ = NULL;
}

BOOL StaticEx::SubclassWindow(HWND window) {
  Reset();
  // first get text from exising control
  unsigned length = ::SendMessage(window, WM_GETTEXTLENGTH, 0, 0);
  CString text;
  if (length > 0) {
    TCHAR* buffer = text.GetBufferSetLength(length);
    ::SendMessage(window, WM_GETTEXT, length + 1, reinterpret_cast<LPARAM>(buffer));
    text.ReleaseBuffer(-1);
  }

  // then subclass
  BOOL result = CWindowImpl<StaticEx>::SubclassWindow(window);

  // set text back (it will parse it and replace text in subclassed control
  // with readble text)
  if (result && length > 0) {
    SetWindowText(text);
  }

  return result;
}

HWND StaticEx::UnsubclassWindow(BOOL force /*= FALSE*/) {
  Reset();  // clean up an old state
  return CWindowImpl<StaticEx>::UnsubclassWindow(force);
}

LRESULT StaticEx::OnSetText(UINT msg, WPARAM wparam, LPARAM lparam,
                            BOOL& handled) {
  // parse text first, because we will need to get "readable" text
  text_ = reinterpret_cast<const TCHAR*>(lparam);
  ParseText();

  // set readable text to subclassed control, this text will be return by
  // GetWindowText() or WM_GETTEXT. (when GetWindowText is called from another
  // process it doesn't send WM_GETTEXT but reads text directly from contol)
  // so we need to set text to it.
  // Disable redraw, without it calling DefWindowProc would redraw control
  // immediately without sending WM_PAINT message
  SetRedraw(FALSE);
  DefWindowProc(msg, wparam,
      reinterpret_cast<LPARAM>(static_cast<const TCHAR*>(GetReadableText())));
  SetRedraw(TRUE);

  // now invalidate to display new text
  Invalidate();

  handled = TRUE;
  return 1;
}

LRESULT StaticEx::OnGetText(UINT, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  if (!lparam) return 0;
  unsigned size = static_cast<unsigned>(wparam);
  TCHAR* buffer = reinterpret_cast<TCHAR*>(lparam);

  handled = TRUE;
  unsigned my_size = text_.GetLength();
  if (my_size < size) {
    StringCchCopy(buffer, size, text_);
    return my_size;
  }

  StringCchCopyN(buffer, size, text_, size - 1);
  buffer[size - 1] = 0;

  return size - 1;
}

LRESULT StaticEx::OnGetTextLength(UINT, WPARAM, LPARAM, BOOL& handled) {
  handled = TRUE;
  return text_.GetLength();
}

void StaticEx::set_background_color(COLORREF back_color) {
  background_color_ = back_color;
  use_background_color_ = true;
  Invalidate();
}

void StaticEx::set_margins(const RECT& rect) {
  margins_ = rect;
  Invalidate();
}

void StaticEx::set_margins(int left, int top, int right, int bottom) {
  margins_.SetRect(left, top, right, bottom);
  Invalidate();
}


LRESULT StaticEx::OnLButtonDown(UINT, WPARAM wparam, LPARAM lparam, BOOL&) {
  if (wparam != MK_LBUTTON)
    return 0;

  CPoint point(LOWORD(lparam), HIWORD(lparam));

  int height = margins_.top + (border_ & kBorderTop) ? 1 : 0;
  size_t size = lines_.size();
  for (size_t i = 0; i < size; i++) {
    height += lines_[i]->height();
    if (point.y < height) {
      CString action;
      if (lines_[i]->IsUrlUnderMouse(point, &action) && !action.IsEmpty()) {
        // First let the parent window handle the click.
        LRESULT handled = 0;
        NM_STATICEX notification = {0};
        notification.header.hwndFrom = m_hWnd;
        notification.header.idFrom = GetDlgCtrlID();
        notification.header.code = wparam;
        notification.action = action;

        HWND parent = GetParent();
        if (parent) {
          handled = ::SendMessage(parent, WM_NOTIFY, notification.header.idFrom,
                                  reinterpret_cast<LPARAM>(&notification));
        }

        // If the parent window did not handle the click, then we should try and
        // handle the click ourself.
        if (handled != 1 && _tcsnicmp(action, _T("http"), 4) == 0) {
          // open URL in a browser
          ShellExecute(m_hWnd, _T("open"), action, NULL, NULL, SW_SHOWNORMAL);
        }
      }
      break;
    }
  }
  return 0;
}


LRESULT StaticEx::OnSetCursor(UINT, WPARAM, LPARAM lparam, BOOL& handled) {
  int hit_test = LOWORD(lparam);
  handled = FALSE;
  if (hit_test != HTCLIENT) {
    return 0;
  }

  POINT position;
  if (!GetCursorPos(&position))
    return 0;

  ScreenToClient(&position);

  int offset = margins_.top + (border_ & kBorderTop) ? 1 : 0;
  size_t size = lines_.size();
  for (size_t i = 0; i < size; i++) {
    offset += lines_[i]->height();
    if (position.y < offset) {
      if (lines_[i]->IsUrlUnderMouse(position, NULL)) {
        ::SetCursor(GetHandCursor());
        handled = TRUE;
      }
      break;
    }
  }

  return 0;
}

void StaticEx::set_ellipsis(int ellipsis) {
  if (ellipsis == DT_END_ELLIPSIS  ||
      ellipsis == DT_WORD_ELLIPSIS ||
      ellipsis == DT_PATH_ELLIPSIS ||
      ellipsis == 0) {
    ellipsis_ = ellipsis;
    if (ellipsis != 0)
      ModifyStyle(0, SS_LEFTNOWORDWRAP);
    Invalidate();
  }
}

void StaticEx::set_border(int border) {
  border_ = border;
  Invalidate();
}

void StaticEx::set_border_color(COLORREF border_color) {
  border_color_ = border_color;
  Invalidate();
}

void StaticEx::ParseText() {
  EraseNodes();
  EraseLines(&lines_);

  if (text_.IsEmpty())
    return;

  if (!default_font_)
    default_font_ = GetFont();

  NodeState node_state(m_hWnd);
  node_state.SetStdFont(default_font_);

  const TCHAR* current_string = text_;
  int current_offset = 0;
  bool had_good_tag = true;
  while (*current_string) {
    current_offset = 0;

    if (had_good_tag) {
      // if it was a good tag we consumed it and need to start with a new node
      Node* node = new Node(m_hWnd);
      nodes_.push_back(node);

      // -1 if there is no Open Bracket "<"
      current_offset = FindOpenBracket(current_string);

      if (current_offset < 0) {
        // no tags left, just plain text
        node->AddText(current_string);
        node->set_node_state(node_state);
        break;
      }

      if (*current_string != _T('<')) {
        // has some text before the tag
        node->AddText(CString(current_string, current_offset));
        node->set_node_state(node_state);
        current_string += current_offset;
        continue;
      }

      int next_offset = node_state.ConsumeTag(current_string + current_offset);

      if (next_offset > 0) {
        // it was a known tag
        had_good_tag = true;
        current_string += current_offset + next_offset;
      }  else  {
        // unknown tag, will keep looking
        had_good_tag = false;
        node->AddText(CString(current_string, current_offset + 1));
        node->set_node_state(node_state);
        current_string += current_offset + 1;
        continue;
      }
    } else {
      had_good_tag = true;
    }
    delete nodes_.back();
    nodes_.pop_back();
  }
}

CString StaticEx::GetReadableText() {
  CString text;
  for (size_t i = 0; i < nodes_.size(); i++) {
    text += nodes_[i]->node_text();
  }
  return text;
}

void StaticEx::EraseNodes() {
  for (size_t i = 0; i < nodes_.size(); ++i) {
    delete nodes_[i];
  }
  nodes_.clear();
}

void StaticEx::EraseLines(std::vector<StaticLine*>* lines) {
  size_t size = lines->size();
  for (size_t i = 0; i < size; i++) {
    delete (*lines)[i];
  }
  lines->clear();
}

int StaticEx::FindOpenBracket(const TCHAR* string) {
  const TCHAR* left_bracket = _tcschr(string, _T('<'));

  if (left_bracket == NULL)
    return -1;

  return static_cast<int>(left_bracket - string);
}

LRESULT StaticEx::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
  PAINTSTRUCT paint_struct;
  HDC hdc = BeginPaint(&paint_struct);

  CRect client_rect;
  GetClientRect(&client_rect);

  CRect working_rect(client_rect);

  working_rect.DeflateRect(margins_);
  working_rect.DeflateRect((border_ & kBorderLeft)   ? 1 : 0,
                           (border_ & kBorderTop)    ? 1 : 0,
                           (border_ & kBorderRight)  ? 1 : 0,
                           (border_ & kBorderBottom) ? 1 : 0);

  DWORD style = GetStyle();

  EraseLines(&lines_);
  PrePaint(hdc, &lines_, nodes_, working_rect, style, ellipsis_);

  if (use_background_color_) {
    FillRect(hdc, &client_rect, CreateSolidBrush(background_color_));
  } else {
    HBRUSH brush = reinterpret_cast<HBRUSH>(::SendMessage(GetParent(),
        WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(hdc),
        reinterpret_cast<LPARAM>(m_hWnd)));
    if (brush) {
      ::FillRect(hdc, &client_rect, brush);
    }
  }

  if (border_ != kBorderNone)
    DrawBorder(hdc, client_rect);

  Paint(hdc, lines_, working_rect, style, ellipsis_);

  EndPaint(&paint_struct);
  return 0;
}

void StaticEx::PrePaint(HDC hdc, std::vector<StaticLine*>* lines,
                        const std::vector<Node*>& nodes, RECT rect, DWORD style,
                        int ellipsis) {
  if (nodes.empty())
    return;

  int x = 0;
  int width = rect.right - rect.left;
  StaticLine* line = new StaticLine;
  lines->push_back(line);
  bool done = false;
  size_t size = nodes.size();
  for (size_t i = 0; i < size; ++i) {
    Node* node = nodes[i];
    const NodeState& node_state = node->node_state();
    CString text = node->node_text();
    int string_len = text.GetLength();

    HFONT font = node_state.GetFont();
    if (!font)
      return;

    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, font));

    TEXTMETRIC text_metrics;
    GetTextMetrics(hdc, &text_metrics);

    int height    = text_metrics.tmHeight + text_metrics.tmExternalLeading;
    int base_line = text_metrics.tmHeight + text_metrics.tmExternalLeading -
                    text_metrics.tmDescent;
    line->AdjustHeight(height);
    line->AdjustBaseLine(base_line);

    bool single_line = (style & SS_LEFTNOWORDWRAP) != 0;

    int  current_pos = 0;
    bool more_left   = false;
    while (true) {
      int current_length = string_len - current_pos;

      // find LF if any
      int lf_position = text.Find(_T('\n'), current_pos);
      if (lf_position == current_pos) {
        if (single_line) {
          if (ellipsis)
            line->AddEllipses();
          break;
        }
        line = new StaticLine;
        lines->push_back(line);
        line->AdjustHeight(height);
        line->AdjustBaseLine(base_line);
        x = 0;

        current_pos++;

        continue;
      } else if (lf_position > 0) {
        current_length = lf_position - current_pos;
        more_left = true;
      }

      // check if it will fit in one line
      int fit  = 0;
      SIZE string_size;
      GetTextExtentExPoint(hdc, static_cast<const TCHAR*>(text) + current_pos,
                           current_length, width - x, &fit, NULL, &string_size);

      if (fit < current_length) {
        // string doesn't fit, need to move to the next line
        // find last space
        int fit_saved = fit;
        for (; fit > 0; fit--) {
          if (text.GetAt(current_pos + fit) == _T(' ')) {
            break;
          }
        }

        // if a first word of a node doesn't fit and it starts in a first half
        // of control then wrap the word on a last char that fits
        // otherwise move whole node to the next line
        if ((fit <= 0) && (x < width / 2))
          fit = fit_saved;

        if (fit > 0) {
          line->AddNode(node, current_pos, fit, height, base_line, width - x);
        }

        if (single_line) {
          if (ellipsis)
            line->AddEllipses();
          done = true;
          break;
        }
        line = new StaticLine;
        lines->push_back(line);
        line->AdjustHeight(height);
        line->AdjustBaseLine(base_line);
        x = 0;

        current_pos += fit;
        // skip spaces
        while (text.GetAt(current_pos) == _T(' '))
          current_pos++;
        continue;
      } else {
        line->AddNode(node, current_pos, fit, height, base_line,
                      string_size.cx);
      }

      // done, it fits
      x += string_size.cx;
      if (!more_left)
        break;

      current_pos += current_length;
      more_left = false;
    }

    if (old_font)
      SelectObject(hdc, old_font);

    if (done)
      break;
  }
}


void StaticEx::Paint(HDC hdc, const std::vector<StaticLine*>& lines, RECT rect,
                     DWORD style, int ellipsis) {
  if ((style & SS_LEFTNOWORDWRAP) == 0) {
    ellipsis = 0;
  }

  size_t size = lines.size();
  int y = rect.top;
  for (size_t i = 0; i < size; i++) {
    int height = lines[i]->Paint(hdc, rect.left, rect.right, y, style,
                                 ellipsis);
    y += height;
  }
}

LRESULT StaticEx::OnEraseBkgnd(UINT /*msg*/, WPARAM /*wparam*/,
                               LPARAM /*lparam*/, BOOL& handled) {
  handled = TRUE;
  return 0;
}

void StaticEx::DrawBorder(HDC hdc, const CRect& rect) {
  HGDIOBJ old_object = SelectObject(hdc, GetStockObject(DC_PEN));
  SetDCPenColor(hdc, border_color_);
  if (border_ & kBorderLeft) {
    MoveToEx(hdc, rect.left, rect.top, NULL);
    LineTo(hdc, rect.left, rect.bottom);
  }
  if (border_ & kBorderTop) {
    MoveToEx(hdc, rect.left, rect.top, NULL);
    LineTo(hdc, rect.right, rect.top);
  }
  if (border_ & kBorderRight) {
    MoveToEx(hdc, rect.right - 1, rect.top, NULL);
    LineTo(hdc, rect.right - 1, rect.bottom);
  }
  if (border_ & kBorderBottom) {
    MoveToEx(hdc, rect.left, rect.bottom - 1, NULL);
    LineTo(hdc, rect.right, rect.bottom - 1);
  }
  SelectObject(hdc, old_object);
}

int StaticEx::GetMinimumHeight(int width) {
  HDC device_context = CreateCompatibleDC(NULL);

  CRect client_rect;
  if (width <= 0)
    GetClientRect(&client_rect);
  else
    client_rect.SetRect(0, 0, width, 100);  // last value is not used

  CRect working_rect(client_rect);

  working_rect.DeflateRect(margins_);
  working_rect.DeflateRect((border_ & kBorderLeft)   ? 1 : 0,
                           (border_ & kBorderTop)    ? 1 : 0,
                           (border_ & kBorderRight)  ? 1 : 0,
                           (border_ & kBorderBottom) ? 1 : 0);

  DWORD style = GetStyle();

  std::vector<StaticLine*> lines;
  PrePaint(device_context, &lines, nodes_, working_rect, style, ellipsis_);

  DeleteDC(device_context);

  int height = 0;
  for (unsigned i = 0; i < lines.size(); i++) {
    height += lines[i]->height();
  }
  height += margins_.top + margins_.bottom;
  height += (border_ & kBorderTop) ? 1 : 0;
  height += (border_ & kBorderBottom) ? 1 : 0;

  return height;
}


HCURSOR StaticEx::GetHandCursor() {
  if (hand_cursor_ == NULL) {
    // Load cursor resource
    hand_cursor_ = (HCURSOR)LoadCursor(NULL, IDC_HAND);  // doesn't work on NT4!
  }
  return hand_cursor_;
}
