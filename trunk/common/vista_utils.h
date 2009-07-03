// Copyright 2006-2009 Google Inc.
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

#ifndef OMAHA_COMMON_VISTA_UTILS_H__
#define OMAHA_COMMON_VISTA_UTILS_H__

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

// Constants.
const TCHAR* const kExplorer = _T("EXPLORER.EXE");
const TCHAR* const kIExplore = _T("IEXPLORE.EXE");

namespace vista {

// Returns true if the current process is running in 'protected mode'.
bool IsProcessProtected();

// Allows processes that run under protected mode access to a shared kernel
// object such as mapped memory.
//
// Returns S_OK if successful, S_FALSE if not running on vista, or an
// error value.
HRESULT AllowProtectedProcessAccessToSharedObject(const TCHAR* name);

// Restarts IEUser process if we can. This is to allow for
// IEUser.exe to refresh it's ElevationPolicy cache. Due to a bug
// within IE7, IEUser.exe does not refresh it's cache unless it
// is restarted in the manner below. If the cache is not refreshed
// IEUser does not respect any new ElevationPolicies that a fresh
// setup program installs for an ActiveX control or BHO. This code
// is adapted from Toolbar.
HRESULT RestartIEUser();

// TODO(Omaha): Move these to a different utils file, since these are not
// Vista-specific.
// TODO(Omaha): rename for consistency with
// GetProcessPidsForActiveUserOrSession.
//
// Gets current user's explorer.exe pid. If that fails, gets the pid of any
// explorer.exe running in the current session.
HRESULT GetExplorerPidForCurrentUserOrSession(uint32* pid);

// Returns the TOKEN of the explorer process of any user that is logged in.
HRESULT GetExplorerTokenForLoggedInUser(HANDLE* token);

// Retrieves a primary token for one of the logged on users. The logged on
// user is either the current user or a user logged on in the same session as
// the current user. The caller must close the token handle.
HRESULT GetLoggedOnUserToken(HANDLE* token);

// Get PIDs for the processes running with the specified executable, user_sid,
// and session_id. user_sid can be blank, in which case, the search will
// encompass all processes with the given name in session_id. The session
// always has to be a valid session, hence the name GetPidsInSession().
HRESULT GetPidsInSession(const TCHAR* exe_name,
                         const TCHAR* user_sid,
                         DWORD session_id,
                         std::vector<uint32>* pids);

// Get the handle of exe_name running under the active user or active session.
// If the call is made from the SYSTEM account, returns PIDs for exe_name
// in the currently active user session. If the call is made from a user account
// returns PIDs for that user, or if that cannot be found, in the current
// session.
HRESULT GetProcessPidsForActiveUserOrSession(const TCHAR* exe_name,
                                             std::vector<uint32>* pids);

// Starts process with the token obtained from the specified process.
HRESULT StartProcessWithTokenOfProcess(uint32 pid,
                                       const CString& command_line);

// Runs the command on behalf of the current user. Creates a fresh environment
// block based on the user's token.
HRESULT RunAsCurrentUser(const CString& command_line);

}  // namespace vista

}  // namespace omaha

#endif  // OMAHA_COMMON_VISTA_UTILS_H__

