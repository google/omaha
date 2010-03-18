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

#ifndef OMAHA_COMMON_VISTAUTIL_H__
#define OMAHA_COMMON_VISTAUTIL_H__

#include <windows.h>
#include <tchar.h>
#include <accctrl.h>
#include <Aclapi.h>
#include <Sddl.h>
#include <WinNT.h>
#include <atlsecurity.h>

namespace omaha {

// SACLs are normally used for auditing, but Vista also uses them to
// determine integrity levels.
// For more info, http://www.google.com/search?q=SDDL+for+Mandatory+Labels
// S = SACL
// ML = Mandatory label (aka integrity level)
// NW = No write up (integrity levels less than low cannot gain access)
// LW = Low Integrity Level (What IE normally runs in)

// The LABEL_SECURITY_INFORMATION SDDL SACL for medium integrity.
// L"S:(ML;;NW;;;ME)"
#define MEDIUM_INTEGRITY_SDDL_SACL  SDDL_SACL             \
                                    SDDL_DELIMINATOR      \
                                    SDDL_ACE_BEGIN        \
                                    SDDL_MANDATORY_LABEL  \
                                    SDDL_SEPERATOR        \
                                    SDDL_SEPERATOR        \
                                    SDDL_NO_WRITE_UP      \
                                    SDDL_SEPERATOR        \
                                    SDDL_SEPERATOR        \
                                    SDDL_SEPERATOR        \
                                    SDDL_ML_MEDIUM        \
                                    SDDL_ACE_END

// The LABEL_SECURITY_INFORMATION SDDL SACL for low integrity.
// L"S:(ML;;NW;;;LW)"
#define LOW_INTEGRITY_SDDL_SACL     SDDL_SACL             \
                                    SDDL_DELIMINATOR      \
                                    SDDL_ACE_BEGIN        \
                                    SDDL_MANDATORY_LABEL  \
                                    SDDL_SEPERATOR        \
                                    SDDL_SEPERATOR        \
                                    SDDL_NO_WRITE_UP      \
                                    SDDL_SEPERATOR        \
                                    SDDL_SEPERATOR        \
                                    SDDL_SEPERATOR        \
                                    SDDL_ML_LOW           \
                                    SDDL_ACE_END

namespace vista_util {

// This is fast, since it caches the answer after first run.
bool IsVistaOrLater();

// Is the user running on Vista or later with a split-token.
HRESULT IsUserRunningSplitToken(bool* is_split_token);

// Returns true if the user has the reg key for disabling UAC policy at startup
// set.
bool IsUACDisabled();

// Returns true if the process is running under credentials of an user
// belonging to the admin group in case of pre-Vista and in case Vista
// returns true if the user is running as an elevated admin.
bool IsUserAdmin();

// Returns true if the user is running as a non-elevated admin in case of
// Vista. In case of XP always returns false.
bool IsUserNonElevatedAdmin();

// Determine the mandatory level of a process
//   processID, the process to query, or (0) to use the current process
//   On Vista, level should alwys be filled in with either
//     MandatoryLevelLow (IE)
//     MandatoryLevelMedium(user), or
//     MandatoryLevelHigh( Elevated Admin)
//   On error, level remains unchanged
HRESULT GetProcessIntegrityLevel(DWORD processID, MANDATORY_LEVEL* level);

// Elevated processes need to be careful how they launch child processes
// to avoid having them inherit too many credentials or not being able to
// elevate their own IE processes normally.  Microsoft's advice from
// http://msdn.microsoft.com/library/en-us/ietechcol/dnwebgen/protectedmode.asp
// will launch a low integrity IE, but that IE cannot elevate properly since
// it was running from the wrong token. The best method I can gather is to find
// an existing process on the machine running at normal user rights, and launch
// this process impersonating that token rather than trying to adjust token
// privileges of the elevated token.  TODO(omaha): Implement and test this.
HRESULT CreateProcessAsNormalUserFromElevatedAdmin(const TCHAR* commandline,
    STARTUPINFO* startup_info, PROCESS_INFORMATION* process_info);

// Starts a new elevated process. file_path specifies the program to be run.
// If exit_code is not null, the function waits until the spawned process has
// completed. The exit code of the process is returned therein.
// If exit_code is null, the function will return after spawning the program
// and will not wait for completion.
// show_window is one of the SW_* constants to specify howw the windows is
// opened.
HRESULT RunElevated(const TCHAR* file_path, const TCHAR* parameters,
    int show_window, DWORD* exit_code);

// If there is no specific integrity level defined, return S_FALSE (1) and set
// level to MandatoryLevelMedium (the Vista default)
HRESULT GetFileOrFolderIntegrityLevel(const TCHAR* file,
    MANDATORY_LEVEL* level, bool* and_children);

// A level of MandatoryLevelUntrusted (0) will remove the integrity level for
// this file and all children
HRESULT SetFileOrFolderIntegrityLevel(const TCHAR* file,
    MANDATORY_LEVEL level, bool and_children);

// If there is no specific integrity level defined, return S_FALSE (1) and set
// level to MandatoryLevelMedium (the Vista default)
// root must be one of the 4 pre-defined roots: HKLM, HKCU, HKCR, HCU
HRESULT GetRegKeyIntegrityLevel(HKEY root, const TCHAR* subkey,
    MANDATORY_LEVEL* level, bool* and_children);

// A level of MandatoryLevelUntrusted (0) will remove the integrity label
// root must be one of the 4 pre-defined roots: HKLM, HKCU, HKCR, HCU
HRESULT SetRegKeyIntegrityLevel(HKEY root, const TCHAR* subkey,
    MANDATORY_LEVEL level, bool and_children);

// Creates a security descriptor that can be used to make an object accessible
// from the specified integrity level. When not running on Windows Vista or
// in case of errors, the function returns NULL, which results in using
// the default security descriptor.
// The caller must take ownership of the returned security descriptor.
// Mask will be added as an allowed ACE of the DACL.
// For example, use MUTEX_ALL_ACCESS for shared mutexes.
CSecurityDesc* CreateLowIntegritySecurityDesc(ACCESS_MASK mask);
CSecurityDesc* CreateMediumIntegritySecurityDesc(ACCESS_MASK mask);

// For Vista or later, add the low integrity SACL to an existing CSecurityDesc.
HRESULT AddLowIntegritySaclToExistingDesc(CSecurityDesc* sd);

}  // namespace vista_util

}  // namespace omaha

#endif  // OMAHA_COMMON_VISTAUTIL_H__

