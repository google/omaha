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
// Constants for dealing with machines.
//
// Manifests are requested over HTTPS. All other network communication goes
// over HTTP.

#ifndef OMAHA_BASE_CONST_ADDRESSES_H_
#define OMAHA_BASE_CONST_ADDRESSES_H_

#include <tchar.h>
#include "omaha/base/constants.h"

namespace omaha {

// Static string that gives the main Google website address
// TODO(omaha): Rename this as a connection-check URL. Name should be in caps
// and not include "Google".
#define kGoogleHttpServer _T("www.") COMPANY_DOMAIN

// Static string used as an identity for the "Omaha" Google domain.
// TODO(omaha): Rename this as a plug-in domain.
const TCHAR* const kGoopdateServer = _T("tools.") COMPANY_DOMAIN;

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
    kHttpsProto _T("tools.") COMPANY_DOMAIN _T("/service/update2");

// Pings.
const TCHAR* const kUrlPing =
    _T("http://tools.") COMPANY_DOMAIN _T("/service/update2");

// Crash reports.
const TCHAR* const kUrlCrashReport =
    _T("http://clients2.") COMPANY_DOMAIN _T("/cr/report");

// More information url.
// Must allow query parameters to be appended to it.
const TCHAR* const kUrlMoreInfo =
    _T("http://www.") COMPANY_DOMAIN _T("/support/installer/?");

// Code Red check url.
const TCHAR* const kUrlCodeRedCheck =
    _T("http://cr-tools.clients.") COMPANY_DOMAIN _T("/service/check2");

// Usage stats url.
const TCHAR* const kUrlUsageStatsReport =
    _T("http://clients5.") COMPANY_DOMAIN _T("/tbproxy/usagestats");

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_ADDRESSES_H_
