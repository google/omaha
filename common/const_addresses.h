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
// Constants for dealing with machines.
//
// Manifests are requested over HTTPS. All other network communication goes
// over HTTP.

#ifndef OMAHA_COMMON_CONST_ADDRESSES_H__
#define OMAHA_COMMON_CONST_ADDRESSES_H__

#include <tchar.h>

namespace omaha {

// Static string that gives the main Google website address
#define kGoogleHttpServer         _T("www.google.com")

// Static string used as an identity for the "Omaha" Google domain.
#define kGoopdateServer           _T("tools.google.com")

// HTTP protocol prefix
#define kProtoSuffix              _T("://")
#define kFileProtoScheme          _T("file")
#define kHttpProtoScheme          _T("http")
#define kHttpsProtoScheme         _T("https")
#define kHttpProto                kHttpProtoScheme kProtoSuffix
#define kHttpsProto               kHttpsProtoScheme kProtoSuffix
#define kFileProto                kFileProtoScheme kProtoSuffix

// Default ports for proxies
#define kDefaultHttpProxyPort     80
#define kDefaultSslProxyPort      443

// Update checks and manifest requests.
const TCHAR* const kUrlUpdateCheck =
    _T("https://tools.google.com/service/update2");

// Pings.
const TCHAR* const kUrlPing = _T("http://tools.google.com/service/update2");

// WebPlugin checks
const TCHAR* const kUrlWebPluginCheck =
    _T("https://tools.google.com/service/update2/oneclick");

// Crash reports.
const TCHAR* const kUrlCrashReport =
    _T("http://clients2.google.com/cr/report");

// More information url.
const TCHAR* const kUrlMoreInformation =
    _T("http://www.google.com/support/installer/?");

// Code Red check url.
const TCHAR* const kUrlCodeRedCheck =
    _T("http://cr-tools.clients.google.com/service/check2");

// Usage stats url.
const TCHAR* const kUrlUsageStatsReport =
    _T("http://tools.google.com/tbproxy/usagestats");

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_ADDRESSES_H__

