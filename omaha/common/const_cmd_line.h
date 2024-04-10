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

// Externally initiated modes. These modes are invoked by metainstallers.

// The "install" switch indicates installing Omaha and the app.
const TCHAR* const kCmdLineInstall = _T("install");

// The "installelevated" switch indicates installing after elevating.
const TCHAR* const kCmdLineInstallElevated = _T("installelevated");

// The "update" switch indicates an Omaha self-update.
const TCHAR* const kCmdLineUpdate = _T("update");

// The "recover" switch indicates Omaha is to be repaired due to a
// Code Red scenario.
const TCHAR* const kCmdLineRecover = _T("recover");

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

// The "ig" switch was used in Omaha 2 to indicate a worker to finish installing
// Omaha and perform an interactive install of an application.
// ig = Install Google Update.
const TCHAR* const kCmdLineLegacyFinishGoogleUpdateInstall = _T("ig");

// The "ua" switch indicates a worker to perform an update check for all
// applications.
// ua = Update Applications.
const TCHAR* const kCmdLineUpdateApps = _T("ua");

// The "cr" switch indicates that the worker has been invoked to perform a Code
// Red check.
const TCHAR* const kCmdLineCodeRedCheck = _T("cr");

// The "registerproduct" switch will register a product GUID in Clients and
// install only Omaha.
const TCHAR* const kCmdLineRegisterProduct = _T("registerproduct");

// The "unregisterproduct" switch will unregister a product GUID from Clients.
const TCHAR* const kCmdLineUnregisterProduct = _T("unregisterproduct");

//
// Minor modes
//

// The "svc" switch indicates that Omaha runs as a service that only accepts
// calls from high integrity COM callers.
const TCHAR* const kCmdLineService = _T("svc");

// The "medsvc" switch indicates that Omaha runs as a service that accepts
// calls from medium integrity COM callers.
const TCHAR* const kCmdLineMediumService = _T("medsvc");

// The "regsvc" switch is used to register the service. Only used by unit
// tests at the moment.
const TCHAR* const kCmdLineRegisterService = _T("regsvc");

// The "unregsvc" switch is used to unregister the service. Only used by
// unit tests at the moment.
const TCHAR* const kCmdLineUnregisterService = _T("unregsvc");

// The "/comsvc" switch indicates that has the service is being invoked via COM.
// This switch will be passed in via ServiceParameters.
const TCHAR* const kCmdLineServiceComServer = _T("/comsvc");

// The "regserver" switch indicates that Omaha should do its Windows
// service COM server registration including coclasses and its APPID.
const TCHAR* const kCmdRegServer = _T("regserver");

// The "unregserver" switch indicates that Omaha should undo its
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

// Activates broker mode. Broker mode is intended to facilitate communication
// between low-integrity clients and the high-integrity Omaha DCOM service.
const TCHAR* const kCmdLineComBroker = _T("/broker");

// Activates OnDemand mode.
const TCHAR* const kCmdLineOnDemand = _T("/ondemand");

// The "uninstall" switch indicates that Omaha should uninstall if appropriate.
const TCHAR* const kCmdLineUninstall = _T("uninstall");

// The "healthcheck" switch allows the installed version of Omaha to indicate
// whether it is installed and functioning correctly by returning S_OK.
const TCHAR* const kCmdLineHealthCheck = _T("healthcheck");

// The "registermsihelper" switch allows installing the MSI Helper in a
// separate process, isolating any crashes in MSI registration from affecting
// the rest of the codebase.
const TCHAR* const kCmdLineRegisterMsiHelper = _T("registermsihelper");

// The "ping" switch makes Omaha send a ping with the specified string. The
// string is expected to be web safe base64 encoded and it will be decoded
// before sending it to the server.
const TCHAR* const kCmdLinePing = _T("ping");

//
// Non-product modes
// These are used for debug, testing, etc.
//

// The "crash" switch indicates that Omaha should crash upon startup.
// This option is used to test the crash reporting system.
const TCHAR* const kCmdLineCrash = _T("crash");

//
// Parameters for other modes
//

// The "silent" switch specifies that normally interactive modes should run
// silently.
const TCHAR* const kCmdLineSilent = _T("silent");

// The "alwayslaunchcmd" switch specifies that the launch command is to be
// executed unconditionally, even for silent modes.
const TCHAR* const kCmdLineAlwaysLaunchCmd = _T("alwayslaunchcmd");

const TCHAR* const kCmdLineLegacyOfflineInstall = _T("offlineinstall");
const TCHAR* const kCmdLineOfflineDir = _T("offlinedir");

// The "oem" switch specifies that this is an OEM install in Sysprep mode in an
// OEM factory.
const TCHAR* const kCmdLineOem = _T("oem");

// The "eularequired" switch specifies that a EULA must be accepted before
// checking for updates or pinging.
const TCHAR* const kCmdLineEulaRequired = _T("eularequired");

// The "enterprise" switch indicates that no pings should be sent during the
// install.
const TCHAR* const kCmdLineEnterprise = _T("enterprise");

// The "machine" switch specifies to repair machine Omaha when specified with
// "recover". Also used to tell the setup phase 2 worker to do a machine install
// when doing a recover setup.
const TCHAR* const kCmdLineMachine = _T("machine");

// The "i" switch indicates that the crash has happend in an
// interactive process which has a UI up. The switch is an option for
// the "report" switch.
const TCHAR* const kCmdLineInteractive = _T("i");

