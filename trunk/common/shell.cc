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

#include "omaha/common/shell.h"

#include <shlobj.h>
#include <shellapi.h>
#include "omaha/common/app_util.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/commands.h"
#include "omaha/common/const_utils.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/system.h"
#include "omaha/common/utils.h"

namespace omaha {

// create and store a shortcut
// uses shell IShellLink and IPersistFile interfaces
HRESULT Shell::CreateLink(const TCHAR *source,
                          const TCHAR *destination,
                          const TCHAR *working_dir,
                          const TCHAR *arguments,
                          const TCHAR *description,
                          WORD hotkey_virtual_key_code,
                          WORD hotkey_modifiers,
                          const TCHAR *icon) {
  ASSERT1(source);
  ASSERT1(destination);
  ASSERT1(working_dir);
  ASSERT1(arguments);
  ASSERT1(description);

  scoped_co_init co_init(COINIT_APARTMENTTHREADED);
  HRESULT hr = co_init.hresult();
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    UTIL_LOG(LEVEL_ERROR, (_T("[Shell::CreateLink - failed to co_init]"), hr));
    return hr;
  }

  UTIL_LOG(L1, (_T("[Create shell link]")
                _T("[source %s dest %s dir %s arg %s desc %s hotkey %x:%x]"),
                source, destination, working_dir, arguments, description,
                hotkey_modifiers, hotkey_virtual_key_code));

  // Get a pointer to the IShellLink interface
  CComPtr<IShellLink> shell_link;

  RET_IF_FAILED(shell_link.CoCreateInstance(CLSID_ShellLink));
  ASSERT(shell_link, (L""));

  // Set the path to the shortcut target and add the description
  VERIFY1(SUCCEEDED(shell_link->SetPath(source)));
  VERIFY1(SUCCEEDED(shell_link->SetArguments(arguments)));
  VERIFY1(SUCCEEDED(shell_link->SetDescription(description)));
  VERIFY1(SUCCEEDED(shell_link->SetWorkingDirectory(working_dir)));

  // If we are given an icon, then set it
  // For now, we always use the first icon if this happens to have multiple ones
  if (icon) {
    VERIFY1(SUCCEEDED(shell_link->SetIconLocation(icon, 0)));
  }

// C4201: nonstandard extension used : nameless struct/union
#pragma warning(disable : 4201)
  union {
    WORD flags;
    struct {                  // little-endian machine:
      WORD virtual_key:8;     // low order byte
      WORD modifiers:8;       // high order byte
    };
  } hot_key;
#pragma warning(default : 4201)

  hot_key.virtual_key = hotkey_virtual_key_code;
  hot_key.modifiers = hotkey_modifiers;

  if (hot_key.flags) {
    shell_link->SetHotkey(hot_key.flags);
  }

  // Query IShellLink for the IPersistFile interface for saving the shortcut in
  // persistent storage
  CComQIPtr<IPersistFile> persist_file(shell_link);
  if (!persist_file)
    return E_FAIL;

  // Save the link by calling IPersistFile::Save
  RET_IF_FAILED(persist_file->Save(destination, TRUE));

  return S_OK;
}

HRESULT Shell::RemoveLink(const TCHAR *link) {
  ASSERT(link, (L""));
  ASSERT(*link, (L""));

  return File::Remove(link);
}

