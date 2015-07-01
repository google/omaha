// Copyright 2006 Google Inc.
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

// TODO(omaha): namespaces

#ifndef OMAHA_BASE_BROWSER_UTILS_H_
#define OMAHA_BASE_BROWSER_UTILS_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

// Must be kept in sync with the enum in base/omaha3_idl.idl. Do not include
// BROWSER_MAX in the IDL file.
// Do not move or remove existing elements.
enum BrowserType {
  BROWSER_UNKNOWN = 0,
  BROWSER_DEFAULT = 1,
  BROWSER_IE      = 2,
  BROWSER_FIREFOX = 3,
  BROWSER_CHROME  = 4,
  // Add new browsers above this.
  BROWSER_MAX
};

// Read the browser information from legacy keys. See
// http://support.microsoft.com/kb/224816.
HRESULT GetLegacyDefaultBrowserInfo(CString* name, CString* browser_path);

// Gets the default browser name.
// When calling this method from the local system account, the caller must
// impersonate the user first. This assumes the user profile is loaded under
// HKEY_USERS, which is true if the user has an interactive session.
// Otherwise, the caller must load the profile for the user before
// calling the function.
HRESULT GetDefaultBrowserName(CString* name);

// Returns the default browser type.
HRESULT GetDefaultBrowserType(BrowserType* type);

// Get the default browser path
HRESULT GetDefaultBrowserPath(CString* path);

// Get the default profile of Firefox
HRESULT GetFirefoxDefaultProfile(CString* name, CString* path);

// Returns the executable name corresponding to the browser type.
HRESULT BrowserTypeToProcessName(BrowserType type, CString* exe_name);

// Returns the absolute filename of the browser executable.
HRESULT GetBrowserImagePath(BrowserType type, CString* path);

// Terminates all the browsers identified by type for a user.
HRESULT TerminateBrowserProcess(BrowserType type,
                                const CString& sid,
                                int timeout_msec,
                                bool* found);

// Waits for all instances of browser to die.
HRESULT WaitForBrowserToDie(BrowserType type,
                            const CString& sid,
                            int timeout_msec);

// Launches the browser using RunAsCurrentUser. On failure, falls back to
// using ShellExecute.
HRESULT RunBrowser(BrowserType type, const CString& url);

// Returns the current font size selection in Internet Explorer.
// Possible font size values:
// 0 : smallest font
// 1 : small font
// 2 : medium font
// 3 : large font
// 4 : largest font
HRESULT GetIeFontSize(uint32* font_size);

}  // namespace omaha

#endif  // OMAHA_BASE_BROWSER_UTILS_H_
