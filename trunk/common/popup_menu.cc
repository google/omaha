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

#include "omaha/common/popup_menu.h"

#include <windows.h>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

// Hold owner draw data
struct OwnerDrawData {
  HFONT font;
  CString text;
  HICON icon;

  OwnerDrawData(HFONT f, const TCHAR* t, HICON i) {
    font = f;
    text = t;
    icon = i;
  }
};


// Default constructor
PopupMenu::PopupMenu()
    : wnd_(NULL) {
  reset(menu_, ::CreatePopupMenu());
  ASSERT1(menu_);
}

// Constructor
PopupMenu::PopupMenu(HINSTANCE inst, const TCHAR* name)
    : wnd_(NULL) {
  LoadFromResource(inst, name);
  ASSERT1(menu_);
}

// Destructor
PopupMenu::~PopupMenu() {
}

// Load from resource
bool PopupMenu::LoadFromResource(HINSTANCE inst, const TCHAR* name) {
  reset(menu_, GetSubMenu(reinterpret_cast<HMENU>(::LoadMenu(inst, name)), 0));
  return get(menu_) != NULL;
}

// Append menu item
bool PopupMenu::AppendMenuItem(int menu_item_id, const TCHAR* text) {
  return AppendMenuItem(menu_item_id, text, NULL);
}

// Append menu item
bool PopupMenu::AppendMenuItem(int menu_item_id,
                               const TCHAR* text,
                               const MenuItemDrawStyle* style) {
  int count = ::GetMenuItemCount(get(menu_));
  if (count == -1)
    return false;

  return InsertMenuItem(menu_item_id, count, true, text, style);
}

// Append separator
bool PopupMenu::AppendSeparator() {
  return AppendMenuItem(-1, NULL, NULL);
}

// Insert menu item
bool PopupMenu::InsertMenuItem(int menu_item_id,
                               int before_item,
                               bool by_pos,
                               const TCHAR* text) {
  return InsertMenuItem(menu_item_id, before_item, by_pos, text, NULL);
}

// Helper function that populates the MENUITEMINFO structure and sets
// accelerator keys for OWNERDRAW menu items
MENUITEMINFO PopupMenu::PrepareMenuItemInfo(int menu_item_id, const TCHAR* text,
                                            const MenuItemDrawStyle* style) {
  // Fill in the MENUITEMINFO structure
  MENUITEMINFO menuitem_info;
  SetZero(menuitem_info);
  menuitem_info.cbSize = sizeof(MENUITEMINFO);
  menuitem_info.wID = menu_item_id;
  if (text == NULL) {
    menuitem_info.fMask = MIIM_FTYPE | MIIM_ID;
    menuitem_info.fType = MFT_SEPARATOR;
  } else {
    if (!style) {
      menuitem_info.fMask = MIIM_STRING | MIIM_ID;
      menuitem_info.fType = MFT_STRING;
      menuitem_info.dwTypeData = const_cast<TCHAR*>(text);
    } else {
      // Handle bold font style
      HFONT font = NULL;
      if (style->is_bold) {
        font = GetBoldFont();
      }

      // Remove '&' if it is there
      CString text_str(text);
      int pos = String_FindChar(text_str, _T('&'));
      if (pos != -1) {
        if (pos + 1 < text_str.GetLength()) {
          accelerator_keys_.Add(Char_ToLower(text_str[pos + 1]), menu_item_id);
        }
        ReplaceCString(text_str, _T("&"), _T(""));
      }

      // Set owner-draw related properties
      OwnerDrawData* data = new OwnerDrawData(font, text_str, style->icon);
      menuitem_info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_ID;
      menuitem_info.fType = MFT_OWNERDRAW;
      menuitem_info.dwItemData = reinterpret_cast<ULONG_PTR>(data);
    }
  }

  return menuitem_info;
}

// Insert menu item
bool PopupMenu::InsertMenuItem(int menu_item_id,
                               int before_item,
                               bool by_pos,
                               const TCHAR* text,
                               const MenuItemDrawStyle* style) {
  MENUITEMINFO menuitem_info = PrepareMenuItemInfo(menu_item_id, text, style);
  if (!::InsertMenuItem(get(menu_), before_item, by_pos, &menuitem_info))
    return false;

  return Redraw();
}

// Insert separator
bool PopupMenu::InsertSeparator(int before_item, bool by_pos) {
  return InsertMenuItem(-1, before_item, by_pos, NULL, NULL);
}

