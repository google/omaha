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

#ifndef OMAHA_WORKER_UILIB_NODE_STATE_H_
#define OMAHA_WORKER_UILIB_NODE_STATE_H_

#include <atlstr.h>

class NodeState {
 public:
  explicit NodeState(HWND window);
  virtual ~NodeState();

  void SetStdFont(HFONT font);
  HFONT GetFont() const;
  COLORREF text_color() const { return text_color_; }
  bool IsURL() const { return !url_.IsEmpty(); }
  CString url() const { return url_; }

  int ConsumeTag(const TCHAR* string);

 private:
  enum Actions {
    UNKNOWN,
    BOLD_ON,
    BOLD_OFF,
    ITALIC_ON,
    ITALIC_OFF,
    UNDERLINE_ON,
    UNDERLINE_OFF,
    TEXTCOLOR_ON,
    TEXTCOLOR_OFF,
    TEXTSIZE_ON,
    TEXTSIZE_OFF,
    URL_ON,
    URL_OFF,
  };

  struct Tags {
    const TCHAR*  name_to_match;
    int           length_name_to_match;
    Actions       action;
    bool          no_parameters;
  };

  int ApplyAction(Actions action, const TCHAR* string);
  int ReadNumParameter(const TCHAR* string, int* param);
  int ReadHexParameter(const TCHAR* szString, int* param);
  int ReadColorRef(const TCHAR* string, int* param);
  int ReadString(const TCHAR* string, CString* string_out);
  bool IsDefaultFont() const;

  // Data
  static Tags    tags_[];

  HWND           owner_window_;
  HFONT          default_font_;
  mutable HFONT  font_;

  bool           bold_;
  bool           italic_;
  bool           underline_;
  COLORREF       text_color_;
  int            text_size_;
  CString        url_;
};

#endif  // OMAHA_WORKER_UILIB_NODE_STATE_H_
