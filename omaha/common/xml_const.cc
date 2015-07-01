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
const TCHAR* const kManifest = _T("manifest");
const TCHAR* const kOs = _T("os");
const TCHAR* const kPackage = _T("package");
const TCHAR* const kPackages = _T("packages");

// didrun element. The element is named "ping" for legacy reasons.
const TCHAR* const kPing = _T("ping");
const TCHAR* const kRequest = _T("request");
const TCHAR* const kResponse = _T("response");
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
const TCHAR* const kBrandCode = _T("brand");
const TCHAR* const kBrowserType = _T("browser");
const TCHAR* const kClientId = _T("client");
const TCHAR* const kCodebase = _T("codebase");
const TCHAR* const kCountry = _T("country");
const TCHAR* const kDaysSinceLastActivePing = _T("a");
const TCHAR* const kDaysSinceLastRollCall = _T("r");
const TCHAR* const kDownloadTime = _T("download_time_ms");
const TCHAR* const kElapsedSeconds = _T("elapsed_seconds");
const TCHAR* const kErrorCode = _T("errorcode");
const TCHAR* const kEvent = _T("event");
const TCHAR* const kEventResult = _T("eventresult");
const TCHAR* const kEventType = _T("eventtype");
const TCHAR* const kErrorUrl = _T("errorurl");
const TCHAR* const kExperiments = _T("experiments");
const TCHAR* const kExtraCode1 = _T("extracode1");
const TCHAR* const kHash = _T("hash");
const TCHAR* const kIndex = _T("index");
const TCHAR* const kInstalledAgeDays = _T("installage");
const TCHAR* const kIsMachine = _T("ismachine");
const TCHAR* const kInstallationId = _T("iid");
const TCHAR* const kInstallSource = _T("installsource");
const TCHAR* const kOriginURL = _T("originurl");
const TCHAR* const kLang = _T("lang");
const TCHAR* const kName = _T("name");
const TCHAR* const kNextVersion = _T("nextversion");
const TCHAR* const kParameter = _T("parameter");
const TCHAR* const kPeriodOverrideSec = _T("periodoverridesec");
const TCHAR* const kPlatform = _T("platform");
const TCHAR* const kProtocol = _T("protocol");
const TCHAR* const kRequestId = _T("requestid");
const TCHAR* const kRequired = _T("required");
const TCHAR* const kRun = _T("run");
const TCHAR* const kServicePack = _T("sp");
const TCHAR* const kSessionId = _T("sessionid");
const TCHAR* const kSignature = _T("signature");
const TCHAR* const kSize = _T("size");
const TCHAR* const kStatus = _T("status");
const TCHAR* const kSuccessAction = _T("onsuccess");
const TCHAR* const kSuccessUrl = _T("successurl");
const TCHAR* const kTestSource = _T("testsource");
const TCHAR* const kTerminateAllBrowsers = _T("terminateallbrowsers");
const TCHAR* const kTTToken = _T("tttoken");
const TCHAR* const kUpdateDisabled = _T("updatedisabled");
const TCHAR* const kUserId = _T("userid");
const TCHAR* const kVersion = _T("version");
const TCHAR* const kXmlns = _T("xmlns");

}  // namespace attribute

namespace value {

const TCHAR* const kArchAmd64 = _T("x64");
const TCHAR* const kArchIntel = _T("x86");
const TCHAR* const kArchUnknown = _T("unknown");
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
const TCHAR* const kUpdate = _T("update");
const TCHAR* const kVersion3 = _T("3.0");

}  // namespace value

}  // namespace xml

}  // namespace omaha
