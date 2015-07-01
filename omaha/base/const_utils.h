// Copyright 2004-2010 Google Inc.
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

#ifndef OMAHA_BASE_CONST_UTILS_H_
#define OMAHA_BASE_CONST_UTILS_H_

namespace omaha {

// The registry key for the registered application path. Take a look at
// http://msdn2.microsoft.com/en-us/library/ms997545.aspx for more information.
#define kRegKeyApplicationPath \
    _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths")
#define kRegKeyPathValue _T("Path")

#define USER_REG_VISTA_LOW_INTEGRITY_HKCU \
    _T("Software\\Microsoft\\Internet Explorer\\") \
    _T("InternetRegistry\\REGISTRY\\USER")

// Including this property specification in the msiexec command line will
// prevent MSI from rebooting in all cases.
const TCHAR* const kMsiSuppressAllRebootsCmdLine = _T("REBOOT=ReallySuppress");

// TODO(omaha): We could probably move most of these to browser_utils.
// TODO(omaha): The rest are Windows constants. Maybe name this file as such.

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
#define kChromeBrowserName _T("Google Chrome")

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

// SEHOP setting under IFEO.
#define kRegKeyWindowsNTCurrentVersion \
    _T("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion")
#define kRegKeyImageFileExecutionOptions \
      kRegKeyWindowsNTCurrentVersion _T("\\") _T("Image File Execution Options")
#define kRegKeyDisableSEHOPValue _T("DisableExceptionChainValidation")


}  // namespace omaha

#endif  // OMAHA_BASE_CONST_UTILS_H_
