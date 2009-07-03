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
// Kernel object names.
//
// TODO(omaha): rename MutexPrefix to ObjectPrefix
//              remove the similar names from constants.h

#ifndef OMAHA_COMMON_CONST_OBJECT_NAMES_H__
#define OMAHA_COMMON_CONST_OBJECT_NAMES_H__

#include <tchar.h>

namespace omaha {

// The prefix to use for global names in the win32 API's.
const TCHAR* const kGlobalPrefix = _T("Global\\G");

// Preserve legacy prefixes
const TCHAR* const kOmaha10GlobalPrefix = _T("Global\\");
const TCHAR* const kOmaha11GlobalPrefix = _T("Global\\G");
const TCHAR* const kOmaha11LocalPrefix = _T("Local\\L");

// Ensures that only one instance of machine or user Omaha is trying to setup at
// a time.
const TCHAR* const kSetupMutex = _T("{A9A86B93-B54E-4570-BE89-42418507707B}");

// Signals the process to exit. Currently the core and the worker listen to
// this event.
// This same name was used in Omaha 1 (post-i18n).
// TODO(omaha): Consider making all our processes listen to it. Maybe not the
// service, since the SCM controls the life time of the service.
const TCHAR* const kShutdownEvent =
    _T("{A0C1F415-D2CE-4ddc-9B48-14E56FD55162}");

// Only used to shut down older (pre-i18n) Goopdates.
const TCHAR* const kEventLegacyQuietModeName =
    _T("Global\\%s{B048F41D-5515-40eb-B4A6-B7F460379454}");

// The installed setup worker sends an event to the setup worker running from
// the temp directory to tell it to release the Setup Lock. The event's name
// is passed in this environment variable name.
const TCHAR* const kSetupCompleteEventEnvironmentVariableName =
    _T("GOOGLE_UPDATE_SETUP_COMPLETE_EVENT_NAME");

// The installed setup worker sends an event to the setup worker running from
// the temp directory to tell it a UI has been displayed so it will not display
// a second UI on error. The event's name is passed in this environment variable
// name.
const TCHAR* const kUiDisplayedEventEnvironmentVariableName =
    _T("GOOGLE_UPDATE_UI_DISPLAYED_EVENT_NAME");

// It enforces the Core only runs one instance per machine and one instance per
// each user session.
const TCHAR* const kCoreSingleInstance =
    _T("{B5665124-2B19-40e2-A7BC-B44321E72C4B}");

// It enforces the Crash Handler only runs one instance per machine and one
// instance per each user session.
const TCHAR* const kCrashHandlerSingleInstance =
    _T("{C4F406E5-F024-4e3f-89A7-D5AB7663C3CD}");

// The mutex names for ensuring single instances of the worker.
// We allow only one instance of the update worker per session,
// and only one instance of the install worker per application.
// However since user omaha cannot create global names, the interactive
// worker is also per session. I.e. two users can simultaneously install
// the same application.
const TCHAR* const kSingleupdateWorker =
    _T("{D0BB2EF1-C183-4cdb-B218-040922092869}");

// Mutex name for ensuring only one installer for an app is running in a
// session. The %s is replaced with the application guid.
const TCHAR* const kSingleInstallWorker =
    _T("%s-{F707E94F-D66B-4525-AD84-B1DA87D6A971}");

// Base name of job object for Setup phase 1 processes except self updates.
// These may not be running as Local System for machine installs like
// self-updates do.
const TCHAR* const kSetupPhase1NonSelfUpdateJobObject =
    _T("{5A913EF1-4160-48bc-B688-4D67EAEB698A}");

// Base name of job object for interactive install processes except /install.
const TCHAR* const kAppInstallJobObject =
    _T("{8AD051DB-4FE6-458b-B103-7DCC78D56013}");

// Base name of job object for silent processes that are okay to kill.
const TCHAR* const kSilentJobObject =
    _T("{A2300FD6-CBED-48a6-A3CB-B35C38A42F8E}");

// Base name of job object for silent processes that should not be killed.
const TCHAR* const kSilentDoNotKillJobObject =
    _T("{D33A8A53-F57D-4fd9-A32D-238FD69B4BC4}");

// The global lock to ensure that a single app is being installed for this
// user/machine at a given time.
const TCHAR* const kInstallManagerSerializer =
    _T("{0A175FBE-AEEC-4fea-855A-2AA549A88846}");

// Serializes access to metrics stores, machine and user, respectively.
const TCHAR* const kMetricsSerializer =
    _T("{C68009EA-1163-4498-8E93-D5C4E317D8CE}");

// Serializes access to the global network configuration, such as the CUP keys.
const TCHAR* const kNetworkConfigLock =
    _T("{0E900C7B-04B0-47f9-81B0-F8D94F2DF01B}");

// The name of the shared memory object containing the serialized COM
// interface pointer exposed by the machine core.
const TCHAR* const kGoogleUpdateCoreSharedMemoryName =
    _T("Global\\GoogleUpdateCore");

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_OBJECT_NAMES_H__

