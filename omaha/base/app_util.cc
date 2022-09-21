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
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"

namespace omaha {

namespace app_util {

HMODULE GetModuleHandleFromAddress(void* address) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  ASSERT1(result == sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}

HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(GetCurrentModuleHandle);
}


CString GetModulePath(HMODULE module_handle) {
  ASSERT1(IsModuleHandleValid(module_handle));

  CString modulePath;

  DWORD pathlen = MAX_PATH - 1;
  DWORD bufsize = 0;
  do {
    bufsize = pathlen + 1;

    // On WinXP, if path length >= bufsize, the output is truncated and NOT
    // null-terminated.  On Vista and later, it will null-terminate the
    // truncated string. We call ReleaseBuffer on all OSes to be safe.
    pathlen = ::GetModuleFileName(module_handle,
                                  modulePath.GetBuffer(bufsize),
                                  bufsize);
    ASSERT(pathlen != 0, (_T("::GetLastError[%d]"), ::GetLastError()));
    modulePath.ReleaseBuffer(pathlen < bufsize ? pathlen : 0);
  } while (pathlen >= bufsize);

  ASSERT1(pathlen == static_cast<DWORD>(modulePath.GetLength()));
  return modulePath;
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
  CString winPath;

  UINT pathlen = MAX_PATH - 1;
  UINT bufsize = 0;
  do {
    bufsize = pathlen + 1;
    pathlen = ::GetWindowsDirectory(winPath.GetBuffer(bufsize), bufsize);
    ASSERT(pathlen != 0, (_T("::GetLastError[%d]"), ::GetLastError()));
    winPath.ReleaseBuffer(pathlen < bufsize ? pathlen : 0);
  } while (pathlen >= bufsize);

  ASSERT1(pathlen == static_cast<UINT>(winPath.GetLength()));
  return winPath;
}

CString GetSystemDir() {
  CString systemPath;

  UINT pathlen = MAX_PATH - 1;
  UINT bufsize = 0;
  do {
    bufsize = pathlen + 1;
    pathlen = ::GetSystemDirectory(systemPath.GetBuffer(bufsize), bufsize);
    ASSERT(pathlen != 0, (_T("::GetLastError[%d]"), ::GetLastError()));
    systemPath.ReleaseBuffer(pathlen < bufsize ? pathlen : 0);
  } while (pathlen >= bufsize);

  ASSERT1(pathlen == static_cast<UINT>(systemPath.GetLength()));
  return systemPath;
}

CString GetSystemWow64Dir() {
  // This function always fails under 32-bit Windows.  Return an empty string
  // without asserting.
  if (!Process::IsWow64(::GetCurrentProcessId())) {
    UTIL_LOG(L5, (_T("[GetSystemWow64Directory called on 32-bit OS]")));
    return _T("");
  }

  CString systemPath;

  UINT pathlen = MAX_PATH - 1;
  UINT bufsize = 0;
  do {
    bufsize = pathlen + 1;
    pathlen = ::GetSystemWow64Directory(systemPath.GetBuffer(bufsize), bufsize);
    ASSERT(pathlen != 0, (_T("::GetLastError[%d]"), ::GetLastError()));
    systemPath.ReleaseBuffer(pathlen < bufsize ? pathlen : 0);
  } while (pathlen >= bufsize);

  ASSERT1(pathlen == static_cast<UINT>(systemPath.GetLength()));
  return systemPath;
}

CString GetTempDir() {
  CString tempPath;

  DWORD pathlen = MAX_PATH - 1;
  DWORD bufsize = 0;
  do {
    bufsize = pathlen + 1;
    pathlen = ::GetTempPath(bufsize, tempPath.GetBuffer(bufsize));
    ASSERT(pathlen != 0, (_T("::GetLastError[%d]"), ::GetLastError()));
    tempPath.ReleaseBuffer(pathlen < bufsize ? pathlen : 0);
  } while (pathlen >= bufsize);

  ASSERT1(pathlen == static_cast<DWORD>(tempPath.GetLength()));
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
      // check that we're not in an overflow situation here
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
  return GetVersionFromFile(GetModulePath(instance));
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

