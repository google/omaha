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
// node.h : Declaration of the Node

#ifndef OMAHA_WORKER_UILIB_NODE_H_
#define OMAHA_WORKER_UILIB_NODE_H_

#include "omaha/worker/uilib/node_state.h"

class Node {
 public:

  explicit Node(HWND window) : node_state_(window) {}
  virtual ~Node() {}

  CString node_text() const { return node_text_; }
  void AddText(const TCHAR* text) { node_text_ += text; }

  void set_node_state(const NodeState& node_state) { node_state_ = node_state; }
  const NodeState& node_state() const { return node_state_; }

 private:
  CString       node_text_;
  NodeState     node_state_;
};

#endif  // OMAHA_WORKER_UILIB_NODE_H_
