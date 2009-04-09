// Copyright 2007-2009 Google Inc.
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
// Constants used as command line arguments.

#ifndef OMAHA_COMMON_CONST_CMD_LINE_H__
#define OMAHA_COMMON_CONST_CMD_LINE_H__

namespace omaha {

//
// Externally initiated modes.
// These modes are invoked by or on metainstallers or by the OneClick plugin  .
//

// The "install" switch indicates installing Google Update and the app.
const TCHAR* const kCmdLineInstall = _T("install");

// The "installelevated" switch indicates installing after elevating.
const TCHAR* const kCmdLineInstallElevated = _T("installelevated");

// The "update" switch indicates a self-update Google Update.
const TCHAR* const kCmdLineUpdate = _T("update");

// The "recover" switch indicates Google Update is to be repaired due to a
// Code Red scenario.
const TCHAR* const kCmdLineRecover = _T("recover");

// The "pi" switch indicates that this came from a webplugin.
// Requires two subarguments "siteurl" and "{args}" where
// siteurl is the base URL where the plugin ran from and {args}
// are the args to pass on once validation is complete.
const TCHAR* const kCmdLineWebPlugin = _T("pi");

//
// Main operating modes
//

// The "c" option indicates a core process.
const TCHAR* const kCmdLineCore = _T("c");

// Specifies to not kick off the crash handler from the Core.
const TCHAR* const kCmdLineNoCrashHandler = _T("nocrashserver");

// The "crashhandler" option indicates a crash handler process.
const TCHAR* const kCmdLineCrashHandler = _T("crashhandler");

// Types of "Workers"

// The "handoff" switch indicates a worker to perform an interactive install of
// an application.
const TCHAR* const kCmdLineAppHandoffInstall = _T("handoff");

// The "ig" switch indicates a worker to finish installing Google Update and
// perform an interactive install of an application.
// ig = Install Google Update.
const TCHAR* const kCmdLineFinishGoogleUpdateInstall = _T("ig");

// The "ua" switch indicates a worker to perform an update check for all
// applications.
// ua = Update Applications.
const TCHAR* const kCmdLineUpdateApps = _T("ua");

// The "ug" switch indicates a worker to finish updating Google Update.
// No application update checks are performed.
// ug = Update Google Update.
const TCHAR* const kCmdLineFinishGoogleUpdateUpdate = _T("ug");

// The "cr" switch indicates that the worker has been invoked to perform a Code
// Red check.
const TCHAR* const kCmdLineCodeRedCheck = _T("cr");

// The "registerproduct" switch will register a product GUID in Clients and
// install only goopdate.
const TCHAR* const kCmdLineRegisterProduct = _T("registerproduct");

// The "unregisterproduct" switch will unregister a product GUID from Clients.
const TCHAR* const kCmdLineUnregisterProduct = _T("unregisterproduct");

//
// Minor modes
//

// The "svc" switch indicates that goopdate runs as a service.
const TCHAR* const kCmdLineService = _T("svc");

// The "regsvc" switch is used to register the service. Only used by unit
// tests at the moment.
const TCHAR* const kCmdLineRegisterService = _T("regsvc");

// The "unregsvc" switch is used to unregister the service. Only used by
// unit tests at the moment.
const TCHAR* const kCmdLineUnregisterService = _T("unregsvc");

// The "/comsvc" switch indicates that has the service is being invoked via COM.
// This switch will be passed in via ServiceParameters.
const TCHAR* const kCmdLineServiceComServer = _T("/comsvc");

// The "regserver" switch indicates that goopdate should do its Windows
// service COM server registration including coclasses and its APPID.
const TCHAR* const kCmdRegServer = _T("regserver");

// The "unregserver" switch indicates that goopdate should undo its
// COM registration.
const TCHAR* const kCmdUnregServer = _T("unregserver");

// The "report" switch makes Omaha upload the crash report.
const TCHAR* const kCmdLineReport = _T("report");

// The "custom_info_filename" switch specifies the file that contains custom
// crash info.
const TCHAR* const kCmdLineCustomInfoFileName = _T("custom_info_filename");

// The "Embedding" switch indicates that the worker has been invoked to launch
// the browser. The -Embedding switch is automatically added by COM when
// launching the COM server.
const TCHAR* const kCmdLineComServer = _T("Embedding");
const TCHAR* const kCmdLineComServerDash = _T("-Embedding");

//
// Legacy support modes
//

// The legacy "UI" switch supports hand-off machine installs from Omaha 1.
const TCHAR* const kCmdLineLegacyUi = _T("ui");

// The legacy "lang" switch supports /lang for the /ui command.  No one else
// should be sending /lang as a command line switch to goopdate.
const TCHAR* const kCmdLineLegacyLang = _T("lang");

// The "uiuser" switch is used to support hand off of of Omaha 1 user manifests.
const TCHAR* const kCmdLineLegacyUserManifest = _T("uiuser");

// The "extra" switch was used to provide a string containing additional
// arguments. Extra args are now passed as an argument to the respective switch.
const TCHAR* const kCmdLineLegacyExtra = _T("extra");

// Passed to the new instance when launching it elevated.
const TCHAR* const kCmdLineLegacyVistaInstall = _T("installelevated");

//
// Non-product modes
// These are used for debug, testing, etc.
//

// Run network diagnostics.
const TCHAR* const kCmdLineNetDiags = _T("netdiags");

// The "crash" switch indicates that goopdate should crash upon startup.
// This option is used to test the crash reporting system.
const TCHAR* const kCmdLineCrash = _T("crash");

//
// Parameters for other modes
//

// The "silent" switch specifies that normally interactive modes should run
// silently.
const TCHAR* const kCmdLineSilent = _T("silent");

const TCHAR* const kCmdLineOfflineInstall = _T("offlineinstall");

// The "oem" switch specifies that this is an OEM install in Sysprep mode in an
// OEM factory.
const TCHAR* const kCmdLineOem = _T("oem");

// The "eularequired" switch specifies that a EULA must be accepted before
// checking for updates or pinging.
const TCHAR* const kCmdLineEulaRequired = _T("eularequired");

// The "machine" switch specifies to repair machine Omaha when specified with
// "recover". Also used to tell the setup phase 2 worker to do a machine install
// when doing a recover setup.
const TCHAR* const kCmdLineMachine = _T("machine");

// The "uninstall" switch is an option to /ua to tell it to skip the update
// check and proceed with an uninstall.
const TCHAR* const kCmdLineUninstall = _T("uninstall");

// The "i" switch indicates that the crash has happend in an
// interactive process which has a UI up. The switch is an option for
// the "report" switch.
const TCHAR* const kCmdLineInteractive = _T("i");

// The "installsource" switch that is used to pass the source of installation
// for ping tracking.  For example:  "/installsource OneClick".
const TCHAR* const kCmdLineInstallSource = _T("installsource");

// This is a valid value for installsource that means it's a OneClick install.
const TCHAR* const kCmdLineInstallSource_OneClick = _T("oneclick");

const TCHAR* const kCmdLineInstallSource_OnDemandUpdate = _T("ondemandupdate");
const TCHAR* const kCmdLineInstallSource_OnDemandCheckForUpdate =
    _T("ondemandcheckforupdate");

const TCHAR* const kCmdLineInstallSource_ClickOnce = _T("clickonce");

const TCHAR* const kCmdLineInstallSource_Offline = _T("offline");

//
// "Extra" arguments provided in the metainstaller tag.
//

// "lang" extra argument tells Omaha the language of the product the user is
// installing.
const TCHAR* const kExtraArgLanguage = _T("lang");

// "usagestats" extra argument tells Omaha the user has agreed to provide
// usage stats, crashreports etc.
const TCHAR* const kExtraArgUsageStats = _T("usagestats");

// "iid" extra argument is a unique value for this installation session.
// It can be used to follow the progress from the website to installation
// completion.
const TCHAR* const kExtraArgInstallationId = _T("iid");

// "brand" extra argument is the Brand Code used for branding.
// If a brand value already exists on the system, it is ignored.
// This value is used to set the initial brand for Omaha and the client app.
const TCHAR* const kExtraArgBrandCode = _T("brand");

// "client" extra argument is the Client ID used for branding.
// If a client value already exists on the system, it is ignored.
// This value is used to set the initial client for Omaha and the client app.
const TCHAR* const kExtraArgClientId = _T("client");

// "referral" extra argument is a referral ID used for tracking referrals.
const TCHAR* const kExtraArgReferralId = _T("referral");

// '" extra argument tells Omaha to set the ap value in the registry.
const TCHAR* const kExtraArgAdditionalParameters = _T("ap");

// "tt_token" extra argument tells Omaha to set the TT value in the registry.
const TCHAR* const kExtraArgTTToken = _T("tttoken");


// "browser" extra argument tells Omaha which browser to restart on
// successful install.
const TCHAR* const kExtraArgBrowserType = _T("browser");

// The list of arguments that are needed for a meta-installer, to
// indicate which application is being installed. These are stamped
// inside the meta-installer binary.
const TCHAR* const kExtraArgAppGuid = _T("appguid");
const TCHAR* const kExtraArgAppName = _T("appname");
const TCHAR* const kExtraArgNeedsAdmin = _T("needsadmin");
const TCHAR* const kExtraArgInstallDataIndex = _T("installdataindex");

// App arguments are arguments explicitly passed on the command line. They are
// formatted similar to the regular extra args. For example:
//     /appargs "appguid={GUID}&installerdata=BlahData"
// Unlike the regular extra args, they are not embedded in the executable.
const TCHAR* const kCmdLineAppArgs = _T("appargs");

// This switch allows extra data to be communicated to the application
// installer. The extra data needs to be URL-encoded. The data will be decoded
// and written to the file, that is then passed in the command line to the
// application installer in the form "/installerdata=blah.dat". One per
// application.
const TCHAR* const kExtraArgInstallerData = _T("installerdata");

//
// Parsing characters
//

const TCHAR* const kExtraArgsSeparators        = _T("&");
const TCHAR* const kDisallowedCharsInExtraArgs = _T("/");
const TCHAR        kNameValueSeparatorChar     = _T('=');

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_CMD_LINE_H__

