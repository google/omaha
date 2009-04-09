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

#include "omaha/worker/uilib/static_line.h"
#include "omaha/worker/uilib/node.h"
#include "omaha/worker/uilib/static_ex.h"


StaticLine::StaticLine()
    : base_line_(0),
      height_(0),
      elipses_(false) {
}


StaticLine::~StaticLine() {
}


int StaticLine::AdjustHeight(int height) {
  height_ = std::max(height_, height);
  return height_;
}


int StaticLine::AdjustBaseLine(int base_line) {
  base_line_ = std::max(base_line_, base_line);
  return base_line_;
}


void StaticLine::AddNode(Node* node, int start, int length, int height,
                         int base_line, int width) {
  nodes_.push_back(Nodes(node, start, length, height, base_line, width));
  AdjustHeight(height);
  AdjustBaseLine(base_line);
}


int StaticLine::Paint(HDC hdc, int left, int right, int y, DWORD window_style,
                      int ellipsis) {
  int old_bk_mode = SetBkMode(hdc, TRANSPARENT);
  bool single_line = (window_style & SS_LEFTNOWORDWRAP) != 0;

  size_t size = nodes_.size();
  for (size_t i = 0; i < size; i++) {
    Node* node    = nodes_[i].node;
    int start     = nodes_[i].start;
    int length    = nodes_[i].length;
    int base_line = nodes_[i].base_line;
    int width     = nodes_[i].width;

    CString text(static_cast<LPCTSTR>(node->node_text()) + start, length);
    if (elipses_ && (i == (size - 1)))
      text += "...";

    CRect rect(left, y + base_line_ - base_line, left + width, y + height_);
    if (single_line)
      rect.right = right;

    nodes_[i].rect = rect;

    const NodeState& nodeState = node->node_state();
    HFONT font = nodeState.GetFont();
    if (!font)
      return height_;

    HFONT old_font = static_cast<HFONT>(SelectObject(hdc, font));
    COLORREF old_text_color = SetTextColor(hdc, nodeState.text_color());

    DWORD draw_style = 0;
    draw_style = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    if (ellipsis && (i == (size - 1)))
      draw_style = draw_style | ellipsis;

    DrawText(hdc, text, text.GetLength(), rect, draw_style);
    left += width;

    SetTextColor(hdc, old_text_color);
    if (old_font)
      SelectObject(hdc, old_font);
  }

  SetBkMode(hdc, old_bk_mode);
  return height_;
}


int StaticLine::HitTest(CPoint point) {
  size_t size = nodes_.size();
  for (size_t i = 0; i < size; i++) {
    if (nodes_[i].node->node_state().IsURL()) {
      if (nodes_[i].rect.PtInRect(point)) {
        return static_cast<int>(i);
      }
    }
  }

  return -1;
}


bool StaticLine::IsUrlUnderMouse(CPoint point, CString* action) {
  int index = HitTest(point);
  if (index >= 0 && action) {
    *action = nodes_[index].node->node_state().url();
  }
  return (index >= 0);
}
