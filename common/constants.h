// Copyright 2003-2009 Google Inc.
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

#ifndef OMAHA_COMMON_CONSTANTS_H__
#define OMAHA_COMMON_CONSTANTS_H__

#include <windows.h>
#include <tchar.h>

namespace omaha {

// Exe loading address for Windows executables
// (also the HMODULE for an exe app)
#define kExeLoadingAddress      0x00400000

// application name (for debugging messages)
// kAppName == "Google Update"
#define kAppName          _T(PUBLISHER_NAME_ANSI) _T(" ") _T(PRODUCT_NAME_ANSI)

// Product name to report
// kProductNameToReport == "Google Update"
#define kProductNameToReport    kAppName

// prefix for any created files
#define kFilePrefix  _T("GoogleUpdate")

// prefix for any Win32 objects (mutexes, events, etc.)
#define kLockPrefix   _T("_GOOGLE_UPDATE_")

//
// Omaha's app ID
//
#define GOOPDATE_APP_ID _T("{430FD4D0-B729-4F61-AA34-91526481799D}")
const TCHAR* const kGoogleUpdateAppId = GOOPDATE_APP_ID;
const GUID kGoopdateGuid = {0x430FD4D0, 0xB729, 0x4F61,
                            {0xAA, 0x34, 0x91, 0x52, 0x64, 0x81, 0x79, 0x9D}};

//
// Directory names
//
#define OFFLINE_DIR_NAME _T("Offline")

#define OMAHA_REL_GOOGLE_DIR _T("Google")
#define OMAHA_REL_CRASH_DIR OMAHA_REL_GOOGLE_DIR _T("\\CrashReports")
#define OMAHA_REL_GOOPDATE_INSTALL_DIR OMAHA_REL_GOOGLE_DIR _T("\\Update")
#define OMAHA_REL_LOG_DIR OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\Log")
#define OMAHA_REL_OFFLINE_STORAGE_DIR \
    OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\") OFFLINE_DIR_NAME
#define OMAHA_REL_DOWNLOAD_STORAGE_DIR \
    OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\Download")
#define OMAHA_REL_TEMP_DOWNLOAD_DIR  _T("\\Temp")
#define OMAHA_REL_MANIEFST_DIR OMAHA_REL_GOOPDATE_INSTALL_DIR _T("\\Manifest")
#define OMAHA_REL_INITIAL_MANIFEST_DIR OMAHA_REL_MANIEFST_DIR _T("\\Initial")
#define OMAHA_REL_MANIFEST_INPROGRESS_DIR \
    OMAHA_REL_MANIEFST_DIR _T("\\InProgress")

//
// Registry keys and values
//
#define MACHINE_KEY_NAME _T("HKLM")
#define MACHINE_KEY MACHINE_KEY_NAME _T("\\")
#define USER_KEY_NAME _T("HKCU")
#define USER_KEY USER_KEY_NAME _T("\\")
#define USERS_KEY _T("HKU\\")
#define GOOGLE_MAIN_KEY _T("Software\\Google\\")
#define GOOPDATE_MAIN_KEY GOOGLE_MAIN_KEY _T("Update\\")
#define GOOPDATE_REG_RELATIVE_CLIENTS GOOPDATE_MAIN_KEY _T("Clients\\")
#define GOOPDATE_REG_RELATIVE_CLIENT_STATE GOOPDATE_MAIN_KEY _T("ClientState\\")
#define GOOPDATE_REG_RELATIVE_CLIENT_STATE_MEDIUM \
    GOOPDATE_MAIN_KEY _T("ClientStateMedium\\")
#define GOOGLE_POLICIES_MAIN_KEY _T("Software\\Policies\\Google\\")
#define GOOPDATE_POLICIES_RELATIVE GOOGLE_POLICIES_MAIN_KEY _T("Update\\")

#define USER_REG_GOOGLE USER_KEY GOOGLE_MAIN_KEY
#define USER_REG_UPDATE USER_KEY GOOPDATE_MAIN_KEY
#define USER_REG_CLIENTS USER_KEY GOOPDATE_REG_RELATIVE_CLIENTS
#define USER_REG_CLIENTS_GOOPDATE  USER_REG_CLIENTS GOOPDATE_APP_ID
#define USER_REG_CLIENT_STATE USER_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE
#define USER_REG_CLIENT_STATE_GOOPDATE USER_REG_CLIENT_STATE GOOPDATE_APP_ID

#define MACHINE_REG_GOOGLE MACHINE_KEY GOOGLE_MAIN_KEY
#define MACHINE_REG_UPDATE MACHINE_KEY GOOPDATE_MAIN_KEY
#define MACHINE_REG_CLIENTS MACHINE_KEY GOOPDATE_REG_RELATIVE_CLIENTS
#define MACHINE_REG_CLIENTS_GOOPDATE  MACHINE_REG_CLIENTS GOOPDATE_APP_ID
#define MACHINE_REG_CLIENT_STATE MACHINE_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE
#define MACHINE_REG_CLIENT_STATE_GOOPDATE \
    MACHINE_REG_CLIENT_STATE GOOPDATE_APP_ID
#define MACHINE_REG_CLIENT_STATE_MEDIUM \
    MACHINE_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE_MEDIUM

#define USER_REG_VISTA_LOW_INTEGRITY_HKCU \
    _T("Software\\Microsoft\\Internet Explorer\\") \
    _T("InternetRegistry\\REGISTRY\\USER")

#define MACHINE_REG_UPDATE_DEV MACHINE_KEY GOOGLE_MAIN_KEY _T("UpdateDev\\")

//
// Registry values under MACHINE_REG_UPDATE_DEV allow customization of the
// default behavior. The overrides apply for both user and machine
// instances of omaha.
//
// The values below can only be overriden in debug builds.
const TCHAR* const kRegValueNameOverInstall    = _T("OverInstall");
const TCHAR* const kRegValueNameUrl            = _T("url");
const TCHAR* const kRegValueNamePingUrl        = _T("PingUrl");
const TCHAR* const kRegValueNameWebPluginUrl   = _T("WebPluginUrl");

// The values below can be overriden in both debug and opt builds.
const TCHAR* const kRegValueTestSource         = _T("TestSource");
const TCHAR* const kRegValueAuCheckPeriodMs    = _T("AuCheckPeriodMs");
const TCHAR* const kRegValueCrCheckPeriodMs    = _T("CrCheckPeriodMs");
const TCHAR* const kRegValueProxyHost          = _T("ProxyHost");
const TCHAR* const kRegValueProxyPort          = _T("ProxyPort");

// The values below can be overriden in unofficial builds.
const TCHAR* const kRegValueNameWindowsInstalling = _T("WindowsInstalling");

// Allows Google Update to log events in the Windows Event Log. This is
// a DWORD value 0: Log nothing, 1: Log warnings and errors, 2: Log everything.
const TCHAR* const kRegValueEventLogLevel      = _T("LogEventLevel");

enum LogEventLevel {
  LOG_EVENT_LEVEL_NONE           = 0,
  LOG_EVENT_LEVEL_WARN_AND_ERROR = 1,
  LOG_EVENT_LEVEL_ALL            = 2
};

// How often Google Update checks the server for updates.
const TCHAR* const kRegValueLastCheckPeriodSec = _T("LastCheckPeriodSec");

// Uses the production or the test cup keys. Once the client has negotiated
// CUP credentials {sk, c} and it has saved them under the corresponding
// Google\Update\network key then the client does not need any of the CUP keys.
// To force the client to use test or production keys, {sk, c} credentials must
// be cleared too.
const TCHAR* const kRegValueCupKeys            = _T("TestKeys");

// Allow a custom host pattern to be specified. For example,
// "^https?://some_test_server\.google\.com/". For other examples, look at
// site_lock_pattern_strings in oneclick_worker.cc. The detailed regular
// expression syntax is documented in the MSDN documentation for the CAtlRegExp
// class: http://msdn.microsoft.com/en-us/library/k3zs4axe(VS.80).aspx
const TCHAR* const kRegValueOneClickHostPattern = _T("OneClickHostPattern");

// Disables the Code Red check.
const TCHAR* const kRegValueNoCodeRedCheck     = _T("NoCrCheck");

// Enables sending usage stats always if the value is present.
const TCHAR* const kRegValueForceUsageStats    = _T("UsageStats");

// Enables monitoring the 'LastChecked' value for testing purposes. When
// the 'LastChecked' is deleted, the core starts a worker process to do an
// update check. This value must be set before the core process starts.
const TCHAR* const kRegValueMonitorLastChecked = _T("MonitorLastChecked");

// The test_source value to use for googleupdate instances that have
// customizations, and hence should be discarded from metrics on the dashboard.
const TCHAR* const kRegValueTestSourceAuto     = _T("auto");

// The network configuration to override the network detection.
// The corresponding value must have the following format:
// wpad=[false|true];script=script_url;proxy=host:port
const TCHAR* const kRegValueNetConfig          = _T("NetConfig");

const int kMaxAppNameLength = 512;

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

// The value that is used in the run key.
const TCHAR* const kRunValueName = _T("Google Update");

// Default brand code value when one is not specified.
// This has been specifically assigned to Omaha.
const TCHAR* const kDefaultGoogleUpdateBrandCode = _T("GGLS");

// The platform named used for Windows.
const TCHAR* const kPlatformWin = _T("win");

// The following are response strings returned by the server.
// They must exactly match the strings returned by the server.
const TCHAR* const kResponseStatusOkValue = _T("ok");
const TCHAR* const kResponseStatusNoUpdate = _T("noupdate");
const TCHAR* const kResponseStatusRestrictedExportCountry = _T("restricted");
const TCHAR* const kResponseStatusOsNotSupported = _T("error-osnotsupported");
const TCHAR* const kResponseStatusUnKnownApplication =
    _T("error-UnKnownApplication");
const TCHAR* const kResponseStatusInternalError = _T("error-internal");

const TCHAR* const kLocalSystemSid = _T("S-1-5-18");

// Defines LastCheckPeriodSec: the time interval between actual server
// update checks. Opt builds have an aggressive check for updates every 5 hours.
// This introduces some time shift for computers connected all the time, for
// example, the update checks occur at: 12, 17, 22, 3, 8, 13, 18, etc...
const int kMsPerSec           = 1000;
const int kMinPerHour         = 60;
const int kSecondsPerHour     = 60 * 60;

// Since time computation for LastChecked is done in seconds, sometimes it
// can miss an update check, depending on arithmetic truncations.
// Adjust down the LastCheckPeriod so that the update worker does not miss it.
//
// Almost 5 hours for production users and almost hourly for Googlers.
const int kLastCheckPeriodSec         = 5 * 59 * kMinPerHour;
const int kLastCheckPeriodGooglerSec  = 1 * 59 * kMinPerHour;


const int kMinLastCheckPeriodSec = 60;  // 60 seconds minimum.

// Defines the time interval when the core is kicking off silent workers. When
// there is nothing to do, a worker does not take more than 200 ms to run.
// Googlers are supposed to update at least once every hour. Therefore, start
// workers every 30 minutes.
const int kAUCheckPeriodMs        = 60 * 60 * 1000;   // Hourly.
const int kAUCheckPeriodGooglerMs = 30 * 60 * 1000;   // 30 minutes.

// Avoids starting workers too soon. This helps reduce disk thrashing at
// boot or logon, as well as needlessly starting a worker after setting up.
const int kUpdateTimerStartupDelayMinMs = 5 * 60 * 1000;   // 5 minutes.

// Maximum amount of time to wait before starting an update worker.
const int kUpdateTimerStartupDelayMaxMs = 15 * 60 * 1000;   // 15 minutes.

// Minimum AU check interval is lowered to 3 seconds to speed up test
// automation.
const int kMinAUCheckPeriodMs = 1000 * 3;   // 3 seconds.

// The Code Red check frequency.
const int kCodeRedCheckPeriodMs     = 24 * 60 * 60 * 1000;    // 24 hours.
const int kMinCodeRedCheckPeriodMs  = 60 * 1000;              // 1 minute.

// The minimum amount of time after a /oem install that Google Update is
// considered to be in OEM mode regardless of audit mode.
const int kMinOemModeMs = 72 * 60 * 60 * 1000;  // 72 hours.

// The amount of time to wait for the setup lock before giving up.
const int kSetupLockWaitMs = 1000;  // 1 second.

// The amount of time to wait for other instances to shutdown before giving up.
// There is no UI during this time, so the interactive value cannot be too long.
// Even the silent value cannot be too long because Setup Lock is being held.
const int kSetupShutdownWaitMsInteractiveNoUi = 2000;   // 2 seconds.
const int kSetupShutdownWaitMsSilent          = 15000;  // 15 seconds.

// Time to wait for the busy MSI when uninstalling. If MSI is found busy,
// Omaha won't uninstall. The timeout should be high enough so that it allows
// a normal application uninstall to finish and trigger an Omaha uninstall
// when needed.
const int kWaitForMSIExecuteMs                = 5 * 60000;  // 5 minutes.

// Name of the language key-value pair inside the version resource.
const TCHAR* const kLanguageVersionName = _T("LanguageId");

// These must be in sync with the WiX files.
const TCHAR* const kHelperInstallerName = _T("GoogleUpdateHelper.msi");
const TCHAR* const kHelperInstallerProductGuid =
    _T("{A92DAB39-4E2C-4304-9AB6-BC44E68B55E2}");
const TCHAR* const kHelperPatchName = _T("GoogleUpdateHelperPatch.msp");
const TCHAR* const kHelperPatchGuid =
    _T("{E0D0D2C9-5836-4023-AB1D-54EC3B90AD03}");

// Group name to use to read/write INI files for custom crash client info.
const TCHAR* const kCustomClientInfoGroup = _T("ClientCustomData");

}  // namespace omaha

#endif  // OMAHA_COMMON_CONSTANTS_H__

