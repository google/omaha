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

#ifndef OMAHA_COMMON_SYSTEM_INFO_H__
#define OMAHA_COMMON_SYSTEM_INFO_H__

#include <windows.h>
#include <tchar.h>

namespace omaha {

// TODO(omaha): refactor to use a namespace.
class SystemInfo {
 public:
  // Find out if the OS is at least Windows 2000
  // Service pack 4. If OS version is less than that
  // will return false, all other cases true.
  static bool OSWin2KSP4OrLater() {
    // Use GetVersionEx to get OS and Service Pack information.
    OSVERSIONINFOEX osviex;
    ::ZeroMemory(&osviex, sizeof(osviex));
    osviex.dwOSVersionInfoSize = sizeof(osviex);
    BOOL success = ::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osviex));
    // If this failed we're on Win9X or a pre NT4SP6 OS.
    if (!success) {
      return false;
    }

    if (osviex.dwMajorVersion < 5) {
      return false;
    }
    if (osviex.dwMajorVersion > 5) {
      return true;    // way beyond Windows XP.
    }
    if (osviex.dwMinorVersion >= 1) {
      return true;    // Windows XP or better.
    }
    if (osviex.wServicePackMajor >= 4) {
      return true;    // Windows 2000 SP4.
    }

    return false;     // Windows 2000, < SP4.
  }

  // Returns true if the OS is at least XP SP2.
  static bool OSWinXPSP2OrLater();

  // CategorizeOS returns a categorization of what operating system is running,
  // and the service pack level.
  // NOTE: Please keep this in the order of increasing OS versions
  enum OSVersionType {
    OS_WINDOWS_UNKNOWN = 1,
    OS_WINDOWS_9X_OR_NT,
    OS_WINDOWS_2000,
    OS_WINDOWS_XP,
    OS_WINDOWS_SERVER_2003,
    OS_WINDOWS_VISTA,
    OS_WINDOWS_7
  };
  static HRESULT CategorizeOS(OSVersionType* os_version, DWORD* service_pack);
  static const wchar_t* OSVersionTypeAsString(OSVersionType t);

  // Returns true if the current operating system is Windows 2000.
  static bool IsRunningOnW2K();

  // Are we running on Windows XP or later.
  static bool IsRunningOnXPOrLater();

  // Are we running on Windows XP SP1 or later.
  static bool IsRunningOnXPSP1OrLater();

  // Are we running on Windows Vista or later.
  static bool IsRunningOnVistaOrLater();

  static bool IsRunningOnVistaRTM();

  // Returns the version and the name of the operating system.
  static bool GetSystemVersion(int* major_version,
                               int* minor_version,
                               int* service_pack_major,
                               int* service_pack_minor,
                               TCHAR* name_buf,
                               size_t name_buf_len);

  // Returns whether this is a 64-bit system.
  static bool IsRunningOn64Bit();
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SYSTEM_INFO_H__

