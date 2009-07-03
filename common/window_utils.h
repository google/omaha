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


#ifndef OMAHA_COMMON_WINDOW_UTILS_H__
#define OMAHA_COMMON_WINDOW_UTILS_H__

#include <atlcoll.h>
#include "base/basictypes.h"

namespace omaha {

// Flags for window requirements.
const uint32 kWindowMustBeTopLevel  =  0x00000001;
const uint32 kWindowMustHaveSysMenu =  0x00000002;
const uint32 kWindowMustBeVisible   =  0x00000004;

class WindowUtils {
 public:
  // Finds all the primary windows owned by the given process. For the
  // purposes of this function, primary windows are top-level, have a system
  // menu, and are visible.
  static bool FindProcessWindows(uint32 process_id,
                                 uint32 window_flags,
                                 CSimpleArray<HWND>* windows);

  // Forces the window to the foreground.
  static void MakeWindowForeground(HWND wnd);

  // Returns true if the window is the "main window" of a process:
  // if it's Visible, and Top Level
  static bool IsMainWindow(HWND wnd);

  // Returns true if the window has a System Menu
  static bool HasSystemMenu(HWND wnd);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WindowUtils);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_WINDOW_UTILS_H__
