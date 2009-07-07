// Copyright 2005-2009 Google Inc.
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

#ifndef  OMAHA_COMMON_POPUP_MENU_H_
#define  OMAHA_COMMON_POPUP_MENU_H_

#include "omaha/common/debug.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

struct MenuItemDrawStyle {
  bool is_bold;
  HICON icon;

  MenuItemDrawStyle() : is_bold(false), icon(NULL) {}
};


class PopupMenu {
 public:
  PopupMenu();
  PopupMenu(HINSTANCE inst, const TCHAR* name);

  ~PopupMenu();

  // Load from resource
  bool LoadFromResource(HINSTANCE inst, const TCHAR* name);

  // Append menu item
  bool AppendMenuItem(int menu_item_id, const TCHAR* text);
  bool AppendMenuItem(int menu_item_id,
                      const TCHAR* text,
                      const MenuItemDrawStyle* style);

  // Append separator
  bool AppendSeparator();

  // Helper function that populates the MENUITEMINFO structure and sets
  // accelerator keys for OWNERDRAW menu items
  MENUITEMINFO PrepareMenuItemInfo(int menu_item_id,
                                   const TCHAR* text,
                                   const MenuItemDrawStyle* style);

  // Insert menu item
  bool InsertMenuItem(int menu_item_id,
                      int before_item,
                      bool by_pos,
                      const TCHAR* text);

  bool InsertMenuItem(int menu_item_id,
                      int before_item,
                      bool by_pos,
                      const TCHAR* text,
                      const MenuItemDrawStyle* style);

  // Insert separator
  bool InsertSeparator(int before_item, bool by_pos);

  // Modify a given menu item
  bool ModifyMenuItem(int menu_item,
                      bool by_pos,
                      const TCHAR* text,
                      const MenuItemDrawStyle* style);

  // Remove menu item
  bool RemoveMenuItem(int menu_item, bool by_pos);

  // Enable menu item
  bool EnableMenuItem(int menu_item, bool by_pos, bool enabled);

  // Get menu state
  bool GetMenuState(int menu_item, bool by_pos, int* menu_state);

  // Exists a menu item
  bool ExistsMenuItem(int menu_item_id);

  // Get menu pos from ID
  int GetMenuPosFromID(int id);

  // Attach to the window
  void AttachToWindow(HWND wnd) {
    ASSERT1(wnd);
    wnd_ = wnd;
  }

  // Redraw menu
  bool Redraw();

  // Track menu
  bool Track();

  // Handle WM_MEASUREITEM message
  bool OnMeasureItem(MEASUREITEMSTRUCT* mi);

  // Handle WM_MDRAWITEM message
  bool OnDrawItem(DRAWITEMSTRUCT* di);

  // Handle WM_MENUCHAR message
  int OnMenuChar(TCHAR key);

 private:
  // Get bold font
  HFONT GetBoldFont();

  // Draw the text
  bool DrawText(const CString& text,
                DRAWITEMSTRUCT* di,
                int fg_color_idx,
                int bg_color_idx);

  HWND wnd_;           // HWND associated with this menu
  scoped_hmenu menu_;  // HMENU associated with this menu

  // Bold font used in owner draw menu-item
  scoped_hfont bold_font_;

  // Accelerator key used in owner draw menu-item
  CSimpleMap<TCHAR, int> accelerator_keys_;

  DISALLOW_EVIL_CONSTRUCTORS(PopupMenu);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_POPUP_MENU_H_

