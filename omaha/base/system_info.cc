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
#include "omaha/base/utils.h"
#include "omaha/base/wmi_query.h"

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

  scoped_library ntdll(LoadSystemLibrary(_T("ntdll.dll")));
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

// Keep this code in sync with Chromium's  // base/win/windows_version.cc.
VersionType SystemInfo::GetOSVersionType() {
  OSVERSIONINFOEX version_info = {sizeof(version_info)};
  if (!::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info))) {
    // Windows version is XP SP1 or older, so we return a safe default.
    return SUITE_HOME;
  }

  if (version_info.dwMajorVersion == 6 || version_info.dwMajorVersion == 10) {
    DWORD os_type = 0;
    if (!GetProductInfoWrap(version_info.dwMajorVersion,
                            version_info.dwMinorVersion,
                            0,
                            0,
                            &os_type)) {
      // We return a safe default.
      return SUITE_HOME;
    }

    switch (os_type) {
      case PRODUCT_CLUSTER_SERVER:
      case PRODUCT_DATACENTER_SERVER:
      case PRODUCT_DATACENTER_SERVER_CORE:
      case PRODUCT_ENTERPRISE_SERVER:
      case PRODUCT_ENTERPRISE_SERVER_CORE:
      case PRODUCT_ENTERPRISE_SERVER_IA64:
      case PRODUCT_SMALLBUSINESS_SERVER:
      case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
      case PRODUCT_STANDARD_SERVER:
      case PRODUCT_STANDARD_SERVER_CORE:
      case PRODUCT_WEB_SERVER:
        return SUITE_SERVER;

      case PRODUCT_PROFESSIONAL:
      case PRODUCT_ULTIMATE:
        return SUITE_PROFESSIONAL;

      case PRODUCT_ENTERPRISE:
      case PRODUCT_ENTERPRISE_E:
      case PRODUCT_ENTERPRISE_EVALUATION:
      case PRODUCT_ENTERPRISE_N:
      case PRODUCT_ENTERPRISE_N_EVALUATION:
      case PRODUCT_ENTERPRISE_S:
      case PRODUCT_ENTERPRISE_S_EVALUATION:
      case PRODUCT_ENTERPRISE_S_N:
      case PRODUCT_ENTERPRISE_S_N_EVALUATION:
      case PRODUCT_BUSINESS:
      case PRODUCT_BUSINESS_N:
        return SUITE_ENTERPRISE;

      case PRODUCT_EDUCATION:
      case PRODUCT_EDUCATION_N:
        return SUITE_EDUCATION;

      case PRODUCT_HOME_BASIC:
      case PRODUCT_HOME_PREMIUM:
      case PRODUCT_STARTER:
      default:
        return SUITE_HOME;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 2) {
    if (version_info.wProductType == VER_NT_WORKSTATION &&
        SystemInfo::GetProcessorArchitecture() ==
        PROCESSOR_ARCHITECTURE_AMD64) {
      return SUITE_PROFESSIONAL;
    } else if (version_info.wSuiteMask & VER_SUITE_WH_SERVER) {
      return SUITE_HOME;
    } else {
      return SUITE_SERVER;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 1) {
    if (version_info.wSuiteMask & VER_SUITE_PERSONAL)
      return SUITE_HOME;
    else
      return SUITE_PROFESSIONAL;
  } else {
    // Windows is pre XP so we don't care but pick a safe default.
    return SUITE_HOME;
  }
}

bool SystemInfo::CompareOSVersionsInternal(OSVERSIONINFOEX* os,
                                           DWORD type_mask,
                                           BYTE oper) {
  ASSERT1(os);
  ASSERT1(type_mask != 0);
  ASSERT1(oper != 0);

  ULONGLONG cond_mask = 0;
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MAJORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MINORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMAJOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMINOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_BUILDNUMBER, oper);

  // ::VerifyVersionInfo could return FALSE due to an error other than
  // ERROR_OLD_WIN_VERSION. We do not handle that case here.
  // VerifyVersionInfo is documented here: https://msdn.microsoft.com/ms725492.
  return !!::VerifyVersionInfo(os, type_mask, cond_mask);
}

bool SystemInfo::CompareOSVersions(OSVERSIONINFOEX* os, BYTE oper) {
  ASSERT1(os);
  ASSERT1(oper != 0);

  const DWORD os_sp_type_mask = VER_MAJORVERSION | VER_MINORVERSION |
                                VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR;
  const DWORD build_number_type_mask = VER_BUILDNUMBER;

  // If the OS and the SP match, return the build number comparison.
  return CompareOSVersionsInternal(os, os_sp_type_mask, VER_EQUAL) ?
         CompareOSVersionsInternal(os, build_number_type_mask, oper) :
         CompareOSVersionsInternal(os, os_sp_type_mask, oper);
}

CString SystemInfo::GetSerialNumber() {
  const TCHAR kWmiLocal[]            = _T("ROOT\\CIMV2");
  const TCHAR kWmiQueryBios[]        =
      _T("SELECT SerialNumber FROM Win32_Bios");
  const TCHAR kWmiPropSerialNumber[] = _T("SerialNumber");

  CString serial_number;
  WmiQuery wmi_query;
  HRESULT hr = wmi_query.Connect(kWmiLocal);
  if (FAILED(hr)) {
    return CString();
  }
  hr = wmi_query.Query(kWmiQueryBios);
  if (FAILED(hr)) {
    return CString();
  }
  if (wmi_query.AtEnd()) {
    return CString();
  }
  hr = wmi_query.GetValue(kWmiPropSerialNumber, &serial_number);
  if (FAILED(hr)) {
    return CString();
  }

  return serial_number;
}

}  // namespace omaha
