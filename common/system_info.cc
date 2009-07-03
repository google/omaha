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

#include "omaha/common/system_info.h"
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"

namespace omaha {

bool SystemInfo::OSWinXPSP2OrLater() {
  OSVersionType os_type(OS_WINDOWS_UNKNOWN);
  DWORD sp(0);

  HRESULT hr = CategorizeOS(&os_type, &sp);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[CategorizeOS failed][0x%x]"), hr));
    return false;
  }

  return ((os_type == SystemInfo::OS_WINDOWS_XP && sp >= 2) ||
          os_type > SystemInfo::OS_WINDOWS_XP);
}

bool SystemInfo::IsRunningOnW2K() {
  OSVERSIONINFO os_info = {0};
  os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if (!::GetVersionEx(&os_info)) {
    ASSERT(false, (L"GetVersionEx"));
    return false;
  }

  return os_info.dwMajorVersion == 5 && os_info.dwMinorVersion == 0;
}

bool SystemInfo::IsRunningOnXPOrLater() {
  OSVersionType os_type(OS_WINDOWS_UNKNOWN);

  HRESULT hr = CategorizeOS(&os_type, NULL);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[Failed to get os type][0x%x]"), hr));
    return false;
  }

  return os_type >= SystemInfo::OS_WINDOWS_XP;
}

bool SystemInfo::IsRunningOnXPSP1OrLater() {
  OSVersionType os_type(OS_WINDOWS_UNKNOWN);
  DWORD sp(0);

  HRESULT hr = CategorizeOS(&os_type, &sp);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[Failed to get os type][0x%x]"), hr));
    return false;
  }

  return ((os_type == SystemInfo::OS_WINDOWS_XP && sp >= 1) ||
          os_type > SystemInfo::OS_WINDOWS_XP);
}


bool SystemInfo::IsRunningOnVistaOrLater() {
  OSVersionType os_type(OS_WINDOWS_UNKNOWN);
  DWORD sp(0);

  HRESULT hr = CategorizeOS(&os_type, &sp);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[Failed to get os type][0x%x]"), hr));
    return false;
  }

  return (os_type >= OS_WINDOWS_VISTA);
}

bool SystemInfo::IsRunningOnVistaRTM() {
  OSVersionType os_type(OS_WINDOWS_UNKNOWN);
  DWORD sp(0);

  HRESULT hr = CategorizeOS(&os_type, &sp);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[Failed to get os type][0x%x]"), hr));
    return false;
  }

  return (os_type == SystemInfo::OS_WINDOWS_VISTA && sp == 0);
}

