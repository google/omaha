// Copyright 2003-2010 Google Inc.
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
// Constants used by Omaha project

#ifndef OMAHA_BASE_CONSTANTS_H_
#define OMAHA_BASE_CONSTANTS_H_

#include <windows.h>
#include <tchar.h>

namespace omaha {

//
// Begin vendor-specific constants.
//
// When adding values here or using these values for a constant, add a test for
// the value to common\omaha_customization_unittest.cc.
// NOTE: The following are defined in main.scons:
//  + FULL_COMPANY_NAME_ANSI
//  + SHORT_COMPANY_NAME_ANSI
//  + PRODUCT_NAME_ANSI
//  + COMPANY_DOMAIN_ANSI
//  + MAIN_DLL_BASE_NAME_ANSI
//
// *_IDENTIFIER assume that SHORT_COMPANY_NAME_ANSI and PRODUCT_NAME_ANSI are
// legal identifiers (i.e. one word).

// TODO(omaha3): Move vendor-specific constants to a single file to make use
// of open source code by other vendors simpler.
// TODO(omaha3): Only use very specific const variables outside that file. Do
// not build values in other source files by concatenating preprocessor defines.

// Full company name.
// FULL_COMPANY_NAME == "Google LLC"
#define FULL_COMPANY_NAME _T(FULL_COMPANY_NAME_ANSI)
const TCHAR* const kFullCompanyName = FULL_COMPANY_NAME;

// Short company name (for use in messages and to combine with product name).
// Does not include "Inc." and similar formal parts of the company name.
// If the company name can be abbreviated, it will be here.
// SHORT_COMPANY_NAME == "Google"
#define SHORT_COMPANY_NAME _T(SHORT_COMPANY_NAME_ANSI)
const TCHAR* const kShortCompanyName = SHORT_COMPANY_NAME;

// The company name to use in file and registry paths.
// PATH_COMPANY_NAME == "Google"
#define PATH_COMPANY_NAME _T(PATH_COMPANY_NAME_ANSI)
const TCHAR* const kPathCompanyName = PATH_COMPANY_NAME;

// Product name.
// PRODUCT_NAME == "Update"
#define PRODUCT_NAME _T(PRODUCT_NAME_ANSI)

// Company domain name base. Used for MIME type names.
// COMPANY_DOMAIN_BASE_ANSI = "google"
#define COMPANY_DOMAIN_BASE _T(COMPANY_DOMAIN_BASE_ANSI)

// Company domain name base. Used for addresses.
// COMPANY_DOMAIN_ANSI = "google.com"
#define COMPANY_DOMAIN _T(COMPANY_DOMAIN_ANSI)

// Company's internal network DNS domain. Used for detecting internal users.
// If the internal network uses a different domain than the public-facing
// COMPANY_DOMAIN, this will need to be changed.
// kCompanyInternalDnsName = ".google.com"
const TCHAR* const kCompanyInternalDnsName = _T(".") COMPANY_DOMAIN;

// Company's internal network NetBIOS name. Used for detecting internal users.
// If the internal network uses a different domain than the public-facing
// COMPANY_DOMAIN_BASE, this will need to be changed.
// kCompanyInternalLanGroupName = "google"
const TCHAR* const kCompanyInternalLanGroupName = COMPANY_DOMAIN_BASE;

// The base name of the main executable. Everything except the ".exe".
// Most files start with the main .exe's base name.
// MAIN_EXE_BASE_NAME = "GoogleUpdate"
#define MAIN_EXE_BASE_NAME  _T(MAIN_EXE_BASE_NAME_ANSI)

// Base name of the main DLL.
// MAIN_DLL_BASE_NAME = "goopdate"
#define MAIN_DLL_BASE_NAME _T(MAIN_DLL_BASE_NAME_ANSI)

// Application name.
// Use the localized IDS_PRODUCT_DISPLAY_NAME or the formatted
// IDS_INSTALLER_DISPLAY_NAME instead when string is displayed to user.
// kAppName == "Google Update"
// TODO(omaha): Maybe rename all of these kOmahaAppName.
const TCHAR* const kAppName = _T(OMAHA_APP_NAME_ANSI);

#define COMPANY_NAME_IDENTIFIER SHORT_COMPANY_NAME
#define PRODUCT_NAME_IDENTIFIER PRODUCT_NAME
#define APP_NAME_IDENTIFIER SHORT_COMPANY_NAME PRODUCT_NAME

// Prefix for any Win32 objects (mutexes, events, etc.).
// TODO(omaha): This is used by several files in base/. Maybe we can use a
// similar prefix to avoid conflicts with some values in const_object_names.h.
// Consider moving this constant to const_object_names.h.
#define kLockPrefix \
    _T("_") COMPANY_NAME_IDENTIFIER _T("_") PRODUCT_NAME_IDENTIFIER _T("_")

const TCHAR* const kOmahaShellFileName         = MAIN_EXE_BASE_NAME _T(".exe");
const TCHAR* const kOmahaDllName               = MAIN_DLL_BASE_NAME _T(".dll");
const TCHAR* const kOmahaResourceDllNameFormat =
    MAIN_DLL_BASE_NAME _T("res_%s.dll");
const TCHAR* const kOmahaBrokerFileName        =
    MAIN_EXE_BASE_NAME _T("Broker.exe");
const TCHAR* const kOmahaOnDemandFileName      =
    MAIN_EXE_BASE_NAME _T("OnDemand.exe");
const TCHAR* const kCrashHandlerFileName   = CRASH_HANDLER_NAME _T(".exe");
const TCHAR* const kCrashHandler64FileName = CRASH_HANDLER_NAME _T("64.exe");
const TCHAR* const kOmahaMetainstallerFileName =
    MAIN_EXE_BASE_NAME _T("Setup.exe");
const TCHAR* const kOmahaCOMRegisterShell64    =
    MAIN_EXE_BASE_NAME _T("ComRegisterShell64.exe");
const TCHAR* const kOmahaCoreFileName  = MAIN_EXE_BASE_NAME _T("Core.exe");
const TCHAR* const kPSFileNameMachine  = _T("psmachine.dll");
const TCHAR* const kPSFileNameMachine64= _T("psmachine_64.dll");
const TCHAR* const kPSFileNameUser     = _T("psuser.dll");
const TCHAR* const kPSFileNameUser64   = _T("psuser_64.dll");

// TODO(omaha): Replace the following literal in clickonce\build.scons.
// '%s/GoogleUpdateSetup.exe'

const TCHAR* const kLegacyHelperInstallerGuid =
    _T("{A92DAB39-4E2C-4304-9AB6-BC44E68B55E2}");

// The value that is used in the run key.
const TCHAR* const kRunValueName = kAppName;

// TODO(omaha): Try to use the above constants in the IDL file help strings.
// TODO(omaha): Consider moving uuid's from the IDL files to here too.
// TODO(omaha): Use these values for the registry maps definitions, and progid
// uses, such as ondemand.h.

// The prefix for temporary filenames that Omaha creates.
const TCHAR* const kTemporaryFilenamePrefix = _T("gup");

//
// Omaha's app ID
//
// TODO(omaha): Rename all of these "Omaha".
#define GOOPDATE_APP_ID _T("{430FD4D0-B729-4F61-AA34-91526481799D}")
const TCHAR* const kGoogleUpdateAppId = GOOPDATE_APP_ID;
const GUID kGoopdateGuid = {0x430FD4D0, 0xB729, 0x4F61,
                            {0xAA, 0x34, 0x91, 0x52, 0x64, 0x81, 0x79, 0x9D}};

// Chrome AppIDs
#define CHROME_APP_ID _T("{8A69D345-D564-463C-AFF1-A69D9E530F96}")
const TCHAR* const kChromeAppId = CHROME_APP_ID;

#if defined(HAS_DEVICE_MANAGEMENT) && defined(HAS_LEGACY_DM_CLIENT)

//
// Cloud-based device management
//

// The name of the legacy client application.
#define LEGACY_DM_CLIENT_APP _T("Chrome")

#endif  // defined(HAS_DEVICE_MANAGEMENT) && defined(HAS_LEGACY_DM_CLIENT)

//
// Directory names
//
#define OFFLINE_DIR_NAME          _T("Offline")
#define DOWNLOAD_DIR_NAME         _T("Download")
#define INSTALL_WORKING_DIR_NAME  _T("Install")

// Directories relative to \Google
#define OMAHA_REL_COMPANY_DIR PATH_COMPANY_NAME
#define OMAHA_REL_CRASH_DIR OMAHA_REL_COMPANY_DIR _T("\\CrashReports")
#define OMAHA_REL_POLICY_RESPONSES_DIR OMAHA_REL_COMPANY_DIR _T("\\Policies")
#define OMAHA_REL_TEMP_DIR OMAHA_REL_COMPANY_DIR _T("\\Temp")

// Directories relative to \Google\Update
#define OMAHA_REL_GOOPDATE_INSTALL_DIR \
    OMAHA_REL_COMPANY_DIR _T("\\") PRODUCT_NAME
#define OMAHA_REL_LOG_DIR OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\Log")
#define OMAHA_REL_OFFLINE_STORAGE_DIR \
    OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\") OFFLINE_DIR_NAME
#define OMAHA_REL_DOWNLOAD_STORAGE_DIR \
    OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\") DOWNLOAD_DIR_NAME
#define OMAHA_REL_INSTALL_WORKING_DIR \
    OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\") INSTALL_WORKING_DIR_NAME

// This directory is relative to the user profile app data local.
#define LOCAL_APPDATA_REL_TEMP_DIR _T("\\Temp")

//
// Registry keys and values
//
#define MACHINE_KEY_NAME _T("HKLM")
#define MACHINE_KEY MACHINE_KEY_NAME _T("\\")
#define MACHINE_KEY_64 MACHINE_KEY_NAME _T("[64]\\")
#define USER_KEY_NAME _T("HKCU")
#define USER_KEY USER_KEY_NAME _T("\\")
#define USERS_KEY _T("HKU\\")
#define COMPANY_MAIN_KEY _T("Software\\") PATH_COMPANY_NAME _T("\\")
#define GOOPDATE_MAIN_KEY COMPANY_MAIN_KEY PRODUCT_NAME _T("\\")
#define GOOPDATE_REG_RELATIVE_CLIENTS GOOPDATE_MAIN_KEY _T("Clients\\")
#define GOOPDATE_REG_RELATIVE_CLIENT_STATE GOOPDATE_MAIN_KEY _T("ClientState\\")
#define GOOPDATE_REG_RELATIVE_CLIENT_STATE_MEDIUM \
    GOOPDATE_MAIN_KEY _T("ClientStateMedium\\")
#define COMPANY_POLICIES_MAIN_KEY \
    _T("Software\\Policies\\") PATH_COMPANY_NAME _T("\\")
#define GOOPDATE_POLICIES_RELATIVE COMPANY_POLICIES_MAIN_KEY \
    PRODUCT_NAME _T("\\")
#define CLOUD_MANAGEMENT_POLICIES_RELATIVE COMPANY_POLICIES_MAIN_KEY \
    _T("CloudManagement\\")

#define USER_LOCAL USER_KEY _T("Software\\Classes\\Local Settings\\")
#define USER_LOCAL_REG_UPDATE USER_LOCAL GOOPDATE_MAIN_KEY

#define USER_REG_GOOGLE USER_KEY COMPANY_MAIN_KEY
#define USER_REG_UPDATE USER_KEY GOOPDATE_MAIN_KEY
#define USER_REG_CLIENTS USER_KEY GOOPDATE_REG_RELATIVE_CLIENTS
#define USER_REG_CLIENTS_GOOPDATE  USER_REG_CLIENTS GOOPDATE_APP_ID
#define USER_REG_CLIENT_STATE USER_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE
#define USER_REG_CLIENT_STATE_GOOPDATE USER_REG_CLIENT_STATE GOOPDATE_APP_ID

#define MACHINE_REG_GOOGLE MACHINE_KEY COMPANY_MAIN_KEY
#define MACHINE_REG_UPDATE MACHINE_KEY GOOPDATE_MAIN_KEY
#define MACHINE_REG_CLIENTS MACHINE_KEY GOOPDATE_REG_RELATIVE_CLIENTS
#define MACHINE_REG_CLIENTS_GOOPDATE  MACHINE_REG_CLIENTS GOOPDATE_APP_ID
#define MACHINE_REG_CLIENT_STATE MACHINE_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE
#define MACHINE_REG_CLIENT_STATE_GOOPDATE \
    MACHINE_REG_CLIENT_STATE GOOPDATE_APP_ID
#define MACHINE_REG_CLIENT_STATE_MEDIUM \
    MACHINE_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE_MEDIUM

#define REG_UPDATE_DEV COMPANY_MAIN_KEY PRODUCT_NAME _T("Dev\\")

// Expands to HKEY_LOCAL_MACHINE\SOFTWARE\Google\UpdateDev
#define MACHINE_REG_UPDATE_DEV MACHINE_KEY REG_UPDATE_DEV

//
// Minimum compatible shell version.
// Shell versions equal to or newer than the following version are compatible
// with the current version of goopdate.dll and do not need be replaced:
// 1.3.26.1.
const ULONGLONG kCompatibleMinimumOlderShellVersion = 0x00010003001A0001;

//
// End vendor-specific constants.
//

//
// Registry values under MACHINE_REG_UPDATE_DEV allow customization of the
// default behavior. The overrides apply for both user and machine
// instances of omaha.
//
// The default ACLs for MACHINE_REG_UPDATE_DEV only allow privileged users
// to make changes to the registry keys and values.
// Modifying these settings could affect the overall security of Omaha. This is
// not a security hole since the user has to be an admin first but it is
// something to be used cautiously.
//
// The values below can only be overriden in debug builds.
const TCHAR* const kRegValueNameOverInstall    = _T("OverInstall");
const TCHAR* const kRegValueNameCrashIfSpecificError
    = _T("CrashIfSpecificError");

// The values below can be overriden in both debug and opt builds. Code Red url
// can be overriden but its value is defined in the Code Red module. The value
// name is "CodeRedUrl".
const TCHAR* const kRegValueNameUrl                 = _T("url");
const TCHAR* const kRegValueNamePingUrl             = _T("PingUrl");
const TCHAR* const kRegValueNameCrashReportUrl      = _T("CrashReportUrl");
const TCHAR* const kRegValueNameGetMoreInfoUrl      = _T("MoreInfoUrl");
const TCHAR* const kRegValueNameUsageStatsReportUrl = _T("UsageStatsReportUrl");
const TCHAR* const kRegValueNameAppLogoUrl          = _T("AppLogoUrl");
const TCHAR* const kRegValueTestSource              = _T("TestSource");
const TCHAR* const kRegValueAuCheckPeriodMs         = _T("AuCheckPeriodMs");
const TCHAR* const kRegValueCrCheckPeriodMs         = _T("CrCheckPeriodMs");
const TCHAR* const kRegValueAutoUpdateJitterMs      = _T("AutoUpdateJitterMs");
const TCHAR* const kRegValueProxyHost               = _T("ProxyHost");
const TCHAR* const kRegValueProxyPort               = _T("ProxyPort");
const TCHAR* const kRegValueMID                     = _T("mid");

const TCHAR* const kRegValueDisablePayloadAuthenticodeVerification =
    _T("DisablePayloadAuthenticodeVerification");

// File extensions that can be verified with an Authenticode signature.
const TCHAR* const kAuthenticodeVerifiableExtensions[] = {
  _T("exe"),  _T("msi"),    _T("dll"),  _T("sys"),  _T("cab"), _T("ocx"),
  _T("xpi"),  _T("xap"),    _T("cat"),  _T("jar"),  _T("ps1"), _T("psm1"),
  _T("psd1"), _T("ps1xml"), _T("psc1"), _T("acm "), _T("ax"),  _T("cpl"),
  _T("drv"),  _T("efi"),    _T("mui"),  _T("scr"),  _T("sys"), _T("tsp")
};

#if defined(HAS_DEVICE_MANAGEMENT)
const TCHAR* const kRegValueNameDeviceManagementUrl = _T("DeviceManagementUrl");
#endif

// The values below can be overriden in unofficial builds.
const TCHAR* const kRegValueNameWindowsInstalling = _T("WindowsInstalling");

// Allows Omaha to log to %ALLUSERSPROFILE%\Google\Update\Log\GoogleUpdate.log.
const TCHAR* const kRegValueIsEnabledLogToFile    = _T("IsEnabledLogToFile");

// Allows Omaha to log events in the Windows Event Log. This is
// a DWORD value 0: Log nothing, 1: Log warnings and errors, 2: Log everything.
const TCHAR* const kRegValueEventLogLevel      = _T("LogEventLevel");

enum LogEventLevel {
  LOG_EVENT_LEVEL_NONE           = 0,
  LOG_EVENT_LEVEL_WARN_AND_ERROR = 1,
  LOG_EVENT_LEVEL_ALL            = 2
};

// How often Omaha checks the server for updates.
const TCHAR* const kRegValueLastCheckPeriodSec = _T("LastCheckPeriodSec");

// Uses the production or the test cup keys.
const TCHAR* const kRegValueCupKeys            = _T("TestKeys");

// Disables executable verification for application commands.
const TCHAR* const kRegValueSkipCommandVerification =
    _T("NoAppCommandVerification");

// Disables the Code Red check.
const TCHAR* const kRegValueNoCodeRedCheck     = _T("NoCrCheck");

// Enables sending usage stats always if the value is present.
const TCHAR* const kRegValueForceUsageStats    = _T("UsageStats");

// Override to allow/disallow the machine to appear as part of a domain:
// * not present; domain membership is determined via ::NetGetJoinInformation.
// * present and set to TRUE; the machine acts as it were part of a domain.
// * present and set to FALSE; the machine acts as it were not part of a domain.
const TCHAR* const kRegValueIsEnrolledToDomain = _T("IsEnrolledToDomain");

// Enables crash uploads if the value is 1. Crashes can be uploaded only if
// certain conditions are met. This value allows overriding of the default
// crash uploading behavior.
const TCHAR* const kRegValueAlwaysAllowCrashUploads =
    _T("AlwaysAllowCrashUploads");

// Overrides the default maximum number of crash uploads we make per day.
const TCHAR* const kRegValueMaxCrashUploadsPerDay =
    _T("MaxCrashUploadsPerDay");

const TCHAR* const kRegValueDisableUpdateAppsHourlyJitter =
    _T("DisableUpdateAppsHourlyJitter");

// Enables monitoring the 'LastChecked' value for testing purposes. When
// the 'LastChecked' is deleted, the core starts a worker process to do an
// update check. This value must be set before the core process starts.
const TCHAR* const kRegValueMonitorLastChecked = _T("MonitorLastChecked");

// The test_source value to use for Omaha instances that have
// customizations, and hence should be discarded from metrics.
const TCHAR* const kRegValueTestSourceAuto     = _T("auto");

// The network configuration to override the network detection.
// The corresponding value must have the following format:
// wpad=[false|true];script=script_url;proxy=host:port
const TCHAR* const kRegValueNetConfig          = _T("NetConfig");

// Setting this value makes the client create the IGoogleUpdate3 COM server
// in-proc.
const TCHAR* const kRegValueUseInProcCOMServer = _T("UseInProcCOMServer");

// The maximum length of application and bundle names.
const int kMaxNameLength = 512;

// Specifies whether a tristate item has a value and if so what the value is.
enum Tristate {
  TRISTATE_FALSE,
  TRISTATE_TRUE,
  TRISTATE_NONE
};

// Number of periods to use when abbreviating URLs
#define kAbbreviationPeriodLength 3

// The Unicode "Byte Order Marker" character.  This is the native
// encoding, i.e. after conversion from UTF-8 or whatever.
const wchar_t kUnicodeBom = 0xFEFF;

// Using these constants will make ATL load the
// typelib directly from a DLL instead of looking up typelib
// registration in registry.
const DWORD kMajorTypeLibVersion = 0xFFFF;
const DWORD kMinorTypeLibVersion = 0xFFFF;

// Brand id length
const int kBrandIdLength = 4;

// Country code length according to ISO 3166-3.
const int kCountryCodeMaxLength = 5;

// Language code length.
const int kLangMaxLength = 10;

// When not specified, the country code defaults to USA
const TCHAR* const kDefaultCountryCode = _T("us");

// the max length of the extra info we can store inside the install stubs.
const int kExtraMaxLength = 64 * 1024;  // 64 KB

// Default brand code value when one is not specified.
// This has been specifically assigned to Omaha.
const TCHAR* const kDefaultGoogleUpdateBrandCode = _T("GGLS");

// The platform named used for Windows.
const TCHAR* const kPlatformWin = _T("win");

const TCHAR* const kLocalSystemSid = _T("S-1-5-18");

// Time-related constants for defining durations.
const int kMsPerSec           = 1000;
const int kSecPerMin          = 60;
const int kMinPerHour         = 60;
const int kSecondsPerHour     = 60 * 60;
const int kSecondsPerDay      = 24 * kSecondsPerHour;

// Defines LastCheckPeriodSec: the time interval between actual server
// update checks. Opt builds have an aggressive check for updates every 5 hours.
// This introduces some time shift for computers connected all the time, for
// example, the update checks occur at: 12, 17, 22, 3, 8, 13, 18, etc...
//
// Almost 5 hours for production users and almost hourly for internal users. A
// constant jitter is introduced here so that when the scheduler fires at the
// hourly mark, the timer interval between updates will have a high likelihood
// of being satisfied.
const int kLastCheckJitterSec = 5 * kSecPerMin;
const int kLastCheckPeriodSec = 5 * kSecondsPerHour - kLastCheckJitterSec;
const int kLastCheckPeriodInternalUserSec = kLastCheckPeriodSec / 5;

const int kMinLastCheckPeriodSec = 60;  // 60 seconds minimum.

// Defines the time interval when the core is kicking off silent workers. When
// there is nothing to do, a worker does not take more than 200 ms to run.
// Internal users are supposed to update at least once every hour. Therefore,
// start workers every 30 minutes.
const int kAUCheckPeriodMs             = 60 * 60 * 1000;  // Hourly.
const int kAUCheckPeriodInternalUserMs = 30 * 60 * 1000;  // 30 minutes.

// Avoids starting workers too soon. This helps reduce disk thrashing at
// boot or logon, as well as needlessly starting a worker after setting up.
const int kUpdateTimerStartupDelayMinMs = 5 * 60 * 1000;   // 5 minutes.

// Maximum amount of time to wait before starting an update worker.
const int kUpdateTimerStartupDelayMaxMs = 15 * 60 * 1000;   // 15 minutes.

// Minimum AU check interval is lowered to 3 seconds to speed up test
// automation.
const int kMinAUCheckPeriodMs = 1000 * 3;   // 3 seconds.

// The maximum wait time between Core runs. If the Core has not run for more
// than this interval, it is time to run the Core again.
const int kMaxWaitBetweenCoreRunsMs = 25 * 60 * 60 * 1000;    // 25 hours.

// The Code Red check frequency.
const int kCodeRedCheckPeriodMs     = 24 * 60 * 60 * 1000;    // 24 hours.
const int kMinCodeRedCheckPeriodMs  = 60 * 1000;              // 1 minute.

// The minimum amount of time after a /oem install that Omaha is considered to
// be in OEM mode regardless of audit mode.
const int kMinOemModeSec = 72 * 60 * 60;  // 72 hours.

// The amount of time to wait for the setup lock before giving up.
const int kSetupLockWaitMs = 1000;  // 1 second.

// The amount of time to wait for other instances to shutdown before giving up.
const int kSetupInstallShutdownWaitMs = 45 * 1000;      // 45 seconds.
const int kSetupUpdateShutdownWaitMs  = 3 * 60 * 1000;  // 3 minutes.

// Time to wait for the busy MSI when uninstalling. If MSI is found busy,
// Omaha won't uninstall. The timeout should be high enough so that it allows
// a normal application uninstall to finish and trigger an Omaha uninstall
// when needed.
const int kWaitForMSIExecuteMs                = 5 * 60000;  // 5 minutes.

// The Scheduled Tasks are initially set to start 5 minutes from the
// installation time.
#define kScheduledTaskDelayStartNs (5 * kMinsTo100ns);

// The Scheduled Tasks are set to run once daily.
const int kScheduledTaskIntervalDays = 1;

// The Scheduled Task will be run daily at 24 hour intervals.
const DWORD kScheduledTaskDurationMinutes = 24 * 60;

// Name of the language key-value pair inside the version resource.
const TCHAR* const kLanguageVersionName = _T("LanguageId");

// Group name to use to read/write INI files for custom crash client info.
const TCHAR* const kCustomClientInfoGroup = _T("ClientCustomData");

// ***                                            ***
// *** UI constants.                              ***
// ***                                            ***
const COLORREF kTextColor = RGB(0x29, 0x29, 0x29);
const COLORREF kBkColor = RGB(0XFB, 0XFB, 0XFB);

const COLORREF kCaptionForegroundColor = RGB(0x00, 0x00, 0x00);
const COLORREF kCaptionBkHover = RGB(0xE9, 0xE9, 0xE9);
const COLORREF kCaptionFrameColor = RGB(0xC1, 0xC1, 0xC1);

const COLORREF kProgressOuterFrameLight = RGB(0x3c, 0x86, 0xf0);
const COLORREF kProgressOuterFrameDark = RGB(0x23, 0x6d, 0xd6);
const COLORREF kProgressInnerFrameLight = RGB(0x6e, 0xc2, 0xfe);
const COLORREF kProgressInnerFrameDark = RGB(0x44, 0x90, 0xfc);
const COLORREF kProgressBarLightColor = RGB(0x4d, 0xa4, 0xfd);
const COLORREF kProgressBarDarkColor = RGB(0x40, 0x86, 0xfd);
const COLORREF kProgressEmptyFillColor = RGB(0xb6, 0xb6, 0xb6);
const COLORREF kProgressEmptyFrameColor = RGB(0xad, 0xad, 0xad);
const COLORREF kProgressShadowLightColor = RGB(0xbd, 0xbd, 0xbd);
const COLORREF kProgressShadowDarkColor = RGB(0xa5, 0xa5, 0xa5);
const COLORREF kProgressLeftHighlightColor = RGB(0xbd, 0xbd, 0xbd);

// ***                                                       ***
// *** Custom HTTP request headers sent by the Omaha Client. ***
// ***                                                       ***
const TCHAR kHeaderUserAgent[]           = _T("User-Agent");

// The HRESULT and HTTP status code updated by the prior
// NetworkRequestImpl::DoSendHttpRequest() call.
const TCHAR kHeaderXLastHR[]             = _T("X-Last-HR");
const TCHAR kHeaderXLastHTTPStatusCode[] = _T("X-Last-HTTP-Status-Code");

// The "mid" value if it exists in HKLM\SOFTWARE\Google\UpdateDev.
const TCHAR kHeaderXMID[]                = _T("X-MID");

// The 407 retry count in the case of authenticated proxies.
const TCHAR kHeaderXProxyRetryCount[]    = _T("X-Proxy-Retry-Count");

// Indicates that we had to prompt the user for proxy credentials.
const TCHAR kHeaderXProxyManualAuth[]    = _T("X-Proxy-Manual-Auth");

// The age in seconds between the current time and when a ping was first
// persisted.
const TCHAR kHeaderXRequestAge[]         = _T("X-RequestAge");

// The current retry count defined by the outermost
// NetworkRequestImpl::DoSendWithRetries() call.
const TCHAR kHeaderXRetryCount[]         = _T("X-Retry-Count");

// Count of DoSendHttpRequest() calls.
const TCHAR kHeaderXHTTPAttempts[]       = _T("X-HTTP-Attempts");

// If the user id has been reset because the MAC changed, X-Old-UID contains the
// previous UID.
const TCHAR kHeaderXOldUserId[]          = _T("X-Old-UID");

// The client sends a X-Goog-Update-Interactivity header to indicate whether
// the current request is foreground or background.
// A value of "fg" ("foreground") indicates foreground install or on-demand
// updates. "bg" ("background") indicates silent update traffic.
const TCHAR kHeaderXInteractive[] = _T("X-Goog-Update-Interactivity");

// The client sends a X-Goog-Update-AppId header to indicate the apps
// associated with the request. When updating multiple apps, the client
// specifies a comma-separated list of app ids.
const TCHAR kHeaderXAppId[]       = _T("X-Goog-Update-AppId");

// The client sends a X-Goog-Update-Updater header to indicate the identity of
// the updater. This is the "updater" version string also present in the
// request. In the case of Omaha, prepend "Omaha-" to the version string.
const TCHAR kHeaderXUpdater[]     = _T("X-Goog-Update-Updater");

// ***                                                                      ***
// *** Custom HTTP request headers that may be in an Omaha server response. ***
// ***                                                                      ***

// The time in seconds since the start of the day on the server's clock.  (If
// the XML request is successfully validated and contains a <daystart> element,
// consider it more authoritative than this value.)
const TCHAR kHeaderXDaystart[]           = _T("X-Daystart");

// Number of days since datum on the server's clock. (If the XML is
// successfully validated and contains an |elapsed_days| attribute in
// <daystart> element, consider it more authoritative than this value.)
const TCHAR kHeaderXDaynum[]             = _T("X-Daynum");

// Alternate ETag custom header. In case the real ETag header is removed before
// it reaches the client, this custom header may prove more resilient to content
// filtering.
const TCHAR kHeaderXETag[]               = _T("X-Cup-Server-Proof");

// Weak ETags header values are prefixed with "W/".
const TCHAR kWeakETagPrefix[]            = _T("W/");

// The server uses the optional X-Retry-After header to indicate that the
// current request should not be attempted again. Any response received along
// with the X-Retry-After header should be interpreted as it would have been
// without the X-Retry-After header.
//
// In addition to the presence of the header, the value of the header is
// used as a signal for when to do future update checks, but only when the
// response is over https. Values over http are not trusted and are ignored.
//
// The value of the header is the number of seconds to wait before trying to do
// a subsequent update check. The uppper bound for the number of seconds to wait
// before trying to do a subsequent update check is capped at 24 hours.
const TCHAR kHeaderXRetryAfter[]         = _T("X-Retry-After");

}  // namespace omaha

#endif  // OMAHA_BASE_CONSTANTS_H_
