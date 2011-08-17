// Copyright 2003-2010 Google Inc.
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

#include "omaha/base/app_util.h"
#include <shlwapi.h>
#include <atlsecurity.h>
#include <vector>
#include "omaha/base/cgi.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/file_ver.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"

namespace omaha {

namespace app_util {

HMODULE GetModuleHandleFromAddress(void* address) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  DWORD result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  ASSERT1(result == sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}

HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(GetCurrentModuleHandle);
}


CString GetModulePath(HMODULE module_handle) {
  ASSERT1(IsModuleHandleValid(module_handle));
  CString mod_path;

  DWORD result = ::GetModuleFileName(module_handle,
                                     mod_path.GetBufferSetLength(MAX_PATH),
                                     MAX_PATH);
  mod_path.ReleaseBuffer();
  ASSERT1(result == static_cast<DWORD>(mod_path.GetLength()));

  return mod_path;
}

CString GetModuleDirectory(HMODULE module_handle) {
  ASSERT1(IsModuleHandleValid(module_handle));
  return GetDirectoryFromPath(GetModulePath(module_handle));
}

CString GetModuleName(HMODULE module_handle) {
  ASSERT1(IsModuleHandleValid(module_handle));
  CString app_name(GetFileFromPath(GetModulePath(module_handle)));

  UTIL_LOG(L5, (_T("[GetModuleName][module 0x%08x][path '%s'][name '%s']"),
                module_handle, GetModulePath(module_handle), app_name));

  return app_name;
}

CString GetModuleNameWithoutExtension(HMODULE module_handle) {
  ASSERT1(IsModuleHandleValid(module_handle));
  CString module_name(GetPathRemoveExtension(GetModuleName(module_handle)));

  UTIL_LOG(L5, (_T("[GetModuleNameWithoutExtension]")
                _T("[module 0x%08x][module '%s'][name '%s']"),
                module_handle, GetModulePath(module_handle), module_name));

  return module_name;
}

CString GetCurrentModulePath() {
  return GetModulePath(GetCurrentModuleHandle());
}

CString GetCurrentModuleDirectory() {
  return GetModuleDirectory(GetCurrentModuleHandle());
}

CString GetCurrentModuleName() {
  return GetModuleName(GetCurrentModuleHandle());
}

CString GetCurrentModuleNameWithoutExtension() {
  return GetModuleNameWithoutExtension(GetCurrentModuleHandle());
}

bool IsAddressInCurrentModule(void* address) {
  return GetCurrentModuleHandle() == GetModuleHandleFromAddress(address);
}

CString GetHostName() {
  CString hostName;
  DWORD name_len = MAX_COMPUTERNAME_LENGTH + 1;
  bool result = !!::GetComputerName(hostName.GetBufferSetLength(name_len),
                                    &name_len);
  ASSERT1(result);
  hostName.ReleaseBuffer();
  ASSERT1(name_len == static_cast<DWORD>(hostName.GetLength()));

  return hostName;
}

CString GetWindowsDir() {
  CString windows_path;

  DWORD result = ::GetWindowsDirectory(
      windows_path.GetBufferSetLength(MAX_PATH), MAX_PATH);
  windows_path.ReleaseBuffer();
  ASSERT1(result == static_cast<DWORD>(windows_path.GetLength()));

  return windows_path;
}

CString GetSystemDir() {
  CString systemPath;

  DWORD result = ::GetSystemDirectory(systemPath.GetBufferSetLength(MAX_PATH),
                                      MAX_PATH);
  systemPath.ReleaseBuffer();
  ASSERT1(result == static_cast<DWORD>(systemPath.GetLength()));

  return systemPath;
}

CString GetTempDir() {
  CString tempPath;

  DWORD result = ::GetTempPath(MAX_PATH, tempPath.GetBufferSetLength(MAX_PATH));
  tempPath.ReleaseBuffer();
  ASSERT1(result == static_cast<DWORD>(tempPath.GetLength()));

  return tempPath;
}

bool IsModuleHandleValid(HMODULE module_handle) {
  if (!module_handle) {
    return true;
  }
  return module_handle == GetModuleHandleFromAddress(module_handle);
}

DWORD DllGetVersion(const CString& dll_path)  {
  HINSTANCE hInst = ::GetModuleHandle(dll_path);
  ASSERT(hInst,
         (_T("[GetModuleHandle failed][%s][%d]"), dll_path, ::GetLastError()));
  DWORD dwVersion = 0;
  DLLGETVERSIONPROC pfn = reinterpret_cast<DLLGETVERSIONPROC>(
                              ::GetProcAddress(hInst, "DllGetVersion"));
  if (pfn != NULL) {
    DLLVERSIONINFO dvi = {0};
    dvi.cbSize = sizeof(dvi);
    HRESULT hr = (*pfn)(&dvi);
    if (SUCCEEDED(hr)) {
      // Since we're fitting both the major and minor versions into a DWORD,
      // let's sanity check that we're not in an overflow situation here
      ASSERT1(dvi.dwMajorVersion <= 0xFFFF);
      ASSERT1(dvi.dwMinorVersion <= 0xFFFF);
      dwVersion = MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
    }
  }
  return dwVersion;
}

DWORD SystemDllGetVersion(const TCHAR* dll_name)  {
  ASSERT1(dll_name);
  CString full_dll_path(String_MakeEndWith(GetSystemDir(), _T("\\"), false) +
                        dll_name);
  ASSERT1(File::Exists(full_dll_path));
  return DllGetVersion(full_dll_path);
}

ULONGLONG GetVersionFromModule(HMODULE instance) {
  TCHAR module_path[MAX_PATH] = {0};
  if (!::GetModuleFileName(instance, module_path, MAX_PATH) != 0) {
    return 0;
  }

  return GetVersionFromFile(module_path);
}

ULONGLONG GetVersionFromFile(const CString& file_path) {
  FileVer existing_file_ver;
  if (!existing_file_ver.Open(file_path)) {
    return 0;
  }

  return existing_file_ver.GetFileVersionAsULONGLONG();
}

// Returns a temporary dir for the impersonated user and an empty string if
// the user is not impersonated or an error occurs.
CString GetTempDirForImpersonatedUser() {
  CAccessToken access_token;
  if (!access_token.GetThreadToken(TOKEN_READ)) {
    return NULL;
  }

  CString temp_dir;
  if (::ExpandEnvironmentStringsForUser(access_token.GetHandle(),
                                        _T("%TMP%"),
                                        CStrBuf(temp_dir, MAX_PATH),
                                        MAX_PATH)) {
    return temp_dir;
  }
  if (::ExpandEnvironmentStringsForUser(access_token.GetHandle(),
                                        _T("%TEMP%"),
                                        CStrBuf(temp_dir, MAX_PATH),
                                        MAX_PATH)) {
    return temp_dir;
  }

  const int kCsIdl = CSIDL_LOCAL_APPDATA | CSIDL_FLAG_DONT_VERIFY;
  if (SUCCEEDED(GetFolderPath(kCsIdl, &temp_dir)) &&
    ::PathAppend(CStrBuf(temp_dir, MAX_PATH), LOCAL_APPDATA_REL_TEMP_DIR)) {
    return temp_dir;
  }

  return NULL;
}


CString GetTempDirForImpersonatedOrCurrentUser() {
  CString temp_dir_for_impersonated_user(GetTempDirForImpersonatedUser());
  if (!temp_dir_for_impersonated_user.IsEmpty()) {
    CStrBuf string_buffer(temp_dir_for_impersonated_user, MAX_PATH);
    if (::PathAddBackslash(string_buffer)) {
      return temp_dir_for_impersonated_user;
    }
  }
  return app_util::GetTempDir();
}

}  // namespace app_util

}  // namespace omaha

