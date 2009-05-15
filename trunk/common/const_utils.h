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
//
// Constants used in util functions

#ifndef OMAHA_COMMON_CONST_UTILS_H__
#define OMAHA_COMMON_CONST_UTILS_H__

namespace omaha {

// The registry key for the registered application path. Take a look at
// http://msdn2.microsoft.com/en-us/library/ms997545.aspx for more information.
#define kRegKeyApplicationPath \
    _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths")
#define kRegKeyPathValue _T("Path")

// The regkey for the default browser the Windows Shell opens.
#define kRegKeyDefaultBrowser _T("SOFTWARE\\Clients\\StartMenuInternet")
#define kRegKeyUserDefaultBrowser _T("HKCU\\") kRegKeyDefaultBrowser
#define kRegKeyMachineDefaultBrowser _T("HKLM\\") kRegKeyDefaultBrowser
#define kRegKeyShellOpenCommand _T("\\shell\\open\\command")
#define kRegKeyLegacyDefaultBrowser _T("HKCR\\http")
#define kRegKeyLegacyDefaultBrowserCommand \
            kRegKeyLegacyDefaultBrowser kRegKeyShellOpenCommand

#define kIeExeName _T("IEXPLORE.EXE")
#define kFirefoxExeName _T("FIREFOX.EXE")
#define kChromeExeName _T("CHROME.EXE")

// The regkey for proxy settings for IE.
const TCHAR* const kRegKeyIESettings =
    _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
const TCHAR* const kRegKeyIESettingsConnections = _T("Connections");
const TCHAR* const kRegValueIEDefaultConnectionSettings =
    _T("DefaultConnectionSettings");
const TCHAR* const kRegValueIEProxyEnable = _T("ProxyEnable");
const TCHAR* const kRegValueIEProxyServer = _T("ProxyServer");
const TCHAR* const kRegValueIEAutoConfigURL = _T("AutoConfigURL");

// Internet Explorer.
#define kRegKeyIeClass \
    _T("HKCR\\CLSID\\{0002DF01-0000-0000-C000-000000000046}\\LocalServer32")
#define kRegValueIeClass _T("")

// Firefox.
#define kRegKeyFirefox \
    _T("HKCR\\Applications\\FIREFOX.EXE\\shell\\open\\command")
#define kRegValueFirefox        _T("")
#define kFullRegKeyFirefox      _T("HKLM\\SOFTWARE\\Mozilla\\Mozilla Firefox")
#define kRegKeyFirefoxPlugins   _T("plugins")
#define kFirefoxCurrentVersion  _T("CurrentVersion")
#define kFirefoxInstallDir      _T("Install Directory")

// Chrome.
#define kRegKeyChrome _T("HKCR\\Applications\\chrome.exe\\shell\\open\\command")
#define kRegValueChrome _T("")

// Amount of disk space required for program files.
#ifdef _DEBUG
// 100 MB since debug usually includes PDBs in addition to huge EXEs.
#define kSpaceRequiredToInstallProgramFiles (100LL * 1000LL * 1000LL)
#else
#define kSpaceRequiredToInstallProgramFiles (10LL * 1000LL * 1000LL)  // 10MB
#endif

// Preferred amount of disk space for data (choose first location if found).
#define kSpacePreferredToInstallDataDir (1000LL * 1000LL * 1000LL)

// Amount of disk space required for data (choose first location if
// could not find a location with the preferred amount of space).
#define kSpaceRequiredToInstallDataDir (500LL * 1000LL * 1000LL)

// Maximum file size allowed for performing authentication.
#define kMaxFileSizeForAuthentication (512000000L)    // 512MB

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_UTILS_H__
