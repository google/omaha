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

#include "omaha/base/system_info.h"
#include "base/basictypes.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file_ver.h"
#include "omaha/base/logging.h"
#include "omaha/base/process.h"
#include "omaha/base/string.h"

namespace omaha {

bool SystemInfo::IsRunningOnXPSP2OrLater() {
  return ::IsWindowsXPSP2OrGreater();
}

bool SystemInfo::IsRunningOnXPSP3OrLater() {
  return ::IsWindowsXPSP3OrGreater();
}

bool SystemInfo::IsRunningOnW2K() {
  OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
  osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_WIN2K);
  osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_WIN2K);

  const DWORD type_mask = VER_MAJORVERSION | VER_MINORVERSION;
  ULONGLONG cond_mask =
      ::VerSetConditionMask(0UL, VER_MAJORVERSION, VER_EQUAL);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MINORVERSION, VER_EQUAL);

  // ::VerifyVersionInfo could return FALSE due to an error other than
  // ERROR_OLD_WIN_VERSION. We do not handle that case here.
  return !!::VerifyVersionInfo(&osvi, type_mask, cond_mask);
}

bool SystemInfo::IsRunningOnXPOrLater() {
  return ::IsWindowsXPOrGreater();
}

bool SystemInfo::IsRunningOnXPSP1OrLater() {
  return ::IsWindowsXPSP1OrGreater();
}


bool SystemInfo::IsRunningOnVistaOrLater() {
  return ::IsWindowsVistaOrGreater();
}

bool SystemInfo::IsRunningOnVistaRTM() {
  return ::IsWindowsVistaOrGreater() && !::IsWindowsVistaSP1OrGreater();
}

bool SystemInfo::IsRunningOnW81OrLater() {
  return ::IsWindows8Point1OrGreater();
}

CString SystemInfo::GetKernel32OSVersion() {
  const TCHAR kProductVersion[] = _T("ProductVersion");

  FileVer file_ver;
  if (!file_ver.Open(_T("kernel32.dll"))) {
    return CString();
  }

  return file_ver.QueryValue(kProductVersion);
}

bool SystemInfo::GetSystemVersion(int* major_version,
                                  int* minor_version,
                                  int* service_pack_major,
                                  int* service_pack_minor) {
  ASSERT1(major_version);
  ASSERT1(minor_version);
  ASSERT1(service_pack_major);
  ASSERT1(service_pack_minor);

  OSVERSIONINFOEX osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };

  // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
  // If that fails, try using the OSVERSIONINFO structure.
  const BOOL ver_info_exists =
      ::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi));
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

  return true;
}

DWORD SystemInfo::GetProcessorArchitecture() {
  static DWORD processor_architecture_cached(PROCESSOR_ARCHITECTURE_UNKNOWN);

  if (processor_architecture_cached == PROCESSOR_ARCHITECTURE_UNKNOWN) {
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

      processor_architecture_cached = sys_info.wProcessorArchitecture;
    } else {
      // If we couldn't get the _native_ system info, then we must be on OS
      // earlier than XP, so can't be 64-bit anyway. Assume Intel.
      processor_architecture_cached = PROCESSOR_ARCHITECTURE_INTEL;
    }
  }

  return processor_architecture_cached;
}

bool SystemInfo::Is64BitWindows() {
#if defined(_WIN64)
  return true;
#else
  return Process::IsWow64(::GetCurrentProcessId());
#endif
}

HRESULT SystemInfo::GetOSVersion(OSVERSIONINFOEX* os_out) {
  ASSERT1(os_out);

  scoped_library ntdll(::LoadLibrary(_T("ntdll.dll")));
  if (!ntdll) {
    return HRESULTFromLastError();
  }

  typedef LONG (WINAPI *RtlGetVersion_type)(OSVERSIONINFOEX*);
  RtlGetVersion_type RtlGetVersion_fn = reinterpret_cast<RtlGetVersion_type>(
      ::GetProcAddress(get(ntdll), "RtlGetVersion"));
  if (!RtlGetVersion_fn) {
    return HRESULTFromLastError();
  }

  ::ZeroMemory(os_out, sizeof(OSVERSIONINFOEX));
  os_out->dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  LONG result = RtlGetVersion_fn(os_out);
  if (!result) {
    return HRESULT_FROM_NT(result);
  }

  return S_OK;
}

bool SystemInfo::CompareOSVersions(OSVERSIONINFOEX* os, BYTE oper) {
  ASSERT1(oper != 0);
  ASSERT1(os);

  const DWORD type_mask = VER_MAJORVERSION | VER_MINORVERSION |
                          VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR;

  ULONGLONG cond_mask = 0;
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MAJORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MINORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMAJOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMINOR, oper);

  // ::VerifyVersionInfo could return FALSE due to an error other than
  // ERROR_OLD_WIN_VERSION. We do not handle that case here.
  return !!::VerifyVersionInfo(os, type_mask, cond_mask);
}

}  // namespace omaha