HRESULT SystemInfo::CategorizeOS(OSVersionType* os_ver, DWORD* sp) {
  static OSVersionType os_ver_cached(OS_WINDOWS_UNKNOWN);
  // Hopefully, Windows doesn't release a SP that's kUint32Max.
  static DWORD sp_cached(kUint32Max);

  ASSERT(os_ver, (L""));

  if (sp) {
    *sp = 0;
  }

  if (os_ver_cached == OS_WINDOWS_UNKNOWN || sp_cached == kUint32Max) {
    // Use GetVersionEx to get OS and Service Pack information.
    OSVERSIONINFOEX osviex;
    ::ZeroMemory(&osviex, sizeof(OSVERSIONINFOEX));
    osviex.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    BOOL r = ::GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osviex));

    // If ::GetVersionEx fails when given an OSVERSIONINFOEX then we're running
    // on NT4.0SP5 or earlier.
    if (!r) {
      os_ver_cached = OS_WINDOWS_9X_OR_NT;
    } else {
      switch (osviex.dwPlatformId) {
        case VER_PLATFORM_WIN32_NT:
          // Windows 7 beta 1 reports the same major version as Vista does.
          if (osviex.dwMajorVersion == 6 && osviex.dwMinorVersion == 1) {
            os_ver_cached = OS_WINDOWS_7;
          } else if (osviex.dwMajorVersion == 6 && osviex.dwMinorVersion == 0) {
            os_ver_cached = OS_WINDOWS_VISTA;
          } else if (osviex.dwMajorVersion == 5 && osviex.dwMinorVersion == 2) {
            os_ver_cached = OS_WINDOWS_SERVER_2003;
          } else if (osviex.dwMajorVersion == 5 && osviex.dwMinorVersion == 1) {
            os_ver_cached = OS_WINDOWS_XP;
          } else if (osviex.dwMajorVersion == 5 && osviex.dwMinorVersion == 0) {
            os_ver_cached = OS_WINDOWS_2000;
          } else if (osviex.dwMajorVersion <= 4) {
            os_ver_cached = OS_WINDOWS_9X_OR_NT;
            break;
          } else {
            os_ver_cached = OS_WINDOWS_UNKNOWN;
            break;
          }
          sp_cached = osviex.wServicePackMajor;
          break;

        case VER_PLATFORM_WIN32_WINDOWS:
        case VER_PLATFORM_WIN32s:
        default:
          os_ver_cached = OS_WINDOWS_9X_OR_NT;
          break;
      }
    }

    UTIL_LOG(L1, (L"[CategorizeOS][version %s][service pack %d]",
                  OSVersionTypeAsString(os_ver_cached),
                  sp_cached));
  }

  ASSERT1(os_ver_cached != OS_WINDOWS_UNKNOWN && sp_cached != kUint32Max);

  *os_ver = os_ver_cached;
  if (sp) {
    *sp = sp_cached;
  }

  return S_OK;
}

const wchar_t* SystemInfo::OSVersionTypeAsString(OSVersionType t) {
  switch (t) {
    case OS_WINDOWS_9X_OR_NT:    return L"OS_WINDOWS_9X_OR_NT";
    case OS_WINDOWS_2000:        return L"OS_WINDOWS_2000";
    case OS_WINDOWS_XP:          return L"OS_WINDOWS_XP";
    case OS_WINDOWS_SERVER_2003: return L"OS_WINDOWS_SERVER_2003";
    case OS_WINDOWS_UNKNOWN:     return L"OS_WINDOWS_UNKNOWN";
    case OS_WINDOWS_VISTA:       return L"OS_WINDOWS_VISTA";
    case OS_WINDOWS_7:           return L"OS_WINDOWS_7";
    default:                     return L"<unknown>";
  }
}

