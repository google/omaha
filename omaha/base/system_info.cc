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

#include <windows.h>

#include <map>

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

const TCHAR* const kArchAmd64 = _T("x64");
const TCHAR* const kArchIntel = _T("x86");
const TCHAR* const kArchArm64 = _T("arm64");
const TCHAR* const kArchUnknown = _T("unknown");

namespace {
// Returns the processor architecture as returned by `::GetNativeSystemInfo()`
// to detect the processor architecture of the installed operating system. See
// http://msdn.microsoft.com/en-us/library/ms724340.aspx.
//
// If the function is called from an x86 or x64 application running on a 64-bit
// system that does not have an Intel64 or x64 processor (such as ARM64), it
// will return information as if the system is x86 only if x86 emulation is
// supported (or x64 if x64 emulation is also supported).
DWORD GetProcessorArchitecture() {
  static DWORD processor_architecture_cached(PROCESSOR_ARCHITECTURE_UNKNOWN);

  if (processor_architecture_cached == PROCESSOR_ARCHITECTURE_UNKNOWN) {
    typedef void(WINAPI * GetSystemInfoFunc)(LPSYSTEM_INFO);

    HMODULE handle = ::GetModuleHandle(_T("kernel32"));
    ASSERT1(handle);
    GetSystemInfoFunc get_native_system_info =
        reinterpret_cast<GetSystemInfoFunc>(
            ::GetProcAddress(handle, "GetNativeSystemInfo"));

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

// Returns a string representation of the processor architecture if one exists,
// or an empty string if there is no match.
CString GetProcessorArchitectureString() {
  switch (GetProcessorArchitecture()) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      return kArchIntel;

    case PROCESSOR_ARCHITECTURE_AMD64:
      return kArchAmd64;

    default:
      ASSERT1(false);
      return kArchUnknown;
  }
}

// Returns the native architecture if the current process is running under
// WOW64 and `::IsWow64Process2()` is available. Otherwise returns
// `IMAGE_FILE_MACHINE_UNKNOWN`.
USHORT GetNativeArchitecture() {
  typedef BOOL(WINAPI * IsWow64Process2Func)(HANDLE, USHORT*, USHORT*);
  const IsWow64Process2Func is_wow64_process2 =
      reinterpret_cast<IsWow64Process2Func>(::GetProcAddress(
          ::GetModuleHandle(_T("kernel32.dll")), "IsWow64Process2"));
  if (!is_wow64_process2) {
    return IMAGE_FILE_MACHINE_UNKNOWN;
  }

  USHORT process_machine = 0;
  USHORT native_architecture = IMAGE_FILE_MACHINE_UNKNOWN;
  return is_wow64_process2(::GetCurrentProcess(), &process_machine,
                           &native_architecture)
             ? native_architecture
             : IMAGE_FILE_MACHINE_UNKNOWN;
}

// Returns a string representation of the native architecture if one exists,
// or an empty string if there is no match.
CString GetNativeArchitectureString() {
  const std::map<int, CString> kNativeArchitectureImagesToStrings = {
      {IMAGE_FILE_MACHINE_I386, kArchIntel},
      {IMAGE_FILE_MACHINE_AMD64, kArchAmd64},
      {IMAGE_FILE_MACHINE_ARM64, kArchArm64},
  };

  const auto native_arch =
      kNativeArchitectureImagesToStrings.find(GetNativeArchitecture());
  return native_arch != kNativeArchitectureImagesToStrings.end()
             ? native_arch->second
             : CString(_T(""));
}

}  // namespace

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

CString SystemInfo::GetArchitecture() {
  const CString native_arch = GetNativeArchitectureString();
  return !native_arch.IsEmpty() ? native_arch
                                : GetProcessorArchitectureString();
}

bool SystemInfo::IsArchitectureSupported(const CString& arch) {
  if (arch.IsEmpty()) {
    return true;
  }

  const CString current_arch = GetArchitecture().MakeLower();
  if (arch == current_arch) {
    return true;
  }

  typedef HRESULT(WINAPI * IsWow64GuestMachineSupportedFunc)(USHORT, BOOL*);
  const IsWow64GuestMachineSupportedFunc is_wow64_guest_machine_supported =
      reinterpret_cast<IsWow64GuestMachineSupportedFunc>(
          ::GetProcAddress(::GetModuleHandle(_T("kernel32.dll")),
                           "IsWow64GuestMachineSupported"));

  if (is_wow64_guest_machine_supported) {
    const std::map<CString, int> kNativeArchitectureStringsToImages = {
        {kArchIntel, IMAGE_FILE_MACHINE_I386},
        {kArchAmd64, IMAGE_FILE_MACHINE_AMD64},
        {kArchArm64, IMAGE_FILE_MACHINE_ARM64},
    };

    const auto image = kNativeArchitectureStringsToImages.find(arch);
    if (image != kNativeArchitectureStringsToImages.end()) {
      BOOL is_machine_supported = false;
      if (SUCCEEDED(is_wow64_guest_machine_supported(
              static_cast<USHORT>(image->second), &is_machine_supported))) {
        return is_machine_supported;
      }
    }
  }

  return arch == kArchIntel;
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
        SystemInfo::GetArchitecture() == kArchAmd64) {
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
