// Copyright 2007-2010 Google Inc.
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
// Goopdate constants - Reduces dependencies on goopdate.h for names that
// are needed outside Goopdate.
//
// TODO(omaha): it appears that the string constants below are not
// optimized out the program image even if not used. Figure out why they still
// show up in the tiny shell in optimized builds.

#ifndef OMAHA_COMMON_CONST_GOOPDATE_H_
#define OMAHA_COMMON_CONST_GOOPDATE_H_

#include <tchar.h>
#include "omaha/base/constants.h"

// TODO(omaha3): Many of these values are specific to the COM server and a few
// may apply to the client. Move most of them to the goopdate directory.

namespace omaha {

enum ActiveStates {
  ACTIVE_NOTRUN = 0,
  ACTIVE_RUN,
  ACTIVE_UNKNOWN
};

// Specifies what Omaha should do on successful installation.
enum SuccessfulInstallAction {
  SUCCESS_ACTION_DEFAULT = 0,
  SUCCESS_ACTION_EXIT_SILENTLY,
  SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
};

// Specifies the type of result the app installer returned.
enum InstallerResultType {
  INSTALLER_RESULT_UNKNOWN,
  INSTALLER_RESULT_SUCCESS,
  INSTALLER_RESULT_ERROR_MSI,
  INSTALLER_RESULT_ERROR_SYSTEM,
  INSTALLER_RESULT_ERROR_OTHER,
};

// The enumeration of the events. Some of the events are used in IPC,
// these are named events, the rest are not named. Of the named events there
// are two types the global and the local events.
// Global Events:
// The events which are global are indicated with the Global suffix in the
// enum. These events have the Global prefix attached to the event name and
// are used for IPC across terminal server sessions, these events are used
// as barriers, i.e. all waiting threads are release on these events.
// Local Events:
// The local counter parts of these events have the Local prefix attached
// to their names, and dont have any suffix in the enum names. The local events
// are used to release only one thread, this works as we know that there is
// only one user goopdate process in a user session and one machine goopdate.
// The local events also have the user sid added to the event name. This is to
// work around a bug in win2K and on XP where in case of TS, the event names
// will collide, inspite of the name changes.
enum GoopdateEvents {
  EVENT_INVALID = -1,
  EVENT_KILL_MESSAGE_LOOP = 0,
  EVENT_UPDATE_TIMER,
  EVENT_NEW_MANIFEST,          // Used in IPC.
  EVENT_QUIET_MODE,            // Used in IPC.
  EVENT_LEGACY_QUIET_MODE,     // Only used to shut down pre-i18n goopdates.
  EVENT_CODE_RED_TIMER,
};

// Represents the values that are used by the application to indicate its
// requirement for admin.
enum NeedsAdmin {
  NEEDS_ADMIN_NO = 0,   // The application will install per user.
  NEEDS_ADMIN_YES,      // The application will install machine-wide.
  NEEDS_ADMIN_PREFERS,  // The application will install machine-wide if
                        // permissions allow, else will install per-user.
};

// Represents the values that are used by the application to indicate the
// Runtime Mode.
enum RuntimeMode {
  RUNTIME_MODE_NOT_SET = -1,
  RUNTIME_MODE_FALSE = 0,     // Omaha will uninstall if no registered apps.
  RUNTIME_MODE_TRUE = 1,      // Omaha will remain around for 24 hours.
  RUNTIME_MODE_PERSIST = 2,   // Omaha will remain around indefinitely.
};

// Using extern or intern linkage for these strings yields the same code size
// for the executable DLL.

// The string representation of the NeedsAdmin enum as specified in the tag or
// command line. This is eventually parsed into the needs_admin member of
// CommandLineAppArgs.
const TCHAR* const kNeedsAdminNo = _T("&needsadmin=false");
const TCHAR* const kNeedsAdminYes = _T("&needsadmin=true");
const TCHAR* const kNeedsAdminPrefers = _T("&needsadmin=prefers");

// Environment variable inherited by an installer child process that indicates
// whether GoogleUpdate is running as user or machine.
const TCHAR* const kEnvVariableIsMachine = APP_NAME_IDENTIFIER _T("IsMachine");
const TCHAR* const kEnvVariableUntrustedData = APP_NAME_IDENTIFIER
                                               _T("UntrustedData");
// Maximum allowed length of untrusted data (unescaped).
const int kUntrustedDataMaxLength = 4096;

// Registry values read from the Clients key for transmitting custom install
// errors, messages, etc. On an update, the InstallerXXX values are renamed to
// LastInstallerXXX values. The LastInstallerXXX values remain around until the
// next update.
const TCHAR* const kRegValueInstallerProgress    = _T("InstallerProgress");
const TCHAR* const kRegValueInstallerResult      = _T("InstallerResult");
const TCHAR* const kRegValueInstallerError       = _T("InstallerError");
const TCHAR* const kRegValueInstallerExtraCode1  = _T("InstallerExtraCode1");
const TCHAR* const kRegValueInstallerResultUIString =
    _T("InstallerResultUIString");
const TCHAR* const kRegValueInstallerSuccessLaunchCmdLine =
    _T("InstallerSuccessLaunchCmdLine");

const TCHAR* const kRegValueLastInstallerResult =
    _T("LastInstallerResult");
const TCHAR* const kRegValueLastInstallerError =
    _T("LastInstallerError");
const TCHAR* const kRegValueLastInstallerExtraCode1 =
    _T("LastInstallerExtraCode1");
const TCHAR* const kRegValueLastInstallerResultUIString =
    _T("LastInstallerResultUIString");
const TCHAR* const kRegValueLastInstallerSuccessLaunchCmdLine =
    _T("LastInstallerSuccessLaunchCmdLine");


// Registry subkey in an app's Clients key that contains its commands.
const TCHAR* const kCommandsRegKeyName       = _T("Commands");

// Registry values read from the Clients commands key.
const TCHAR* const kRegValueAutoRunOnOSUpgrade = _T("AutoRunOnOSUpgrade");
const TCHAR* const kRegValueCaptureOutput      = _T("CaptureOutput");
const TCHAR* const kRegValueCommandLine        = _T("CommandLine");
const TCHAR* const kRegValueReportingId        = _T("ReportingId");
const TCHAR* const kRegValueRunAsUser          = _T("RunAsUser");
const TCHAR* const kRegValueSendsPings         = _T("SendsPings");
const TCHAR* const kRegValueWebAccessible      = _T("WebAccessible");

// Registry value in an app's Clients key that contains a registration update
// hook CLSID.
const TCHAR* const kRegValueUpdateHookClsid  = _T("RegistrationUpdateHook");

// Registry values read from the Clients key and stored in the ClientState key.
const TCHAR* const kRegValueLanguage         = _T("lang");
const TCHAR* const kRegValueAppName          = _T("name");
const TCHAR* const kRegValueProductVersion   = _T("pv");

// Registry values stored in the ClientState key.
const TCHAR* const kRegValueAdditionalParams = _T("ap");
const TCHAR* const kRegValueBrandCode        = _T("brand");
const TCHAR* const kRegValueBrowser          = _T("browser");
const TCHAR* const kRegValueClientId         = _T("client");
const TCHAR* const kRegValueDidRun           = _T("dr");
const TCHAR* const kRegValueExperimentLabels = _T("experiment_labels");
const TCHAR* const kRegValueInstallationId   = _T("iid");
const TCHAR* const kRegValueOemInstall       = _T("oeminstall");
const TCHAR* const kRegValuePingFreshness    = _T("ping_freshness");
const TCHAR* const kRegValueReferralId       = _T("referral");

// Prefix for App-Defined attribute values stored in the ClientState key.
const TCHAR* const kRegValueAppDefinedPrefix = _T("_");

// Aggregates are app-defined attribute subkeys that store values that need to
// be aggregated. The only aggregate supported at the moment is "sum()".
const TCHAR* const kRegValueAppDefinedAggregate    = _T("aggregate");
const TCHAR* const kRegValueAppDefinedAggregateSum = _T("sum()");

// Registry values stored at and under the ClientState\{AppID}\CurrentState key.
const TCHAR* const kRegSubkeyCurrentState           = _T("CurrentState");
const TCHAR* const kRegValueStateValue              = _T("StateValue");
const TCHAR* const kRegValueDownloadTimeRemainingMs =
    _T("DownloadTimeRemainingMs");
const TCHAR* const kRegValueDownloadProgressPercent =
    _T("DownloadProgressPercent");
const TCHAR* const kRegValueInstallTimeRemainingMs  =
    _T("InstallTimeRemainingMs");
const TCHAR* const kRegValueInstallProgressPercent  =
    _T("InstallProgressPercent");

// This two registries hold client UTC timestamp of server's midnight of the day
// that last active ping/roll call happened.
const TCHAR* const kRegValueActivePingDayStartSec = _T("ActivePingDayStartSec");
const TCHAR* const kRegValueRollCallDayStartSec   = _T("RollCallDayStartSec");

// These three registry values hold the number of days have elapsed since
// Jan 1. 2007 when initial install or last active ping/roll call happened.
// The values are from server's response.
const TCHAR* const kRegValueDayOfLastActivity     = _T("DayOfLastActivity");
const TCHAR* const kRegValueDayOfLastRollCall     = _T("DayOfLastRollCall");
const TCHAR* const kRegValueDayOfInstall          = _T("DayOfInstall");

// Registry values stored in the ClientState key related to Omaha's actions.
// A "successful check" means "noupdate" received from the server or an update
// was successfully applied.
const TCHAR* const kRegValueInstallTimeSec          = _T("InstallTime");
const TCHAR* const kRegValueLastSuccessfulCheckSec  = _T("LastCheckSuccess");
const TCHAR* const kRegValueLastUpdateTimeSec       = _T("UpdateTime");

// Registry values stored in the ClientState or ClientStateMedium keys.
// Use accessor methods rather than reading them directly.
const TCHAR* const kRegValueEulaAccepted     = _T("eulaaccepted");
const TCHAR* const kRegValueUsageStats       = _T("usagestats");

// Registry values stored in the ClientState key for Omaha's internal use.
const TCHAR* const kRegValueTTToken               = _T("tttoken");
const TCHAR* const kRegValueUpdateAvailableCount  = _T("UpdateAvailableCount");
const TCHAR* const kRegValueUpdateAvailableSince  = _T("UpdateAvailableSince");

const TCHAR* const kRegSubkeyCohort               = _T("cohort");
const TCHAR* const kRegValueCohortHint            = _T("hint");
const TCHAR* const kRegValueCohortName            = _T("name");

// Registry values stored in the Update key.
const TCHAR* const kRegValueRuntimeMode           = _T("RuntimeMode");
const TCHAR* const kRegValueOmahaEulaAccepted     = _T("eulaaccepted");
// TODO(omaha3): Consider renaming these if there is not a upgrade problem.
// If we can't consider moving all "gupdate" values to the customization file.
// Use a non-gupdate name for the new medium service.
#define SERVICE_PREFIX _T("omaha")
#define MEDIUM_SERVICE_PREFIX _T("omaham")
const TCHAR* const kServicePrefix                 = SERVICE_PREFIX;
const TCHAR* const kMediumServicePrefix           = MEDIUM_SERVICE_PREFIX;
const TCHAR* const kRegValueServiceName           = SERVICE_PREFIX _T("_service_name");
const TCHAR* const kRegValueMediumServiceName     = MEDIUM_SERVICE_PREFIX _T("_service_name");
const TCHAR* const kRegValueTaskNameC             = SERVICE_PREFIX _T("_task_name_c");
const TCHAR* const kRegValueTaskNameUA            = SERVICE_PREFIX _T("_task_name_ua");
const TCHAR* const kRegValueLastStartedAU         = _T("LastStartedAU");
const TCHAR* const kRegValueLastChecked           = _T("LastChecked");
const TCHAR* const kRegValueLastCoreRun           = _T("LastCoreRun");
const TCHAR* const kRegValueLastCodeRedCheck      = _T("LastCodeRedCheck");
const TCHAR* const kRegValueOemInstallTimeSec     = _T("OemInstallTime");
const TCHAR* const kRegValueCacheSizeLimitMBytes  = _T("PackageCacheSizeLimit");
const TCHAR* const kRegValueCacheLifeLimitDays    = _T("PackageCacheLifeLimit");
const TCHAR* const kRegValueInstalledPath         = _T("path");
const TCHAR* const kRegValueUninstallCmdLine      = _T("UninstallCmdLine");
const TCHAR* const kRegValueSelfUpdateExtraCode1  = _T("UpdateCode1");
const TCHAR* const kRegValueSelfUpdateErrorCode   = _T("UpdateError");
const TCHAR* const kRegValueSelfUpdateVersion     = _T("UpdateVersion");
const TCHAR* const kRegValueInstalledVersion      = _T("version");
const TCHAR* const kRegValueLastOSVersion         = _T("LastOSVersion");

// Indicates the time when it is safe for the client to connect to the server
// for update checks. See the explanation of kHeaderXRetryAfter in constants.h.
const TCHAR* const kRegValueRetryAfter            = _T("RetryAfter");

// UID registry entries.
const TCHAR* const kRegValueUserId                = _T("uid");
const TCHAR* const kRegValueOldUserId             = _T("old-uid");
const TCHAR* const kRegSubkeyUserId               = _T("uid");
const TCHAR* const kRegValueUserIdCreateTime      = _T("uid-create-time");
const TCHAR* const kRegValueUserIdNumRotations    = _T("uid-num-rotations");
const TCHAR* const kRegValueLegacyMachineId       = _T("mi");
const TCHAR* const kRegValueLegacyUserId          = _T("ui");


// This value is appended to the X-Old-UID header if no subkey "uid" existed.
const TCHAR* const kRegValueDataLegacyUserId      = _T("; legacy");

// These values are appended to the the X-Old-UID header in the format of
// "X-Old-Uid: {UID}; legacy; age=150; cnt=2". cnt is the number of UID
// rotations. age is the number of days since last rotation. Special value "-1"
// is used when last rotation happened within the past 1 minute.
const TCHAR* const kHeaderValueNumUidRotation     = _T("cnt");
const TCHAR* const kHeaderValueUidAge             = _T("age");

#if defined(HAS_DEVICE_MANAGEMENT)

// The full path of the registry key where Omaha persists state related to
// cloud-based device management.
const TCHAR kRegKeyCompanyEnrollment[] =
    MACHINE_KEY COMPANY_MAIN_KEY _T("Enrollment\\");

#if defined(HAS_LEGACY_DM_CLIENT)

// The full path of the registry key where Google Chrome stored the DM token
// when it was solely responsible for registration and use.
const TCHAR kRegKeyLegacyEnrollment[] =
    MACHINE_KEY_64 COMPANY_MAIN_KEY LEGACY_DM_CLIENT_APP _T("\\Enrollment\\");

#endif  // defined(HAS_LEGACY_DM_CLIENT)

// The name of the registry value, within an "Enrollment" key above, holding a
// device management token.
const TCHAR kRegValueDmToken[] = _T("dmtoken");

// The name of the registry value, within Omaha's ClientState key, where an
// install's enrollment token is stored.
const TCHAR kRegValueCloudManagementEnrollmentToken[] =
    _T("CloudManagementEnrollmentToken");

#endif  // defined(HAS_DEVICE_MANAGEMENT)

const TCHAR* const kScheduledTaskNameUserPrefix =
    APP_NAME_IDENTIFIER _T("TaskUser");
const TCHAR* const kScheduledTaskNameMachinePrefix =
    APP_NAME_IDENTIFIER _T("TaskMachine");
const TCHAR* const kScheduledTaskNameCoreSuffix = _T("Core");
const TCHAR* const kScheduledTaskNameUASuffix   = _T("UA");

const TCHAR* const kServiceFileName              = kOmahaShellFileName;
const char*  const kGoopdateDllEntryAnsi         = "DllEntry";

// The name of the Omaha product in Google's Crash reporting system.
const TCHAR* const kCrashOmahaProductName        = _T("Update2");

// Event Id's used for reporting in the event log.
// Crash Report events.
const int kCrashReportEventId        = 1;
const int kCrashUploadEventId        = 2;

// Omaha CustomInfoEntry entry names in Google's Crash reporting system.
const TCHAR* const kCrashCustomInfoCommandLineMode  = _T("CommandLineMode");

// Update Check events.
const int kUpdateCheckEventId        = 11;
const int kUpdateEventId             = 12;
const int kUninstallEventId          = 13;
const int kWorkerStartEventId        = 14;
const int kDownloadEventId           = 15;

// Network Request events.
const int kNetworkRequestEventId     = 20;

// Device management events.
const int kEnrollmentFailedEventId = 30;
const int kEnrollmentRequiresNetworkEventId = 31;
const int kRefreshPoliciesFailedEventId = 32;

// Maximum value the server can respond for elapsed_seconds attribute in
// <daystart ...> element. The value is one day plus an hour ("fall back"
// daylight savings).
const int kMaxTimeSinceMidnightSec   = ((24 + 1) * 60 * 60);

// Value range the server can respond for elapsed_days attribute in
// <daystart ...> element. The value is number of days has passed since
// Jan. 1, 2007.
const int kMinDaysSinceDatum = 2400;   // Maps to Jul 28, 2013.
const int kMaxDaysSinceDatum = 50000;  // This will break on Nov 24, 2143.

// Maximum time to keep the Installation ID. If the app was installed longer
// than this time ago, the Installation ID will be deleted regardless of
// whether the application has been run or not.
const int kMaxLifeOfInstallationIDSec = (7 * 24 * 60 * 60);  // 7 days

// Documented in the IDL for certain properties of ICurrentState.
const int kCurrentStateProgressUnknown = -1;

// COM ProgIDs.
#define kProgIDUpdate3COMClassUser \
    APP_NAME_IDENTIFIER _T(".Update3COMClassUser")
#define kProgIDUpdate3COMClassService \
    APP_NAME_IDENTIFIER _T(".Update3COMClassService")

const TCHAR* const kProgIDOnDemandUser =
    APP_NAME_IDENTIFIER _T(".OnDemandCOMClassUser");
#define kProgIDOnDemandMachine \
    APP_NAME_IDENTIFIER _T(".OnDemandCOMClassMachine")
const TCHAR* const kProgIDOnDemandMachineFallback =
    APP_NAME_IDENTIFIER _T(".OnDemandCOMClassMachineFallback");
const TCHAR* const kProgIDOnDemandSvc =
    APP_NAME_IDENTIFIER _T(".OnDemandCOMClassSvc");

const TCHAR* const kProgIDUpdate3WebUser =
    APP_NAME_IDENTIFIER _T(".Update3WebUser");
#define kProgIDUpdate3WebMachine \
    APP_NAME_IDENTIFIER _T(".Update3WebMachine")
const TCHAR* const kProgIDUpdate3WebMachineFallback =
    APP_NAME_IDENTIFIER _T(".Update3WebMachineFallback");
const TCHAR* const kProgIDUpdate3WebSvc =
    APP_NAME_IDENTIFIER _T(".Update3WebSvc");

const TCHAR* const kProgIDGoogleUpdateCoreService =
    APP_NAME_IDENTIFIER _T(".CoreClass");
const TCHAR* const kProgIDGoogleUpdateCoreMachine =
    APP_NAME_IDENTIFIER _T(".CoreMachineClass");

const TCHAR* const kProgIDProcessLauncher =
    APP_NAME_IDENTIFIER _T(".ProcessLauncher");

const TCHAR* const kProgIDCoCreateAsync =
    APP_NAME_IDENTIFIER _T(".CoCreateAsync");

const TCHAR* const kProgIDCredentialDialogUser =
    APP_NAME_IDENTIFIER _T(".CredentialDialogUser");
const TCHAR* const kProgIDCredentialDialogMachine =
    APP_NAME_IDENTIFIER _T(".CredentialDialogMachine");

const TCHAR* const kProgIDPolicyStatusUser =
    APP_NAME_IDENTIFIER _T(".PolicyStatusUser");
#define kProgIDPolicyStatusMachine \
    APP_NAME_IDENTIFIER _T(".PolicyStatusMachine")
const TCHAR* const kProgIDPolicyStatusMachineFallback =
    APP_NAME_IDENTIFIER _T(".PolicyStatusMachineFallback");
const TCHAR* const kProgIDPolicyStatusSvc =
    APP_NAME_IDENTIFIER _T(".PolicyStatusSvc");

// Offline v3 manifest name.
const TCHAR* const kOfflineManifestFileName = _T("OfflineManifest.gup");

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_GOOPDATE_H_
