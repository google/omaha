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

#include "omaha/worker/uilib/node_state.h"

// Add a list item tag
NodeState::Tags NodeState::tags_[] = {
// name_to_match        length_name_to_match       action      no_parameters;
  { _T("<b>"),       static_cast<int>(_tcslen(_T("<b>"))),      BOLD_ON,        true  },  // NOLINT
  { _T("</b>"),      static_cast<int>(_tcslen(_T("</b>"))),     BOLD_OFF,       true  },  // NOLINT
  { _T("<i>"),       static_cast<int>(_tcslen(_T("<i>"))),      ITALIC_ON,      true  },  // NOLINT
  { _T("</i>"),      static_cast<int>(_tcslen(_T("</i>"))),     ITALIC_OFF,     true  },  // NOLINT
  { _T("<u>"),       static_cast<int>(_tcslen(_T("<u>"))),      UNDERLINE_ON,   true  },  // NOLINT
  { _T("</u>"),      static_cast<int>(_tcslen(_T("</u>"))),     UNDERLINE_OFF,  true  },  // NOLINT
  { _T("<color="),   static_cast<int>(_tcslen(_T("<color="))),  TEXTCOLOR_ON,   false },  // NOLINT
  { _T("</color>"),  static_cast<int>(_tcslen(_T("</color>"))), TEXTCOLOR_OFF,  true  },  // NOLINT
  { _T("<size="),    static_cast<int>(_tcslen(_T("<size="))),   TEXTSIZE_ON,    false },  // NOLINT
  { _T("</size>"),   static_cast<int>(_tcslen(_T("</size>"))),  TEXTSIZE_OFF,   true  },  // NOLINT
  { _T("<a="),       static_cast<int>(_tcslen(_T("<a="))),      URL_ON,         false },  // NOLINT
  { _T("</a>"),      static_cast<int>(_tcslen(_T("</a>"))),     URL_OFF,        true  },  // NOLINT
};

NodeState::NodeState(HWND window)
    : default_font_(NULL),
      font_(NULL),
      bold_(false),
      italic_(false),
      underline_(false),
      text_color_(0),
      text_size_(8),
      owner_window_(window) {
}

NodeState::~NodeState() {
}


void NodeState::SetStdFont(HFONT font) {
  default_font_ = font;
}


HFONT NodeState::GetFont() const {
  if (IsDefaultFont())
    return default_font_;

  if (font_)
    return font_;

  if (default_font_) {
    HDC dc = GetDC(owner_window_);

    LOGFONT log_font;
    GetObject(default_font_, sizeof(LOGFONT), &log_font);
    log_font.lfWeight    = bold_ ? FW_BOLD : FW_NORMAL;
    log_font.lfItalic    = italic_;
    log_font.lfUnderline = underline_;
    log_font.lfHeight    =
        -MulDiv(text_size_, GetDeviceCaps(dc, LOGPIXELSY), 72);
    font_ = CreateFontIndirect(&log_font);
  }

  return font_;
}


bool NodeState::IsDefaultFont() const {
    if (bold_ || italic_ || underline_)
        return false;

    if (text_size_ != 8)
        return false;

    return true;
}


int NodeState::ConsumeTag(const TCHAR* string) {
  int size = sizeof(tags_) / sizeof(tags_[0]);
  for (int i = 0; i < size; i++) {
    if (_tcsnicmp(string, tags_[i].name_to_match,
                  tags_[i].length_name_to_match) == 0) {
      if (tags_[i].no_parameters) {
        ApplyAction(tags_[i].action, NULL);
        return tags_[i].length_name_to_match;
      } else {
        return tags_[i].length_name_to_match +
               ApplyAction(tags_[i].action, string +
                           tags_[i].length_name_to_match) + 1;
      }
    }
  }

  return 0;
}


int NodeState::ApplyAction(Actions action, const TCHAR* string/*=NULL*/) {
  int read = 0;
  switch (action) {
  case BOLD_ON :
    bold_ = true;
    break;

  case BOLD_OFF :
    bold_ = false;
    break;

  case ITALIC_ON :
    italic_ = true;
    break;

  case ITALIC_OFF :
    italic_ = false;
    break;

  case UNDERLINE_ON :
    underline_ = true;
    break;

  case UNDERLINE_OFF :
    underline_ = false;
    break;

  case TEXTCOLOR_ON :
    ATLASSERT(string);
    if (string) {
      int nParam = 0;
      read = ReadColorRef(string, &nParam);
      text_color_ = nParam;
    }
    break;

  case TEXTCOLOR_OFF :
    text_color_ = 0;
    break;

  case TEXTSIZE_ON :
    ATLASSERT(string);
    if (string)
      read = ReadNumParameter(string, &text_size_);
    break;

  case TEXTSIZE_OFF :
    text_size_ = 8;
    break;

  case URL_ON :
    underline_ = true;
    text_color_ = RGB(0, 0, 0xff);
    ATLASSERT(string);
    if (string)
      read = ReadString(string, &url_);
    break;

  case URL_OFF :
    underline_ = false;
    text_color_ = 0;
    url_ = _T("");
    break;

  case UNKNOWN:
    // fall thru

  default:
    ATLASSERT(false);
  }
  return read;
}

int NodeState::ReadNumParameter(const TCHAR* string, int* param) {
  if (!param)
    return 0;

  *param = 0;
  const TCHAR* current_pos = string;
  while (current_pos && _istdigit(*current_pos)) {
    *param *= 10;
    *param += *current_pos - _T('0');
    current_pos++;
  }
  return static_cast<int>(current_pos - string);
}

int NodeState::ReadHexParameter(const TCHAR* string, int* param) {
  if (!param)
    return 0;

  *param = 0;
  const TCHAR* current_pos = string;
  while (current_pos && _istxdigit(*current_pos)) {
    *param = ((*param) << 4);
    if (_istdigit(*current_pos))
      *param += *current_pos - _T('0');
    else
      *param += (*current_pos | 0x20) - _T('a') + 10;
    current_pos++;
  }
  return static_cast<int>(current_pos - string);
}

int NodeState::ReadColorRef(const TCHAR* string, int* param) {
  if (!param)
    return 0;

  int read = ReadHexParameter(string, param);
  *param = RGB((*param) >> 16, ((*param) >> 8) & 0xff, (*param) & 0xff);
  return read;
}

int NodeState::ReadString(const TCHAR* string, CString* string_out) {
  if (!string_out)
    return 0;

  int length = 0;
  int position = _tcscspn(string, _T(" >"));
  if (position >= 0) {
    *string_out = CString(string, position);
    length = position;
  }

  return length;
}
