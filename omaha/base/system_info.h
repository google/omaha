// Copyright 2004-2009 Google Inc.
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

// TODO(omaha): this code should be updated according to code published by
// Microsoft at http://msdn.microsoft.com/en-us/library/ms724429(VS.85).aspx.
// We need a more rigorous clasification of versions.

#ifndef OMAHA_BASE_SYSTEM_INFO_H_
#define OMAHA_BASE_SYSTEM_INFO_H_

#include <windows.h>
#include <versionhelpers.h>
#include <tchar.h>
#include <atlstr.h>

namespace omaha {

// TODO(omaha): refactor to use a namespace.
class SystemInfo {
 public:
  // Find out if the OS is at least Windows 2000
  // Service pack 4. If OS version is less than that
  // will return false, all other cases true.
  static bool OSWin2KSP4OrLater() {
    return ::IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN2K),
                                       LOBYTE(_WIN32_WINNT_WIN2K),
                                       4);
  }

  // Returns true if the OS is at least XP SP2.
  static bool IsRunningOnXPSP2OrLater();

  // Returns true if the OS is at least XP SP3.
  static bool IsRunningOnXPSP3OrLater();

  // Returns true if the current operating system is Windows 2000.
  static bool IsRunningOnW2K();

  // Are we running on Windows XP or later.
  static bool IsRunningOnXPOrLater();

  // Are we running on Windows XP SP1 or later.
  static bool IsRunningOnXPSP1OrLater();

  // Are we running on Windows Vista or later.
  static bool IsRunningOnVistaOrLater();

  static bool IsRunningOnVistaRTM();

  static bool IsRunningOnW81OrLater();

  static CString GetKernel32OSVersion();

  // Returns the version of the operating system.
  static bool GetSystemVersion(int* major_version,
                               int* minor_version,
                               int* service_pack_major,
                               int* service_pack_minor);

  // Returns the processor architecture. We use wProcessorArchitecture in
  // SYSTEM_INFO returned by ::GetNativeSystemInfo() to detect the processor
  // architecture of the installed operating system. Note the "Native" in the
  // function name - this is important. See
  // http://msdn.microsoft.com/en-us/library/ms724340.aspx.
  static DWORD GetProcessorArchitecture();

  // Returns whether this is a 64-bit Windows system.
  static bool Is64BitWindows();

  // Retrieves a full OSVERSIONINFOEX struct describing the current OS.
  static HRESULT GetOSVersion(OSVERSIONINFOEX* os_out);

  // Compares the current OS to the supplied version.  The value of |oper|
  // should be one of the predicate values from VerSetConditionMask() -- for
  // example, VER_GREATER or VER_GREATER_EQUAL.
  //
  // The current OS will always be on the left hand side of the comparison;
  // for example, if |oper| was VER_GREATER and |os| is set to Windows Vista,
  // a machine running Windows 7 or later yields true.
  static bool CompareOSVersions(OSVERSIONINFOEX* os, BYTE oper);

 private:
  static bool CompareOSVersionsInternal(OSVERSIONINFOEX* os,
                                        DWORD type_mask,
                                        BYTE oper);
};

}  // namespace omaha

#endif  // OMAHA_BASE_SYSTEM_INFO_H_