// Open a URL in a new browser window
HRESULT Shell::OpenLinkInNewWindow(const TCHAR* url, UseBrowser use_browser) {
  ASSERT1(url);

  HRESULT hr = S_OK;
  CString browser_path;

  // Try to open with default browser
  if (use_browser == USE_DEFAULT_BROWSER) {
    // Load full browser path from regkey
    hr = GetDefaultBrowserPath(&browser_path);

    // If there is a default browser and it is not AOL, load the url in that
    // browser
    if (SUCCEEDED(hr) && !String_Contains(browser_path, _T("aol"))) {
      if (!browser_path.IsEmpty()) {
        // Have we figured out how to append the URL onto the browser path?
        bool acceptable_url = false;

        if (ReplaceCString(browser_path, _T("\"%1\""), url)) {
          // the "browser.exe "%1"" case
          acceptable_url = true;
        } else if (ReplaceCString(browser_path, _T("%1"), url)) {
          // the "browser.exe %1 "case
          acceptable_url = true;
        } else if (ReplaceCString(browser_path, _T("-nohome"), url)) {
          // the "browser.exe -nohome" case
          acceptable_url = true;
        } else {
          // the browser.exe case.
          // simply append the quoted url.
          EnclosePath(&browser_path);
          browser_path.AppendChar(_T(' '));
          CString quoted_url(url);
          EnclosePath(&quoted_url);
          browser_path.Append(quoted_url);
          acceptable_url = true;
        }

        if (acceptable_url) {
          hr = System::ShellExecuteCommandLine(browser_path, NULL, NULL);
          if (SUCCEEDED(hr)) {
            return S_OK;
          } else {
            UTIL_LOG(LE, (_T("[Shell::OpenLinkInNewWindow]")
                          _T("[failed to start default browser to open url]")
                          _T("[%s][0x%x]"), url, hr));
          }
        }
      }
    }
  }

  // Try to open with IE if can't open with default browser or required
  if (use_browser == USE_DEFAULT_BROWSER ||
      use_browser == USE_INTERNET_EXPLORER) {
    hr = RegKey::GetValue(kRegKeyIeClass, kRegValueIeClass, &browser_path);
    if (SUCCEEDED(hr)) {
      hr = System::ShellExecuteProcess(browser_path, url, NULL, NULL);
      if (SUCCEEDED(hr)) {
        return S_OK;
      } else {
        UTIL_LOG(LE, (_T("[Shell::OpenLinkInNewWindow]")
                      _T("[failed to start IE to open url][%s][0x%x]"),
                      url, hr));
      }
    }
  }

  // Try to open with Firefox if can't open with default browser or required
  if (use_browser == USE_DEFAULT_BROWSER || use_browser == USE_FIREFOX) {
    hr = RegKey::GetValue(kRegKeyFirefox, kRegValueFirefox, &browser_path);
    if (SUCCEEDED(hr) && !browser_path.IsEmpty()) {
      ReplaceCString(browser_path, _T("%1"), url);
      hr = System::ShellExecuteCommandLine(browser_path, NULL, NULL);
      if (SUCCEEDED(hr)) {
        return S_OK;
      } else {
        UTIL_LOG(LE, (_T("[Shell::OpenLinkInNewWindow]")
                      _T("[failed to start Firefox to open url][%s][0x%x]"),
                      url, hr));
      }
    }
  }

  // ShellExecute the url directly as a last resort
  hr = Shell::Execute(url);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Shell::OpenLinkInNewWindow]")
                  _T("[failed to run ShellExecute to open url][%s][0x%x]"),
                  url, hr));
  }

  return hr;
}

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
  // create a dummy parent window for us.
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

#pragma warning(disable : 4510 4610)
// C4510: default constructor could not be generated
// C4610: struct can never be instantiated - user defined constructor required
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
#pragma warning(default : 4510 4610)

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

HRESULT Shell::DeleteDirectory(const TCHAR* dir) {
  ASSERT1(dir && *dir);

  if (!SafeDirectoryNameForDeletion(dir)) {
    return E_INVALIDARG;
  }

  uint32 dir_len = lstrlen(dir);
  if (dir_len >= MAX_PATH) {
    return E_INVALIDARG;
  }

  // the 'from' must be double-terminated with 0. Reserve space for one more
  // zero at the end
  TCHAR from[MAX_PATH + 1] = {0};
  lstrcpyn(from, dir, MAX_PATH);
  from[1 + dir_len] = 0;    // the second zero terminator.

  SHFILEOPSTRUCT file_op = {0};

  file_op.hwnd   = 0;
  file_op.wFunc  = FO_DELETE;
  file_op.pFrom  = from;
  file_op.pTo    = 0;
  file_op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

  // ::SHFileOperation returns non-zero on errors
  return ::SHFileOperation(&file_op) ? HRESULTFromLastError() : S_OK;
}

HRESULT Shell::GetApplicationExecutablePath(const CString& exe,
                                            CString* path) {
  ASSERT1(path);

  CString reg_key_name = AppendRegKeyPath(kRegKeyApplicationPath, exe);
  return RegKey::GetValue(reg_key_name, kRegKeyPathValue, path);
}

}  // namespace omaha

