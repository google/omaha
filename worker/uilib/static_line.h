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
// static_line.h

#ifndef TACTICAL_PROTECTOR_UILIB_CONTROLS_STATIC_LINE_H__
#define TACTICAL_PROTECTOR_UILIB_CONTROLS_STATIC_LINE_H__

#include <atlstr.h>
#include <atltypes.h>
#include <vector>
#include "base/basictypes.h"

class Node;

class StaticLine {
 public:
  StaticLine();
  virtual ~StaticLine();

  int AdjustBaseLine(int base_line);
  int AdjustHeight(int height);

  int base_line() const { return base_line_; }
  int height() const { return height_; }

  void AddNode(Node* node, int start, int end, int height, int base_line,
               int width);
  void AddEllipses() { elipses_ = true; }

  bool IsUrlUnderMouse(CPoint point, CString* action);

  int  Paint(HDC hdc, int left, int right, int y, DWORD window_style,
             int ellipsis);

 protected:
  int     HitTest(CPoint point);

  int   base_line_;
  int   height_;

  struct Nodes {
    Node*   node;
    int     start;     // first char to output
    int     length;    // number of chars
    int     height;
    int     base_line;
    int     width;
    CRect   rect;

    Nodes(Node* node, int start, int length, int height, int base_line,
          int width)
        : node(node), start(start), length(length), height(height),
          base_line(base_line), width(width), rect(0, 0, 0, 0) {}
  };

  std::vector<Nodes>   nodes_;
  bool            elipses_;

  DISALLOW_EVIL_CONSTRUCTORS(StaticLine);
};

#endif  // TACTICAL_PROTECTOR_UILIB_CONTROLS_STATIC_LINE_H__
