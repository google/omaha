// Copyright 2003-2009 Google Inc.
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
// Shell functions

#include "omaha/base/shell.h"

#include <shlobj.h>
#include <shellapi.h>
#include "omaha/base/app_util.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

HRESULT Shell::Execute(const TCHAR* file) {
  ASSERT1(file);

  // Prepare everything required for ::ShellExecuteEx().
  SHELLEXECUTEINFO sei;
  SetZero(sei);
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_FLAG_NO_UI     |  // Do not display an error message box.
              SEE_MASK_NOZONECHECKS   |  // Do not perform a zone check.
              SEE_MASK_NOASYNC;          // Wait to complete before returning.
  // Pass NULL for hwnd. This will have ShellExecuteExEnsureParent()
  // create a default parent window for us.
  // sei.hwnd = NULL;
  sei.lpVerb = _T("open");
  sei.lpFile = file;
  // No parameters to pass
  // sei.lpParameters = NULL;
  // Use parent's starting directory
  // sei.lpDirectory = NULL;
  sei.nShow = SW_SHOWNORMAL;

  // Use ShellExecuteExEnsureParent to ensure that we always have a parent HWND.
  // We need to use the HWND Property to be acknowledged as a Foreground
  // Application on Vista. Otherwise, the elevation prompt will appear minimized
  // on the taskbar.
  if (!ShellExecuteExEnsureParent(&sei)) {
    HRESULT hr(HRESULTFromLastError());
    ASSERT(false,
        (_T("Shell::Execute - ShellExecuteEx failed][%s][0x%x]"), file, hr));
    return hr;
  }

  return S_OK;
}

HRESULT Shell::BasicGetSpecialFolder(DWORD csidl, CString* folder_path) {
  ASSERT1(folder_path);

  // Get a ITEMIDLIST* (called a PIDL in the MSDN documentation) to the
  // special folder
  scoped_any<ITEMIDLIST*, close_co_task_free> folder_location;
  RET_IF_FAILED(::SHGetFolderLocation(NULL,
                                      csidl,
                                      NULL,
                                      0,
                                      address(folder_location)));
  ASSERT(get(folder_location), (_T("")));

  // Get an interface to the Desktop folder
  CComPtr<IShellFolder> desktop_folder;
  RET_IF_FAILED(::SHGetDesktopFolder(&desktop_folder));
  ASSERT1(desktop_folder);

  // Ask the desktop for the display name of the special folder
  STRRET str_return;
  SetZero(str_return);
  str_return.uType = STRRET_WSTR;
  RET_IF_FAILED(desktop_folder->GetDisplayNameOf(get(folder_location),
                                                 SHGDN_FORPARSING,
                                                 &str_return));

  // Get the display name of the special folder and return it
  scoped_any<wchar_t*, close_co_task_free> folder_name;
  RET_IF_FAILED(::StrRetToStr(&str_return,
                              get(folder_location),
                              address(folder_name)));
  *folder_path = get(folder_name);

  return S_OK;
}

HRESULT Shell::GetSpecialFolder(DWORD csidl,
                                bool create_if_missing,
                                CString* folder_path) {
  ASSERT(folder_path, (L""));

  HRESULT hr = Shell::BasicGetSpecialFolder(csidl, folder_path);

  // If the folder does not exist, ::SHGetFolderLocation may return error
  // code ERROR_FILE_NOT_FOUND.
  if (create_if_missing) {
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        (SUCCEEDED(hr) && !File::Exists(*folder_path))) {
      hr = Shell::BasicGetSpecialFolder(csidl | CSIDL_FLAG_CREATE, folder_path);
    }
  }
  ASSERT(FAILED(hr) || File::Exists(*folder_path), (_T("")));

  return hr;
}

struct {
  const TCHAR* name;
  const DWORD csidl;
} const folder_mappings[] = {
  L"APPDATA",            CSIDL_APPDATA,
  L"DESKTOP",            CSIDL_DESKTOPDIRECTORY,
  L"LOCALAPPDATA",       CSIDL_LOCAL_APPDATA,
  L"MYMUSIC",            CSIDL_MYMUSIC,
  L"MYPICTURES",         CSIDL_MYPICTURES,
  L"PROGRAMFILES",       CSIDL_PROGRAM_FILES,
  L"PROGRAMFILESCOMMON", CSIDL_PROGRAM_FILES_COMMON,
  L"PROGRAMS",           CSIDL_PROGRAMS,
  L"STARTMENU",          CSIDL_STARTMENU,
  L"STARTUP",            CSIDL_STARTUP,
  L"SYSTEM",             CSIDL_SYSTEM,
  L"WINDOWS",            CSIDL_WINDOWS,
};

HRESULT Shell::GetSpecialFolderKeywordsMapping(
    std::map<CString, CString>* special_folders_map) {
  ASSERT1(special_folders_map);

  special_folders_map->clear();

  for (size_t i = 0; i < arraysize(folder_mappings); ++i) {
    CString name(folder_mappings[i].name);
    DWORD csidl(folder_mappings[i].csidl);
    CString folder;
    HRESULT hr = GetSpecialFolder(csidl, false, &folder);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[Shell::GetSpecialFolderKeywordsMapping]")
                    _T("[failed to retrieve %s]"), name));
      continue;
    }
    special_folders_map->insert(std::make_pair(name, folder));
  }

  // Get the current module directory
  CString module_dir = app_util::GetModuleDirectory(::GetModuleHandle(NULL));
  ASSERT1(module_dir.GetLength() > 0);
  special_folders_map->insert(std::make_pair(_T("CURRENTMODULEDIR"),
                                             module_dir));

  return S_OK;
}

HRESULT Shell::GetApplicationExecutablePath(const CString& exe,
                                            CString* path) {
  ASSERT1(path);

  CString reg_key_name = AppendRegKeyPath(kRegKeyApplicationPath, exe);
  return RegKey::GetValue(reg_key_name, kRegKeyPathValue, path);
}

}  // namespace omaha

