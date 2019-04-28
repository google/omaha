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
//
// create shortcut links
// remove shortcut links

#ifndef OMAHA_BASE_SHELL_H_
#define OMAHA_BASE_SHELL_H_

#include <windows.h>
#include <shlobj.h>   // for the CSIDL definitions
#include <atlstr.h>
#include <map>

#include "base/basictypes.h"

namespace omaha {

// TODO(omaha): Use BrowserType instead.
enum UseBrowser {
  USE_DEFAULT_BROWSER = 0,
  USE_INTERNET_EXPLORER = 1,
  USE_FIREFOX = 2
};

class Shell {
 public:
  // Execute a file
  static HRESULT Execute(const TCHAR* file);

  // Get the location of a special folder.  The special folders are identified
  // by a unique integer - see the platform SDK files shfolder.h and
  // shlobj.h.  (These names will be the localized versions for the
  // system that is running.)
  static HRESULT GetSpecialFolder(DWORD csidl,
                                  bool create_if_missing,
                                  CString* folder_path);

  // Get a mapping from special folder "env var" names to special folder
  // pathnames.
  // Provides mappings for: APPDATA, DESKTOP, LOCALAPPDATA, MYMUSIC, MYPICTURES,
  // PROGRAMFILES, PROGRAMFILESCOMMON, PROGRAMS, STARTMENU, STARTUP, SYSTEM,
  // WINDOWS.
  static HRESULT GetSpecialFolderKeywordsMapping(
      std::map<CString, CString>* special_folders_map);

  // Reads the application executable path from
  // HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths.
  static HRESULT GetApplicationExecutablePath(const CString& exe,
                                              CString* path);

 private:

  static HRESULT BasicGetSpecialFolder(DWORD csidl, CString* folder_path);

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace omaha

#endif  // OMAHA_BASE_SHELL_H_