// The "sessionid" switch indicates that a specific session ID should be used by
// this instance of Omaha for network requests.  This switch is an option for
// the "install", "handoff", and "update" modes.
const TCHAR* const kCmdLineSessionId = _T("sessionid");

// The "installsource" switch that is used to pass the source of installation
// for ping tracking.  For example:  "/installsource taggedmi".
const TCHAR* const kCmdLineInstallSource = _T("installsource");

// "installsource" values generated internally by Omaha. The server code needs
// to be updated when these values change or new values are defined.
const TCHAR* const kCmdLineInstallSource_TaggedMetainstaller = _T("taggedmi");
const TCHAR* const kCmdLineInstallSource_ClickOnce = _T("clickonce");
const TCHAR* const kCmdLineInstallSource_Offline = _T("offline");
const TCHAR* const kCmdLineInstallSource_InstallDefault = _T("otherinstallcmd");
const TCHAR* const kCmdLineInstallSource_Scheduler = _T("scheduler");
const TCHAR* const kCmdLineInstallSource_Core = _T("core");
const TCHAR* const kCmdLineInstallSource_OnDemandUpdate = _T("ondemandupdate");
const TCHAR* const kCmdLineInstallSource_OnDemandCheckForUpdate =
    _T("ondemandcheckforupdate");
const TCHAR* const kCmdLineInstallSource_OnDemandUA = _T("ondemandua");
const TCHAR* const kCmdLineInstallSource_SelfUpdate = _T("selfupdate");
const TCHAR* const kCmdLineInstallSource_Update3Web = _T("update3web");
const TCHAR* const kCmdLineInstallSource_Update3Web_NewApps =
    _T("update3web-newapps");
const TCHAR* const kCmdLineInstallSource_Update3Web_OnDemand =
    _T("update3web-ondemand");
const TCHAR* const kCmdLineInstallSource_Update3Web_Components =
    _T("update3web-components");
const TCHAR* const kCmdLineInstallSource_ChromeRecovery = _T("chromerecovery");
// Update the server side code when values are added above this comment line.

// This install source is not used as a command line argument but internally
// created by Omaha.
const TCHAR* const kInstallSource_Uninstall = _T("uninstall");

// The "no_mi_tag" switch tells a tagged metainstaller to not append its tag
// to the command line when launching.  This switch MUST be the last tag in
// the command line, and will be removed by the MI prior to launching the
// constant shell.
const TCHAR* const kCmdLineNoMetainstallerTag = _T("nomitag");

// The /installerdata=file.dat switch is passed to an installer if an
// installdataindex is specified in the tag or if installerdata is passed in via
// /appargs. The corresponding installerdata is written to file.dat with an UTF8
// encoding as well as a UTF8 Signature.
const TCHAR* const kCmdLineInstallerData = _T("/installerdata=");

//
// "Extra" arguments provided in the metainstaller tag.
//

// "bundlename" extra argument is the name of the bundle being installed. If not
// specified, the first app's appname is used.
const TCHAR* const kExtraArgBundleName = _T("bundlename");

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

// "experiments" extra argument is a set of experiment labels used to track
// installs that are included in experiments.  Use "experiments" for
// per-app arguments; use "omahaexperiments" for Omaha-specific labels.
const TCHAR* const kExtraArgExperimentLabels = _T("experiments");
const TCHAR* const kExtraArgOmahaExperimentLabels = _T("omahaexperiments");

// "referral" extra argument is a referral ID used for tracking referrals.
const TCHAR* const kExtraArgReferralId = _T("referral");

// "ap" extra argument tells Omaha to set the ap value in the registry.
const TCHAR* const kExtraArgAdditionalParameters = _T("ap");

// "tttoken" extra argument tells Omaha to set the TT value in the registry.
const TCHAR* const kExtraArgTTToken = _T("tttoken");

// "browser" extra argument tells Omaha which browser to restart on
// successful install.
const TCHAR* const kExtraArgBrowserType = _T("browser");

// Runtime Mode:
// * "runtime" extra argument of "true" tells Omaha to only install itself,
// staying on the system without any associated application for at least 24
// hours.
// This is used to expose our COM API to a process that will install
// applications via that API after the meta-installer exits.
//
// * "runtime" extra argument of "persist" tells Omaha to only install itself,
// staying persisted indefinitely on the system without any associated
// application.
// This is used to allow Enterprises to Push application installs to individual
// machines using Policy.
//
// * "runtime" extra argument of "false" tells Omaha that it can uninstall
// itself if there are no registered apps.
const TCHAR* const kExtraArgRuntimeMode = _T("runtime");

#if defined(HAS_DEVICE_MANAGEMENT)

// "etoken" extra argument gives Omaha an enrollment token. Omaha will use this
// for per-machine installs to register the machine with Google's device
// management server.
const TCHAR* const kExtraArgEnrollmentToken = _T("etoken");

#endif  // defined(HAS_DEVICE_MANAGEMENT)

// The list of arguments that are needed for a meta-installer, to
// indicate which application is being installed. These are stamped
// inside the meta-installer binary.
const TCHAR* const kExtraArgAppGuid = _T("appguid");
const TCHAR* const kExtraArgAppName = _T("appname");
const TCHAR* const kExtraArgNeedsAdmin = _T("needsadmin");
const TCHAR* const kExtraArgInstallDataIndex = _T("installdataindex");
const TCHAR* const kExtraArgUntrustedData = _T("untrusteddata");

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