// Modify a given menu item
bool PopupMenu::ModifyMenuItem(int menu_item, bool by_pos, const TCHAR* text,
                               const MenuItemDrawStyle* style) {
  // Get OWNERDRAW data for later deletion
  MENUITEMINFO menuitem_info;
  SetZero(menuitem_info);
  menuitem_info.cbSize = sizeof(MENUITEMINFO);
  menuitem_info.fMask = MIIM_FTYPE | MIIM_DATA;
  if (!::GetMenuItemInfo(get(menu_), menu_item, by_pos, &menuitem_info)) {
    return false;
  }

  OwnerDrawData* old_owner_data = NULL;
  if ((menuitem_info.fType | MFT_OWNERDRAW) && menuitem_info.dwItemData) {
    old_owner_data =
        reinterpret_cast<OwnerDrawData *>(menuitem_info.dwItemData);
  }

  // Remove old accelerator mapping
  int menu_item_id = by_pos ? ::GetMenuItemID(get(menu_), menu_item) :
                              menu_item;
  int key_pos = accelerator_keys_.FindVal(menu_item_id);
  if (key_pos != -1) {
    accelerator_keys_.RemoveAt(key_pos);
  }

  // Set new menu item info
  menuitem_info = PrepareMenuItemInfo(menu_item_id, text, style);
  if (!::SetMenuItemInfo(get(menu_), menu_item, by_pos, &menuitem_info)) {
    return false;
  }

  // Delete old owner draw data
  if (old_owner_data) {
    delete old_owner_data;
  }

  // Redraw
  return Redraw();
}

// Remove a menu item
bool PopupMenu::RemoveMenuItem(int menu_item, bool by_pos) {
  // Get OWNERDRAW data for later deletion
  MENUITEMINFO menuitem_info;
  SetZero(menuitem_info);
  menuitem_info.cbSize = sizeof(MENUITEMINFO);
  menuitem_info.fMask = MIIM_FTYPE | MIIM_DATA;
  if (!::GetMenuItemInfo(get(menu_), menu_item, by_pos, &menuitem_info)) {
    return false;
  }

  OwnerDrawData* old_owner_data = NULL;
  if ((menuitem_info.fType | MFT_OWNERDRAW) && menuitem_info.dwItemData) {
    old_owner_data =
        reinterpret_cast<OwnerDrawData *>(menuitem_info.dwItemData);
  }

  // Remove the menu item
  if (!::RemoveMenu(get(menu_), menu_item, by_pos ? MF_BYPOSITION :
                                                    MF_BYCOMMAND)) {
    return false;
  }

  // Remove old accelerator mapping
  int menu_item_id = by_pos ? ::GetMenuItemID(get(menu_), menu_item) :
                              menu_item;
  int key_pos = accelerator_keys_.FindVal(menu_item_id);
  if (key_pos != -1) {
    accelerator_keys_.RemoveAt(key_pos);
  }

  // Delete old owner draw data
  if (old_owner_data) {
    delete old_owner_data;
  }

  // Redraw
  return Redraw();
}

// Enable menu item
bool PopupMenu::EnableMenuItem(int menu_item, bool by_pos, bool enabled) {
  if (::EnableMenuItem(get(menu_), menu_item,
                        (by_pos ? MF_BYPOSITION : MF_BYCOMMAND) |
                        (enabled ? MF_ENABLED : MF_GRAYED)) == -1)
    return false;

  return Redraw();
}

// Get menu state
bool PopupMenu::GetMenuState(int menu_item, bool by_pos, int* menu_state) {
  int state = ::GetMenuState(get(menu_),
                             menu_item, by_pos ? MF_BYPOSITION : MF_BYCOMMAND);
  if (menu_state)
    *menu_state = state;
  return state != -1;
}

// Exists a menu item
bool PopupMenu::ExistsMenuItem(int menu_item_id) {
  return GetMenuState(menu_item_id, false, NULL);
}

// Redraw menu
bool PopupMenu::Redraw() {
  if (!wnd_)
    return true;

  return ::DrawMenuBar(wnd_) == TRUE;
}

// Track menu
bool PopupMenu::Track() {
  ASSERT1(wnd_);

  // If we don't set it to be foreground, it will not stop tracking even
  // if we click outside of menu.
  ::SetForegroundWindow(wnd_);

  POINT point = {0, 0};
  VERIFY(::GetCursorPos(&point), (_T("")));

  uint32 kFlags = TPM_LEFTALIGN  |
                  TPM_RETURNCMD  |
                  TPM_NONOTIFY   |
                  TPM_LEFTBUTTON |
                  TPM_VERTICAL;
  int command = ::TrackPopupMenuEx(get(menu_),
                                   kFlags,
                                   point.x, point.y, wnd_, NULL);

  if (command != 0)
    ::SendMessage(wnd_, WM_COMMAND, command, 0);

  return true;
}

