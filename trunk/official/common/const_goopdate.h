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

// Registry values read from the Clients key for transmitting custom install
// errors, messages, etc. On an update, the InstallerXXX values are renamed to
// LastInstallerXXX values. The LastInstallerXXX values remain around until the
// next update.
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
const TCHAR* const kRegValueCommandLine      = _T("CommandLine");
const TCHAR* const kRegValueSendsPings       = _T("SendsPings");
const TCHAR* const kRegValueWebAccessible    = _T("WebAccessible");
const TCHAR* const kRegValueReportingId      = _T("ReportingId");

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
const TCHAR* const kRegValueReferralId       = _T("referral");

// This two registries hold client UTC timestamp of server's midnight of the day
// that last active ping/roll call happened.
const TCHAR* const kRegValueActivePingDayStartSec = _T("ActivePingDayStartSec");
const TCHAR* const kRegValueRollCallDayStartSec   = _T("RollCallDayStartSec");

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

// Registry values stored in the Update key.
const TCHAR* const kRegValueDelayOmahaUninstall   = _T("DelayUninstall");
const TCHAR* const kRegValueOmahaEulaAccepted     = _T("eulaaccepted");
// TODO(omaha3): Consider renaming these if there is not a upgrade problem.
// If we can't consider moving all "gupdate" values to the customization file.
const TCHAR* const kRegValueServiceName           = _T("gupdate_service_name");
const TCHAR* const kRegValueMediumServiceName     = _T("gupdatem_service_name");
const TCHAR* const kRegValueTaskNameC             = _T("gupdate_task_name_c");
const TCHAR* const kRegValueTaskNameUA            = _T("gupdate_task_name_ua");
const TCHAR* const kRegValueLastChecked           = _T("LastChecked");
const TCHAR* const kRegValueOemInstallTimeSec     = _T("OemInstallTime");
const TCHAR* const kRegValueCacheSizeLimitMBytes  = _T("PackageCacheSizeLimit");
const TCHAR* const kRegValueCacheLifeLimitDays    = _T("PackageCacheLifeLimit");
const TCHAR* const kRegValueInstalledPath         = _T("path");
const TCHAR* const kRegValueUserId                = _T("uid");
const TCHAR* const kRegValueSelfUpdateExtraCode1  = _T("UpdateCode1");
const TCHAR* const kRegValueSelfUpdateErrorCode   = _T("UpdateError");
const TCHAR* const kRegValueSelfUpdateVersion     = _T("UpdateVersion");
const TCHAR* const kRegValueInstalledVersion      = _T("version");

// TODO(omaha3): Consider moving all "gupdate" values to the customization file.
// Use a non-gupdate name for the new medium service.
const TCHAR* const kServicePrefix               = _T("gupdate");
const TCHAR* const kMediumServicePrefix         = _T("gupdatem");

const TCHAR* const kScheduledTaskNameUserPrefix =
    APP_NAME_IDENTIFIER _T("TaskUser");
const TCHAR* const kScheduledTaskNameMachinePrefix =
    APP_NAME_IDENTIFIER _T("TaskMachine");
const TCHAR* const kScheduledTaskNameCoreSuffix = _T("Core");
const TCHAR* const kScheduledTaskNameUASuffix   = _T("UA");

const TCHAR* const kServiceFileName              = kOmahaShellFileName;
const char*  const kGoopdateDllEntryAnsi         = "DllEntry";


// Event Id's used for reporting in the event log.
// Crash Report events.
const int kCrashReportEventId        = 1;
const int kCrashUploadEventId        = 2;

// Update Check events.
const int kUpdateCheckEventId        = 11;
const int kUpdateEventId             = 12;
const int kUninstallEventId          = 13;
const int kWorkerStartEventId        = 14;
const int kDownloadEventId           = 15;

// Network Request events.
const int kNetworkRequestEventId     = 20;

// Maximum value the server can respond for elapsed_seconds attribute in
// <daystart ...> element. The value is one day plus an hour ("fall back"
// daylight savings).
const int kMaxTimeSinceMidnightSec   = ((24 + 1) * 60 * 60);

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

const TCHAR* const kProgIDOneClickProcessLauncherUser =
    _T(SHORT_COMPANY_NAME_ANSI) _T(".OneClickProcessLauncherUser");
const TCHAR* const kProgIDOneClickProcessLauncherMachine =
    _T(SHORT_COMPANY_NAME_ANSI) _T(".OneClickProcessLauncherMachine");

const TCHAR* const kProgIDCoCreateAsync =
    APP_NAME_IDENTIFIER _T(".CoCreateAsync");

const TCHAR* const kProgIDCredentialDialogUser =
    APP_NAME_IDENTIFIER _T(".CredentialDialogUser");
const TCHAR* const kProgIDCredentialDialogMachine =
    APP_NAME_IDENTIFIER _T(".CredentialDialogMachine");

// Offline v3 manifest name.
const TCHAR* const kOfflineManifestFileName = _T("OfflineManifest.gup");

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_GOOPDATE_H_
