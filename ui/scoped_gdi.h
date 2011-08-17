// Copyright 2010 Google Inc.
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

// The scoped_* classes in this file help to make GDI settings easier. The
// constructors of these classes sets/selects provided new value into the
// context (HDC) and stores the original value in a class member. The
// destructors restore the context to its original state by setting the stored
// value back.
//
// Example usage:
// {
//   // Selects the font into hdc.
//   scoped_select_object select_font(hdc, ::GetStockObject(DEFAULT_GUI_FONT));
//
//   // Changes the bk mode in hdc.
//   scoped_set_bk_mode set_bk_mode(hdc, TRANSPARENT);
//
//   // Changes the bk color in hdc.
//   scoped_set_bk_color set_bk_color(hdc, kBackgroundColor);
//
//   /* Do painting operations with hdc here */
//   ...
//
// }  // scoped_* goes out of scope, restoring hdc back to its original state.
//

#ifndef OMAHA_UI_SCOPED_GDI_H_
#define OMAHA_UI_SCOPED_GDI_H_

#include <wingdi.h>
#include "omaha/base/scoped_any.h"

namespace omaha {

template<typename C, typename T, class set_policy, class invalid_type>
class scoped_context_set {
 public:
  scoped_context_set(C context, T new_value) : context_(context) {
    original_value_ = set_policy::set(context, new_value);
  }

  ~scoped_context_set() {
    invalid_type invalid_value;
    if (original_value_ != invalid_value) {
      set_policy::set(context_, original_value_);
    }
  }

 private:
  T original_value_;
  C context_;
};

template<typename Fn, Fn Pfn>
class set_function {
 public:
  template<typename C, typename T>
  static T set(C context, T value) {
    return Pfn(context, value);
  }
};

typedef set_function<HGDIOBJ (__stdcall *)(HDC hdc, HGDIOBJ gdiobj),  // NOLINT
    ::SelectObject> select_object;
typedef set_function<int (__stdcall *)(HDC hdc, int mode),            // NOLINT
    ::SetBkMode> set_bk_mode;
typedef set_function<COLORREF (__stdcall *)(HDC hdc, COLORREF mode),  // NOLINT
    ::SetBkColor> set_bk_color;

typedef value_const<HGDIOBJ, NULL> invalid_gdi_obj_value;
typedef value_const<int, 0> invalid_bk_mode_value;
typedef value_const<COLORREF, CLR_INVALID> invalid_bk_color_value;

typedef scoped_context_set<HDC,
                           HGDIOBJ,
                           select_object,
                           invalid_gdi_obj_value> ScopedSelectObject;
typedef scoped_context_set<HDC,
                           int,
                           set_bk_mode,
                           invalid_bk_mode_value> ScopedSetBkMode;
typedef scoped_context_set<HDC,
                           COLORREF,
                           set_bk_color,
                           invalid_bk_color_value> ScopedSetBkColor;

}  // namespace omaha

#endif  // OMAHA_UI_SCOPED_GDI_H_