// Handle WM_MEASUREITEM message
bool PopupMenu::OnMeasureItem(MEASUREITEMSTRUCT* mi) {
  ASSERT1(wnd_);

  // Get owner draw data
  ASSERT1(mi->itemData);
  OwnerDrawData* data = reinterpret_cast<OwnerDrawData*>(mi->itemData);

  // Get the DC
  scoped_hdc dc;
  reset(dc, ::GetDC(wnd_));

  // Select the font
  HFONT old_font = reinterpret_cast<HFONT>(::SelectObject(get(dc), data->font));
  if (!old_font)
    return false;

  // compute the size of the text
  SIZE size = {0, 0};
  bool success = ::GetTextExtentPoint32(get(dc),
                                        data->text.GetString(),
                                        data->text.GetLength(),
                                        &size) != 0;
  if (success) {
    mi->itemWidth = size.cx;
    mi->itemHeight = size.cy;
  }

  // deselect the title font
  ::SelectObject(get(dc), old_font);

  return success;
}

// Handle WM_MDRAWITEM message
bool PopupMenu::OnDrawItem(DRAWITEMSTRUCT* di) {
  ASSERT1(di);

  // Get owner draw data
  ASSERT1(di->itemData);
  OwnerDrawData* data = reinterpret_cast<OwnerDrawData*>(di->itemData);

  // Select the font
  HFONT prev_font = NULL;
  if (data->font) {
    prev_font = reinterpret_cast<HFONT>(::SelectObject(di->hDC, data->font));
    if (!prev_font) {
      return false;
    }
  }

  // Draw the text per the menuitem state
  int fg_color_idx =
      (di->itemState & ODS_DISABLED) ?
      COLOR_GRAYTEXT :
      ((di->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);

  int bg_color_idx =
      (di->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_MENU;

  bool success = DrawText(data->text, di, fg_color_idx, bg_color_idx);

  // Restore the original font
  if (prev_font) {
    ::SelectObject(di->hDC, prev_font);
  }

  // Compute the width and height
  int height = di->rcItem.bottom - di->rcItem.top + 1;
  int width = static_cast<int>(::GetSystemMetrics(SM_CXMENUCHECK) *
    (static_cast<double>(height) / ::GetSystemMetrics(SM_CYMENUCHECK)));

  // Draw the icon
  // TODO(omaha): Draw a grayed icon when the menuitem is disabled
  if (success && data->icon) {
    success = ::DrawIconEx(di->hDC,
                           di->rcItem.left,
                           di->rcItem.top,
                           data->icon,
                           width,
                           height,
                           0,
                           NULL,
                           DI_NORMAL) != 0;
  }

  return success;
}

// Draw the text
bool PopupMenu::DrawText(const CString& text,
                         DRAWITEMSTRUCT* di,
                         int fg_color_idx,
                         int bg_color_idx) {
  // Set the appropriate foreground and background colors
  COLORREF prev_fg_color = 0, prev_bg_color = 0;
  prev_fg_color = ::SetTextColor(di->hDC, ::GetSysColor(fg_color_idx));
  if (prev_fg_color == CLR_INVALID) {
    return false;
  }
  prev_bg_color = ::SetBkColor(di->hDC, ::GetSysColor(bg_color_idx));
  if (prev_bg_color == CLR_INVALID) {
    return false;
  }

  // Draw the text
  bool success = ::ExtTextOut(
      di->hDC,
      di->rcItem.left + ::GetSystemMetrics(SM_CXMENUCHECK) + 4,
      di->rcItem.top,
      ETO_OPAQUE,
      &di->rcItem,
      text.GetString(),
      text.GetLength(),
      NULL) == TRUE;

  // Restore the original colors
  ::SetTextColor(di->hDC, prev_fg_color);
  ::SetBkColor(di->hDC, prev_bg_color);

  return success;
}

// Handle WM_MENUCHAR message
int PopupMenu::OnMenuChar(TCHAR key) {
  int pos = accelerator_keys_.FindKey(Char_ToLower(key));
  if (pos != -1)
    return GetMenuPosFromID(accelerator_keys_.GetValueAt(pos));
  else
    return -1;
}

HFONT PopupMenu::GetBoldFont() {
  if (!bold_font_) {
    NONCLIENTMETRICS ncm;
    SetZero(ncm);
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0)) {
      ncm.lfMenuFont.lfWeight = FW_BOLD;
      reset(bold_font_, ::CreateFontIndirect(&ncm.lfMenuFont));
    } else {
      UTIL_LOG(LEVEL_ERROR, (_T("[PopupMenu::GetBoldFont]")
                             _T("[failed to get system menu font][0x%x]"),
                             HRESULTFromLastError()));
    }
  }

  ASSERT1(bold_font_);

  return get(bold_font_);
}

// Get menu pos from ID
int PopupMenu::GetMenuPosFromID(int id) {
  ASSERT1(id >= 0);
  int count = ::GetMenuItemCount(get(menu_));
  if (count > 0) {
    for (int pos = 0; pos < count; ++pos) {
      if (::GetMenuItemID(get(menu_), pos) == static_cast<UINT>(id)) {
        return pos;
      }
    }
  }

  return -1;
}

}  // namespace omaha

