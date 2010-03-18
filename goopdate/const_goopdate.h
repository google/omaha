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

#ifndef OMAHA_GOOPDATE_CONST_GOOPDATE_H__
#define OMAHA_GOOPDATE_CONST_GOOPDATE_H__

#include <tchar.h>

namespace omaha {

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

// Using extern or intern linkage for these strings yields the same code size
// for the executable DLL.


// Registry values read from the Clients key for transmitting custom install
// errors, messages, etc. On an update, the InstallerXXX values are renamed to
// LastInstallerXXX values. The LastInstallerXXX values remain around until the
// next update.
const TCHAR* const kRegValueInstallerResult = _T("InstallerResult");
const TCHAR* const kRegValueInstallerError  = _T("InstallerError");
const TCHAR* const kRegValueInstallerResultUIString =
    _T("InstallerResultUIString");
const TCHAR* const kRegValueInstallerSuccessLaunchCmdLine =
    _T("InstallerSuccessLaunchCmdLine");
const TCHAR* const kRegValueLastInstallerResult =
    _T("LastInstallerResult");
const TCHAR* const kRegValueLastInstallerError =
    _T("LastInstallerError");
const TCHAR* const kRegValueLastInstallerResultUIString =
    _T("LastInstallerResultUIString");
const TCHAR* const kRegValueLastInstallerSuccessLaunchCmdLine =
    _T("LastInstallerSuccessLaunchCmdLine");

// Registry subkey in an app's Clients key that contains its components.
const TCHAR* const kComponentsRegKeyName     = _T("Components");

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
const TCHAR* const kRegValueOmahaEulaAccepted     = _T("eulaaccepted");
const TCHAR* const kRegValueServiceName           = _T("gupdate_service_name");
const TCHAR* const kRegValueTaskNameC             = _T("gupdate_task_name_c");
const TCHAR* const kRegValueTaskNameUA            = _T("gupdate_task_name_ua");
const TCHAR* const kRegValueLastChecked           = _T("LastChecked");
const TCHAR* const kRegValueOemInstallTimeSec     = _T("OemInstallTime");
const TCHAR* const kRegValueInstalledPath         = _T("path");
const TCHAR* const kRegValueSelfUpdateExtraCode1  = _T("UpdateCode1");
const TCHAR* const kRegValueSelfUpdateErrorCode   = _T("UpdateError");
const TCHAR* const kRegValueSelfUpdateVersion     = _T("UpdateVersion");
const TCHAR* const kRegValueInstalledVersion      = _T("version");

// TODO(omaha): Remove with legacy Omaha 1 support.
// Used to allow current builds to read opt-in values from previous builds.
// This is deprecated in favor of kRegValueUsageStats.
const TCHAR* const kLegacyRegValueCollectUsageStats = _T("CollectUsageStats");

const TCHAR* const kLegacyServiceName           = _T("gupdate");
const TCHAR* const kServicePrefix               = _T("gupdate");

const TCHAR* const kScheduledTaskNameUserPrefix = _T("GoogleUpdateTaskUser");
const TCHAR* const kScheduledTaskNameMachinePrefix =
    _T("GoogleUpdateTaskMachine");
const TCHAR* const kScheduledTaskNameCoreSuffix = _T("Core");
const TCHAR* const kScheduledTaskNameUASuffix   = _T("UA");

// TODO(omaha): make these two below the same symbol.
const TCHAR* const kServiceFileName              = _T("GoogleUpdate.exe");
const TCHAR* const kGoopdateFileName             = _T("GoogleUpdate.exe");
const TCHAR* const kGoopdateCrashHandlerFileName = _T("GoogleCrashHandler.exe");
const TCHAR* const kGoopdateDllName              = _T("goopdate.dll");
const TCHAR* const kGoopdateResourceDllName      = _T("goopdateres_%s.dll");
const TCHAR* const kGoopdateResDllFindPattern    = _T("goopdateres_*.dll");
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

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_CONST_GOOPDATE_H__
