// Copyright 2004-2009 Google Inc.
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

#include "omaha/common/window_utils.h"

#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"

namespace omaha {

namespace {

struct FindProcessWindowsRecord {
  uint32 process_id;
  uint32 window_flags;
  CSimpleArray<HWND>* windows;
};

BOOL CALLBACK FindProcessWindowsEnumProc(HWND hwnd, LPARAM lparam) {
  FindProcessWindowsRecord* enum_record =
      reinterpret_cast<FindProcessWindowsRecord*>(lparam);
  ASSERT1(enum_record);

  DWORD process_id = 0;
  ::GetWindowThreadProcessId(hwnd, &process_id);

  // Only count this window if it is in the right process
  // and it satisfies all specified window requirements.
  if (enum_record->process_id != process_id) {
    return true;
  }
  if ((enum_record->window_flags & kWindowMustBeTopLevel) &&
      ::GetParent(hwnd)) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustHaveSysMenu) &&
      !(GetWindowLong(hwnd, GWL_STYLE) & WS_SYSMENU)) {
    return true;
  }

  if ((enum_record->window_flags & kWindowMustBeVisible) &&
      !::IsWindowVisible(hwnd)) {
    return true;
  }

  enum_record->windows->Add(hwnd);
  return true;
}

}  // namespace

bool WindowUtils::FindProcessWindows(uint32 process_id,
                                     uint32 window_flags,
                                     CSimpleArray<HWND>* windows) {
  ASSERT1(windows);
  windows->RemoveAll();
  FindProcessWindowsRecord enum_record = {0};
  enum_record.process_id = process_id;
  enum_record.window_flags = window_flags;
  enum_record.windows = windows;
  ::EnumWindows(FindProcessWindowsEnumProc,
                reinterpret_cast<LPARAM>(&enum_record));
  int num_windows = enum_record.windows->GetSize();
  return num_windows > 0;
}

void WindowUtils::MakeWindowForeground(HWND wnd) {
  if (!IsWindowVisible(wnd)) {
    // If the window is hidden and we call SetWindowPos with SWP_SHOWWINDOW
    // then the window will be visible.
    // If the caller wants it visible they should do it themselves first.
    return;
  }
  if (!SetWindowPos(wnd,
                    HWND_TOP,
                    0,
                    0,
                    0,
                    0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)) {
    UTIL_LOG(LE, (_T("[WindowUtils::MakeWindowForeground]")
                  _T("[SetWindowPos failed][0x%08x]"), HRESULTFromLastError()));
  }
}

bool WindowUtils::IsMainWindow(HWND wnd) {
  return NULL == ::GetParent(wnd) && IsWindowVisible(wnd);
}

bool WindowUtils::HasSystemMenu(HWND wnd) {
  return (GetWindowLong(wnd, GWL_STYLE) & WS_SYSMENU) != 0;
}

}  // namespace omaha

