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

#ifndef OMAHA_BASE_CONST_OBJECT_NAMES_H_
#define OMAHA_BASE_CONST_OBJECT_NAMES_H_

#include <tchar.h>
#include "omaha/base/constants.h"

namespace omaha {

// The prefix to use for global names in the win32 API's.
const TCHAR* const kGlobalPrefix = _T("Global\\Omaha");

const TCHAR kCrashPipeNamePrefix[] =
    _T("\\\\.\\pipe\\") SHORT_COMPANY_NAME _T("CrashServices");

// Ensures that only one instance of machine or user Omaha is trying to setup at
// a time.
const TCHAR* const kSetupMutex = _T("{A9A86B93-B54E-4570-BE89-42418507707B}");

// TODO(omaha3): Update this comment.
// Signals the process to exit. Currently the core and the worker listen to
// this event.
// TODO(omaha): Consider making all our processes listen to it. Maybe not the
// service, since the SCM controls the life time of the service.
const TCHAR* const kShutdownEvent =
    _T("{A0C1F415-D2CE-4ddc-9B48-14E56FD55162}");

// This is for Omaha2 backwards compatibility.
// The installed Omaha3 handoff process sets an event to tell an Omaha2 setup
// worker running from the temp directory that a UI has been displayed so that
// the Omaha2 worker will not display a second UI on error. The event's name is
// passed in this environment variable name by the Omaha2 worker.
const TCHAR* const kLegacyUiDisplayedEventEnvironmentVariableName =
    _T("GOOGLE_UPDATE_UI_DISPLAYED_EVENT_NAME");

// Ensures the Core only runs one instance per machine and one instance per
// each user session.
const TCHAR* const kCoreSingleInstance =
    _T("{B5665124-2B19-40e2-A7BC-B44321E72C4B}");

// Ensures the Crash Handler only runs one instance per machine and one
// instance per each user session.
const TCHAR* const kCrashHandlerSingleInstance =
    _T("{C4F406E5-F024-4e3f-89A7-D5AB7663C3CD}");

// Ensures the /ua process only runs one instance per machine and one
// instance per each user session.
const TCHAR* const kUpdateAppsSingleInstance =
    _T("{D0BB2EF1-C183-4cdb-B218-040922092869}");

// Ensures only one installer for an app is running in a session.
// The %s is replaced with the application ID.
const TCHAR* const kInstallAppSingleInstance =
    _T("%s-{F707E94F-D66B-4525-AD84-B1DA87D6A971}");

// Ensures the GoogleUpdate3 server only runs one instance per machine and one
// instance per each user session.
const TCHAR* const kGoogleUpdate3SingleInstance =
    _T("{6885AE8E-C070-458d-9711-37B9BEAB65F6}");

// TODO(omaha): Delete Job Object code.

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

// Serializes access to the registry for application state.
const TCHAR* const kRegistryAccessMutex =
    _T("{66CC0160-ABB3-4066-AE47-1CA6AD5065C8}");

// Serializes opt user id generation.
const TCHAR* const kOptUserIdLock =
    _T("{D19BAF17-7C87-467E-8D63-6C4B1C836373}");

// The name of the shared memory objects containing the serialized COM
// interface pointers exposed by the machine core.
// TODO(omaha): Rename these constants to remove "GoogleUpdate".
// TODO(omaha): Consider following the kGlobalPrefix convention with the 'G'
// for the new shared Omaha 3 name  and building this from the same #define as
// kGlobalPrefix.
const TCHAR* const kGoogleUpdate3SharedMemoryName =
    _T("Global\\") APP_NAME_IDENTIFIER _T("3");
const TCHAR* const kGoogleUpdateCoreSharedMemoryName =
    _T("Global\\") APP_NAME_IDENTIFIER _T("Core");

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_OBJECT_NAMES_H_
