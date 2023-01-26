// Copyright 2011 Google Inc.
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

#ifndef OMAHA_COMMON_XML_CONST_H_
#define OMAHA_COMMON_XML_CONST_H_

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>

namespace omaha {

namespace xml {

const int kGuidLen = 38;

extern const TCHAR* const kXmlDirective;
extern const TCHAR* const kXmlNamespace;

namespace element {

extern const TCHAR* const kAction;
extern const TCHAR* const kActions;
extern const TCHAR* const kApp;
extern const TCHAR* const kData;
extern const TCHAR* const kDayStart;
extern const TCHAR* const kEvent;
extern const TCHAR* const kHw;
extern const TCHAR* const kManifest;
extern const TCHAR* const kOs;
extern const TCHAR* const kPackage;
extern const TCHAR* const kPackages;
extern const TCHAR* const kPing;
extern const TCHAR* const kRequest;
extern const TCHAR* const kResponse;
extern const TCHAR* const kSystemRequirements;
extern const TCHAR* const kUpdateCheck;
extern const TCHAR* const kUrl;
extern const TCHAR* const kUrls;

}  // namespace element

namespace attribute {

extern const TCHAR* const kActive;
extern const TCHAR* const kAdditionalParameters;
extern const TCHAR* const kAppBytesDownloaded;
extern const TCHAR* const kAppBytesTotal;
extern const TCHAR* const kAppGuid;
extern const TCHAR* const kApplicationName;
extern const TCHAR* const kAppId;
extern const TCHAR* const kArch;
extern const TCHAR* const kArguments;
extern const TCHAR* const kAvx;
extern const TCHAR* const kBrandCode;
extern const TCHAR* const kBrowserType;
extern const TCHAR* const kClientId;
extern const TCHAR* const kCodebase;
extern const TCHAR* const kCohort;
extern const TCHAR* const kCohortHint;
extern const TCHAR* const kCohortName;
extern const TCHAR* const kCountry;
extern const TCHAR* const kDaysSinceLastActivePing;
extern const TCHAR* const kDaysSinceLastRollCall;
extern const TCHAR* const kDayOfLastActivity;
extern const TCHAR* const kDayOfLastRollCall;
extern const TCHAR* const kDedup;
extern const TCHAR* const kDlPref;
extern const TCHAR* const kDomainJoined;
extern const TCHAR* const kDownloaded;
extern const TCHAR* const kDownloader;
extern const TCHAR* const kDownloadTime;
extern const TCHAR* const kElapsedDays;
extern const TCHAR* const kElapsedSeconds;
extern const TCHAR* const kErrorCode;
extern const TCHAR* const kErrorUrl;
extern const TCHAR* const kEvent;
extern const TCHAR* const kEventResult;
extern const TCHAR* const kEventType;
extern const TCHAR* const kExperiments;
extern const TCHAR* const kExtraCode1;
extern const TCHAR* const kHash;
extern const TCHAR* const kHashSha256;
extern const TCHAR* const kIndex;
extern const TCHAR* const kInstallationId;
extern const TCHAR* const kInstallDate;
extern const TCHAR* const kInstalledAgeDays;
extern const TCHAR* const kInstallSource;
extern const TCHAR* const kInstallTime;
extern const TCHAR* const kIsBundled;
extern const TCHAR* const kIsMachine;
extern const TCHAR* const kLang;
extern const TCHAR* const kMinOSVersion;
extern const TCHAR* const kName;
extern const TCHAR* const kNextVersion;
extern const TCHAR* const kOriginURL;
extern const TCHAR* const kParameter;
extern const TCHAR* const kPeriodOverrideSec;
extern const TCHAR* const kPhysMemory;
extern const TCHAR* const kPingFreshness;
extern const TCHAR* const kPlatform;
extern const TCHAR* const kProtocol;
extern const TCHAR* const kRequestId;
extern const TCHAR* const kRequired;
extern const TCHAR* const kRollbackAllowed;
extern const TCHAR* const kRun;
extern const TCHAR* const kServicePack;
extern const TCHAR* const kSessionId;
extern const TCHAR* const kShellVersion;
extern const TCHAR* const kSignature;
extern const TCHAR* const kSize;
extern const TCHAR* const kSourceUrlIndex;
extern const TCHAR* const kSse;
extern const TCHAR* const kSse2;
extern const TCHAR* const kSse3;
extern const TCHAR* const kSsse3;
extern const TCHAR* const kSse41;
extern const TCHAR* const kSse42;
extern const TCHAR* const kStateCancelled;
extern const TCHAR* const kStatus;
extern const TCHAR* const kSuccessAction;
extern const TCHAR* const kSuccessUrl;
extern const TCHAR* const kTargetChannel;
extern const TCHAR* const kTargetVersionPrefix;
extern const TCHAR* const kTestSource;
extern const TCHAR* const kTerminateAllBrowsers;
extern const TCHAR* const kTimeSinceDownloadStart;
extern const TCHAR* const kTimeSinceUpdateAvailable;
extern const TCHAR* const kTotal;
extern const TCHAR* const kTTToken;
extern const TCHAR* const kUpdateCheckTime;
extern const TCHAR* const kUpdateDisabled;
extern const TCHAR* const kUpdater;
extern const TCHAR* const kUpdaterVersion;
extern const TCHAR* const kUrl;
extern const TCHAR* const kUserId;
extern const TCHAR* const kVersion;
extern const TCHAR* const kXmlns;

// Prefix for App-Defined attributes.
extern const TCHAR* const kAppDefinedPrefix;

}  // namespace attribute

namespace value {

extern const TCHAR* const kBits;
extern const TCHAR* const kCacheable;
extern const TCHAR* const kClientRegulated;
extern const TCHAR* const kDirect;
extern const TCHAR* const kFalse;
extern const TCHAR* const kInstall;
extern const TCHAR* const kInstallData;
extern const TCHAR* const kPostinstall;
extern const TCHAR* const kPreinstall;
extern const TCHAR* const kRequestType;
extern const TCHAR* const kStatusError;
extern const TCHAR* const kSuccessActionDefault;
extern const TCHAR* const kSuccessActionExitSilently;
extern const TCHAR* const kSuccessActionExitSilentlyOnLaunchCmd;
extern const TCHAR* const kTrue;
extern const TCHAR* const kUid;
extern const TCHAR* const kUntrusted;
extern const TCHAR* const kUpdater;
extern const TCHAR* const kUpdate;
extern const TCHAR* const kVersion3;

}  // namespace value

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_XML_CONST_H_
