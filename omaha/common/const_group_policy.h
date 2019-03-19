// Copyright 2008-2009 Google Inc.
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

#ifndef OMAHA_COMMON_CONST_GROUP_POLICY_H_
#define OMAHA_COMMON_CONST_GROUP_POLICY_H_

#include <tchar.h>
#include "omaha/base/constants.h"

namespace omaha {

// Key containing Omaha Group Policy settings. All policies are in HKLM.
// TODO(omaha): Rename Omaha.
const TCHAR* const kRegKeyGoopdateGroupPolicy =
    MACHINE_KEY GOOPDATE_POLICIES_RELATIVE;

// Preferences Category.
const TCHAR* const kRegValueAutoUpdateCheckPeriodOverrideMinutes =
    _T("AutoUpdateCheckPeriodMinutes");
const TCHAR* const kRegValueUpdatesSuppressedStartHour   =
    _T("UpdatesSuppressedStartHour");
const TCHAR* const kRegValueUpdatesSuppressedStartMin    =
    _T("UpdatesSuppressedStartMin");
const TCHAR* const kRegValueUpdatesSuppressedDurationMin =
    _T("UpdatesSuppressedDurationMin");

// This policy specifies what kind of download URLs could be returned to the
// client in the update response and in which order of priority. The client
// provides this information in the update request as a hint for the server.
// The server may decide to ignore the hint. As a general idea, some urls are
// cacheable, some urls have higher bandwidth, and some urls are slighlty more
// secure since they are https.
const TCHAR* const kRegValueDownloadPreference = _T("DownloadPreference");

// Specifies that urls that can be cached by proxies are preferred.
const TCHAR* const kDownloadPreferenceCacheable = _T("cacheable");

// Proxy Server Category.  (The registry keys used, and the values of ProxyMode,
// directly mirror that of Chrome.  However, we omit ProxyBypassList, as the
// domains that Omaha uses are largely fixed.)
const TCHAR* const kRegValueProxyMode   = _T("ProxyMode");
const TCHAR* const kRegValueProxyServer = _T("ProxyServer");
const TCHAR* const kRegValueProxyPacUrl = _T("ProxyPacUrl");

const TCHAR* const kProxyModeDirect       = _T("direct");
const TCHAR* const kProxyModeAutoDetect   = _T("auto_detect");
const TCHAR* const kProxyModePacScript    = _T("pac_script");
const TCHAR* const kProxyModeFixedServers = _T("fixed_servers");
const TCHAR* const kProxyModeSystem       = _T("system");

// Applications Category.
// The prefix strings have the app's GUID appended to them.
const TCHAR* const kRegValueInstallAppsDefault  = _T("InstallDefault");
const TCHAR* const kRegValueInstallAppPrefix    = _T("Install");
const TCHAR* const kRegValueUpdateAppsDefault   = _T("UpdateDefault");
const TCHAR* const kRegValueUpdateAppPrefix     = _T("Update");
const TCHAR* const kRegValueTargetVersionPrefix = _T("TargetVersionPrefix");
const TCHAR* const kRegValueRollbackToTargetVersion
    = _T("RollbackToTargetVersion");

const int kPolicyDisabled              = 0;
const int kPolicyEnabled               = 1;
const int kPolicyManualUpdatesOnly     = 2;
const int kPolicyAutomaticUpdatesOnly  = 3;

const bool kInstallPolicyDefault    = kPolicyEnabled;
const bool kUpdatePolicyDefault     = kPolicyEnabled;

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_GROUP_POLICY_H_
