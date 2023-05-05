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

// The urls below fall back to http for transport failover purposes. In a small
// number of cases, http requests could succeed even though https requests
// have failed. Since the update checks and pings are critical for update
// functionality, these two types of requests may use unencrypted connections if
// https failed.
//
// Update checks.
// The channel for update checks is secured by using CUP to sign the messages.
// It does not depend solely on https security in any case.
const TCHAR* const kUrlUpdateCheck =
    _T("https://update.") COMPANY_DOMAIN_BASE _T("apis.com/service/update2");

// Pings.
const TCHAR* const kUrlPing =
  _T("https://update.") COMPANY_DOMAIN_BASE _T("apis.com/service/update2");

// The urls below never fall back to http.
//
// Crash reports.
const TCHAR* const kUrlCrashReport =
    _T("https://clients2.") COMPANY_DOMAIN _T("/cr/report");

// More information url.
// Must allow query parameters to be appended to it.
const TCHAR* const kUrlMoreInfo =
    _T("https://www.") COMPANY_DOMAIN _T("/support/installer/?");

// Code Red check url.
const TCHAR* const kUrlCodeRedCheck =
    _T("https://clients2.") COMPANY_DOMAIN _T("/service/check2?crx3=true");

// Usage stats url.
const TCHAR* const kUrlUsageStatsReport =
    _T("https://clients5.") COMPANY_DOMAIN _T("/tbproxy/usagestats");

// App logo.
const TCHAR* const kUrlAppLogo =
    _T("https://dl.") COMPANY_DOMAIN _T("/update2/installers/icons/");

#if defined(HAS_DEVICE_MANAGEMENT)

// Device Management API url.
const TCHAR* const kUrlDeviceManagement =
    _T("https://m.") COMPANY_DOMAIN _T("/devicemanagement/data/api");

#endif  // defined(HAS_DEVICE_MANAGEMENT)

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_ADDRESSES_H_
