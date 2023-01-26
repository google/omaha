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

#include "omaha/common/xml_const.h"

namespace omaha {

namespace xml {

const TCHAR* const kXmlDirective =
    _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");

const TCHAR* const kXmlNamespace = NULL;

namespace element {

const TCHAR* const kAction = _T("action");
const TCHAR* const kActions = _T("actions");
const TCHAR* const kApp = _T("app");
const TCHAR* const kData = _T("data");
const TCHAR* const kDayStart = _T("daystart");

// ping element. The element is named "event" for legacy reasons.
const TCHAR* const kEvent = _T("event");

const TCHAR* const kHw = _T("hw");
const TCHAR* const kManifest = _T("manifest");
const TCHAR* const kOs = _T("os");
const TCHAR* const kPackage = _T("package");
const TCHAR* const kPackages = _T("packages");

// didrun element. The element is named "ping" for legacy reasons.
const TCHAR* const kPing = _T("ping");
const TCHAR* const kRequest = _T("request");
const TCHAR* const kResponse = _T("response");
const TCHAR* const kSystemRequirements = _T("systemrequirements");
const TCHAR* const kUpdateCheck = _T("updatecheck");
const TCHAR* const kUrl = _T("url");
const TCHAR* const kUrls = _T("urls");

}  // namespace element

namespace attribute {

const TCHAR* const kActive = _T("active");
const TCHAR* const kAdditionalParameters = _T("ap");
const TCHAR* const kAppBytesDownloaded = _T("downloaded");
const TCHAR* const kAppBytesTotal = _T("total");
const TCHAR* const kAppGuid = _T("appguid");
const TCHAR* const kApplicationName = _T("appname");
const TCHAR* const kAppId = _T("appid");
const TCHAR* const kArch = _T("arch");
const TCHAR* const kArguments = _T("arguments");
const TCHAR* const kAvx = _T("avx");
const TCHAR* const kBrandCode = _T("brand");
const TCHAR* const kBrowserType = _T("browser");
const TCHAR* const kClientId = _T("client");
const TCHAR* const kCodebase = _T("codebase");
const TCHAR* const kCohort = _T("cohort");
const TCHAR* const kCohortHint = _T("cohorthint");
const TCHAR* const kCohortName = _T("cohortname");
const TCHAR* const kCountry = _T("country");
const TCHAR* const kDaysSinceLastActivePing = _T("a");
const TCHAR* const kDaysSinceLastRollCall = _T("r");
const TCHAR* const kDayOfLastActivity = _T("ad");
const TCHAR* const kDayOfLastRollCall = _T("rd");
const TCHAR* const kDedup = _T("dedup");
const TCHAR* const kDlPref = _T("dlpref");
const TCHAR* const kDomainJoined = _T("domainjoined");
const TCHAR* const kDownloaded = _T("downloaded");
const TCHAR* const kDownloader = _T("downloader");
const TCHAR* const kDownloadTime = _T("download_time_ms");
const TCHAR* const kElapsedDays = _T("elapsed_days");
const TCHAR* const kElapsedSeconds = _T("elapsed_seconds");
const TCHAR* const kErrorCode = _T("errorcode");
const TCHAR* const kErrorUrl = _T("errorurl");
const TCHAR* const kEvent = _T("event");
const TCHAR* const kEventResult = _T("eventresult");
const TCHAR* const kEventType = _T("eventtype");
const TCHAR* const kExperiments = _T("experiments");
const TCHAR* const kExtraCode1 = _T("extracode1");
const TCHAR* const kHash = _T("hash");
const TCHAR* const kHashSha256 = _T("hash_sha256");
const TCHAR* const kIndex = _T("index");
const TCHAR* const kInstallationId = _T("iid");
const TCHAR* const kInstallDate = _T("installdate");
const TCHAR* const kInstalledAgeDays = _T("installage");
const TCHAR* const kInstallSource = _T("installsource");
const TCHAR* const kInstallTime = _T("install_time_ms");
const TCHAR* const kIsBundled = _T("is_bundled");
const TCHAR* const kIsMachine = _T("ismachine");
const TCHAR* const kLang = _T("lang");
const TCHAR* const kMinOSVersion = _T("min_os_version");
const TCHAR* const kName = _T("name");
const TCHAR* const kNextVersion = _T("nextversion");
const TCHAR* const kOriginURL = _T("originurl");
const TCHAR* const kParameter = _T("parameter");
const TCHAR* const kPeriodOverrideSec = _T("periodoverridesec");
const TCHAR* const kPhysMemory = _T("physmemory");
const TCHAR* const kPingFreshness = _T("ping_freshness");
const TCHAR* const kPlatform = _T("platform");
const TCHAR* const kProtocol = _T("protocol");
const TCHAR* const kRequestId = _T("requestid");
const TCHAR* const kRequired = _T("required");
const TCHAR* const kRollbackAllowed = _T("rollback_allowed");
const TCHAR* const kRun = _T("run");
const TCHAR* const kServicePack = _T("sp");
const TCHAR* const kSessionId = _T("sessionid");
const TCHAR* const kShellVersion = _T("shell_version");
const TCHAR* const kSignature = _T("signature");
const TCHAR* const kSize = _T("size");
const TCHAR* const kSourceUrlIndex = _T("source_url_index");
const TCHAR* const kSse = _T("sse");
const TCHAR* const kSse2 = _T("sse2");
const TCHAR* const kSse3 = _T("sse3");
const TCHAR* const kSsse3 = _T("ssse3");
const TCHAR* const kSse41 = _T("sse41");
const TCHAR* const kSse42 = _T("sse42");
const TCHAR* const kStateCancelled = _T("state_cancelled");
const TCHAR* const kStatus = _T("status");
const TCHAR* const kSuccessAction = _T("onsuccess");
const TCHAR* const kSuccessUrl = _T("successurl");
const TCHAR* const kTargetChannel = _T("release_channel");
const TCHAR* const kTargetVersionPrefix = _T("targetversionprefix");
const TCHAR* const kTestSource = _T("testsource");
const TCHAR* const kTerminateAllBrowsers = _T("terminateallbrowsers");
const TCHAR* const kTimeSinceDownloadStart = _T("time_since_download_start_ms");
const TCHAR* const kTimeSinceUpdateAvailable =
    _T("time_since_update_available_ms");
const TCHAR* const kTotal = _T("total");
const TCHAR* const kTTToken = _T("tttoken");
const TCHAR* const kUpdateCheckTime= _T("update_check_time_ms");
const TCHAR* const kUpdateDisabled = _T("updatedisabled");
const TCHAR* const kUpdater = _T("updater");
const TCHAR* const kUpdaterVersion = _T("updaterversion");
const TCHAR* const kUrl = _T("url");
const TCHAR* const kUserId = _T("userid");
const TCHAR* const kVersion = _T("version");
const TCHAR* const kXmlns = _T("xmlns");

const TCHAR* const kAppDefinedPrefix = _T("_");

}  // namespace attribute

namespace value {

const TCHAR* const kBits = _T("bits");
const TCHAR* const kCacheable = _T("cacheable");
const TCHAR* const kClientRegulated = _T("cr");
const TCHAR* const kDirect = _T("direct");
const TCHAR* const kFalse = _T("false");
const TCHAR* const kInstall = _T("install");
const TCHAR* const kInstallData = _T("install");
const TCHAR* const kPostinstall = _T("postinstall");
const TCHAR* const kPreinstall = _T("preinstall");
const TCHAR* const kRequestType = _T("UpdateRequest");
const TCHAR* const kStatusError = _T("error");
const TCHAR* const kSuccessActionDefault = _T("default");
const TCHAR* const kSuccessActionExitSilently = _T("exitsilently");
const TCHAR* const kSuccessActionExitSilentlyOnLaunchCmd =
    _T("exitsilentlyonlaunchcmd");
const TCHAR* const kTrue = _T("true");
const TCHAR* const kUid = _T("uid");
const TCHAR* const kUpdater = _T("Omaha");
const TCHAR* const kUntrusted = _T("untrusted");
const TCHAR* const kUpdate = _T("update");
const TCHAR* const kVersion3 = _T("3.0");

}  // namespace value

}  // namespace xml

}  // namespace omaha
