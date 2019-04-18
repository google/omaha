// Copyright 2006-2010 Google Inc.
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

#ifndef OMAHA_BASE_VISTAUTIL_H_
#define OMAHA_BASE_VISTAUTIL_H_

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

// Sets is_uac_on to true if the user has a split token, or the explorer process
// for the logged on user or session is running at medium integrity.
HRESULT IsUACOn(bool* is_uac_on);

// Returns true if running at High integrity with UAC on.
HRESULT IsElevatedWithUACOn(bool* is_elevated_with_uac_on);

// Returns true if the EnableLUA key does not exist (defaults to 1) or is set to
// 1. EnableLUA is only effective (in turning on/off UAC) after a reboot, so the
// value there may not reflect the exact state of the running machine. So this
// function needs to be used with care.
bool IsEnableLUAOn();

// Returns true if running at High integrity with the EnableLUA key set to 1.
bool IsElevatedWithEnableLUAOn();

// Returns true if the process is running under credentials of an user
// belonging to the admin group in case of pre-Vista and in case Vista
// returns true if the user is running as an elevated admin.
bool IsUserAdmin();

// Returns true if the user is running as a non-elevated admin in case of
// Vista. In case of XP always returns false.
bool IsUserNonElevatedAdmin();

// Starts a new elevated process. file_path specifies the program to be run.
// If exit_code is not null, the function waits until the spawned process has
// completed. The exit code of the process is returned therein.
// If exit_code is null, the function will return after spawning the program
// and will not wait for completion.
// show_window is one of the SW_* constants to specify how the window is
// opened.
HRESULT RunElevated(const TCHAR* file_path, const TCHAR* parameters,
    int show_window, DWORD* exit_code);

// For Vista or later, add the mandatory SACL to an existing CSecurityDesc.
HRESULT SetMandatorySacl(MANDATORY_LEVEL level, CSecurityDesc* sd);

// On Vista or later, enables metadata protection in the heap manager.  This
// causes a process to be terminated immediately when a buffer overflow or
// certain illegal heap operations are detected.  This call enables protection
// for the entire process and cannot be reversed.
HRESULT EnableProcessHeapMetadataProtection();

}  // namespace vista_util

}  // namespace omaha

#endif  // OMAHA_BASE_VISTAUTIL_H_