// The following code which names the operating system comes from MSDN article
// "Getting the System Version"
#define kNullChar (_T('\0'))
bool SystemInfo::GetSystemVersion(int* major_version,
                                  int* minor_version,
                                  int* service_pack_major,
                                  int* service_pack_minor,
                                  TCHAR*  name_buf,
                                  size_t name_buf_len) {
  ASSERT1(major_version);
  ASSERT1(minor_version);
  ASSERT1(service_pack_major);
  ASSERT1(service_pack_minor);
  ASSERT1(name_buf);
  ASSERT1(0 < name_buf_len);

  // Clear the name to start with.
  name_buf[0] = kNullChar;

  DWORD buf_len = MAX_PATH;
  TCHAR buffer[MAX_PATH];
  TCHAR format_buffer[64];

  buffer[0] = kNullChar;

  OSVERSIONINFOEX osvi;
  BOOL ver_info_exists;

  // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
  // If that fails, try using the OSVERSIONINFO structure.
  ::ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  ver_info_exists = ::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi));
  if (!ver_info_exists) {
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi))) {
      return false;
    }
  }

  *major_version      = osvi.dwMajorVersion;
  *minor_version      = osvi.dwMinorVersion;
  *service_pack_major = osvi.wServicePackMajor;
  *service_pack_minor = osvi.wServicePackMinor;

  switch (osvi.dwPlatformId) {
    // Test for the Windows NT product family.
    case VER_PLATFORM_WIN32_NT:

      // Test for the specific product family.
      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2) {
        SafeStrCat(buffer,
                   _T("Microsoft Windows Server 2003 family, "),
                   buf_len);
      }

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1) {
        SafeStrCat(buffer, _T("Microsoft Windows XP "), buf_len);
      }

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) {
        SafeStrCat(buffer, _T("Microsoft Windows 2000 "), buf_len);
      }

      if (osvi.dwMajorVersion <= 4) {
        SafeStrCat(buffer, _T("Microsoft Windows NT "), buf_len);
      }

      // Test for specific product on Windows NT 4.0 SP6 and later.
      if (ver_info_exists) {
        // Test for the workstation type.
        if (osvi.wProductType == VER_NT_WORKSTATION) {
          if (osvi.dwMajorVersion == 4) {
            SafeStrCat(buffer, _T("Workstation 4.0 "), buf_len);
          } else if (osvi.wSuiteMask & VER_SUITE_PERSONAL) {
            SafeStrCat(buffer, _T("Home Edition "), buf_len);
          } else {
            SafeStrCat(buffer, _T("Professional "), buf_len);
          }
        } else if (osvi.wProductType == VER_NT_SERVER ||
                   osvi.wProductType == VER_NT_DOMAIN_CONTROLLER) {
          // server type.
          if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2) {
            if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
              SafeStrCat(buffer, _T("Datacenter Edition "), buf_len);
            } else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
              SafeStrCat(buffer, _T("Enterprise Edition "), buf_len);
            } else if (osvi.wSuiteMask == VER_SUITE_BLADE) {
              SafeStrCat(buffer, _T("Web Edition "), buf_len);
            } else {
              SafeStrCat(buffer, _T("Standard Edition "), buf_len);
            }
          } else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) {
            if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
              SafeStrCat(buffer, _T("Datacenter Server "), buf_len);
            } else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
              SafeStrCat(buffer, _T("Advanced Server "), buf_len);
            } else {
              SafeStrCat(buffer, _T("Server "), buf_len);
            }
          } else {
            // Windows NT 4.0.
            if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
              SafeStrCat(buffer,
                         _T("Server 4.0, Enterprise Edition "),
                         buf_len);
            } else {
              SafeStrCat(buffer, _T("Server 4.0 "), buf_len);
            }
          }
        }
      } else {
        // Test for specific product on Windows NT 4.0 SP5 and earlier.
        HKEY hKey;
        TCHAR product_type[64] = {0};
        DWORD dwBufLen = arraysize(product_type);
        LONG lRet;

        // TODO(omaha): should we use the RegKey API for consistency.
        lRet = ::RegOpenKeyEx(
                   HKEY_LOCAL_MACHINE,
                   _T("SYSTEM\\CurrentControlSet\\Control\\ProductOptions"),
                   0,
                   KEY_QUERY_VALUE,
                   &hKey);
        if (lRet != ERROR_SUCCESS) {
          return false;
        }

        lRet = ::RegQueryValueEx(hKey,
                                 _T("ProductType"),
                                 NULL,
                                 NULL,
                                 reinterpret_cast<byte *>(product_type),
                                 &dwBufLen);
        if ((lRet != ERROR_SUCCESS) || (dwBufLen > arraysize(product_type))) {
          return false;
        }

        ::RegCloseKey(hKey);

        if (::lstrcmpi(_T("WINNT"), product_type) == 0) {
          SafeStrCat(buffer, _T("Workstation "), buf_len);
        }
        if (::lstrcmpi(_T("LANMANNT"), product_type) == 0) {
          SafeStrCat(buffer, _T("Server "), buf_len);
        }
        if (::lstrcmpi(_T("SERVERNT"), product_type) == 0) {
          SafeStrCat(buffer, _T("Advanced Server "), buf_len);
        }

        ::wsprintf(format_buffer,
                   _T("%d.%d "),
                   osvi.dwMajorVersion,
                   osvi.dwMinorVersion);
        SafeStrCat(buffer, format_buffer, buf_len);
      }

      // Display service pack (if any) and build number.
      if (osvi.dwMajorVersion == 4 &&
          ::lstrcmpi(osvi.szCSDVersion, _T("Service Pack 6")) == 0) {
        HKEY hKey;
        LONG lRet;

        // Test for SP6 versus SP6a.
        lRet = ::RegOpenKeyEx(
                   HKEY_LOCAL_MACHINE,
                   _T("SOFTWARE\\Microsoft\\Windows NT\\")
                       _T("CurrentVersion\\Hotfix\\Q246009"),
                   0,
                   KEY_QUERY_VALUE,
                   &hKey);
        if (lRet == ERROR_SUCCESS) {
          ::wsprintf(format_buffer,
                     _T("Service Pack 6a (Build %d)"),
                     osvi.dwBuildNumber & 0xFFFF);
          SafeStrCat(buffer, format_buffer, buf_len);
        } else {
          // Windows NT 4.0 prior to SP6a.
          ::wsprintf(format_buffer, _T("%s (Build %d)"),
                      osvi.szCSDVersion,
                      osvi.dwBuildNumber & 0xFFFF);
          SafeStrCat(buffer, format_buffer, buf_len);
        }
        ::RegCloseKey(hKey);
      } else {
        // Windows NT 3.51 and earlier or Windows 2000 and later.
        ::wsprintf(format_buffer,
                   _T("%s (Build %d)"),
                   osvi.szCSDVersion,
                   osvi.dwBuildNumber & 0xFFFF);
        SafeStrCat(buffer, format_buffer, buf_len);
      }

      break;

      // Test for the Windows 95 product family.
    case VER_PLATFORM_WIN32_WINDOWS:

      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0) {
        SafeStrCat(buffer, _T("Microsoft Windows 95 "), buf_len);
        if (osvi.szCSDVersion[1] == 'C' || osvi.szCSDVersion[1] == 'B') {
          SafeStrCat(buffer, _T("OSR2 "), buf_len);
        }
      }

      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10) {
        SafeStrCat(buffer, _T("Microsoft Windows 98 "), buf_len);
        if (osvi.szCSDVersion[1] == 'A') {
          SafeStrCat(buffer, _T("SE "), buf_len);
        }
      }

      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90) {
        SafeStrCat(buffer,
                   _T("Microsoft Windows Millennium Edition"),
                   buf_len);
      }
      break;

    case VER_PLATFORM_WIN32s:

      SafeStrCat(buffer, _T("Microsoft Win32s"), buf_len);
      break;

  default:
    SafeStrCat(buffer, _T("Unknown operating system"), buf_len);
    break;
  }
  // SKIP_LOC_END

  // Remove trailing space, if any.
  DWORD buffer_len = ::lstrlen(buffer);
  if (buffer[buffer_len-1] == kNullChar) {
    buffer[buffer_len-1] = kNullChar;
  }

  // Copy to destination argument.
  String_StrNCpy(name_buf, buffer, name_buf_len);

  return true;
}


bool SystemInfo::IsRunningOn64Bit() {
  static DWORD is64_cached(kUint32Max);

  if (is64_cached == kUint32Max) {
    typedef void (WINAPI * GetSystemInfoFunc)(LPSYSTEM_INFO);

    HMODULE handle = ::GetModuleHandle(_T("kernel32"));
    ASSERT1(handle);
    GetSystemInfoFunc get_native_system_info =
        reinterpret_cast<GetSystemInfoFunc>(::GetProcAddress(
                                                handle,
                                                "GetNativeSystemInfo"));

    if (get_native_system_info != NULL) {
      SYSTEM_INFO sys_info = {0};

      get_native_system_info(&sys_info);

      is64_cached =
          sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
    } else {
      // If we couldn't get the _native_ system info, then we must be on OS
      // earlier than XP, so can't be 64-bit anyway.
      is64_cached = 0;
    }
  }

  return is64_cached != 0;
}


}  // namespace omaha

