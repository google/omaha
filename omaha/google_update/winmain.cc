// Copyright 2007-2009 Google Inc.
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

// This is a small shell that loads a DLL calls its well known entry point.
// The intention is to only depend on OS mechanisms and avoid the LIBC
// dependency completely.
// This indirection is done primarily to:
// Play nicely with software firewalls. Most firewalls watch the executable
// module making network requests and we do not want them to notify the user
// after we've updated the program. The DLL can change independently of the
// shell and we expect the shell to remain unchanged most of the time.
//
// Changes to this executable will not appear in offical builds until they are
// included in an offical build and the resulting file is checked in to the
// saved shell location.

// Disable the RTC checks because this shell doesn't build with a CRT.
#pragma runtime_checks("", off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>

#include <atlbase.h>
#include <atlpath.h>
#include <atlstr.h>

#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_goopdate.h"

// TODO(omaha3): move to common.
#include "omaha/goopdate/main.h"

namespace omaha {

// Disable the stack checks to keep the code size down.
// The check_stack pragma did not really work. The stack checks had to be
// disabled in the mk_file.
#pragma check_stack()

// Have to define this here since this is used in signaturevalidator.cc.
// This is defined in error.cc, but that pulls in debug.cc, which has a lot
// of additional dependencies we do not want.  Not worth it for just this
// function.
HRESULT HRESULTFromLastError() {
  DWORD error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

// Adapted from File::Exists in file.cc.
bool FileExists(const TCHAR* file_name) {
  if (!file_name || !*file_name) {
    return false;
  }

  WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
  return 0 != ::GetFileAttributesEx(file_name, ::GetFileExInfoStandard, &attrs);
}

// Returns true if the current process is running under the passed-in csidl.
bool IsRunningFromDir(int csidl) {
  // TODO(omaha3): Once we've refactored main.lib (http://b/5904730) and can
  // link with it, replace this with a call to app_util::GetModulePath().
  CString module_path;
  DWORD result = ::GetModuleFileName(NULL,
                                     CStrBuf(module_path, MAX_PATH),
                                     MAX_PATH);
  if (!result || result > MAX_PATH) {
    return false;
  }

  CPath path_temp(module_path);
  path_temp.RemoveFileSpec();
  module_path = static_cast<CString>(path_temp);

  CString folder_path;
  if (FAILED(::SHGetFolderPath(NULL,
                               csidl,
                               NULL,
                               SHGFP_TYPE_CURRENT,
                               CStrBuf(folder_path, MAX_PATH)))) {
    return false;
  }

  folder_path.MakeLower();
  module_path.MakeLower();

  return (module_path.Find(folder_path) == 0);
}

HRESULT GetRegisteredVersion(bool is_machine, CString* version) {
  HKEY key = NULL;
  LONG res = ::RegOpenKeyEx(is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                            GOOPDATE_REG_RELATIVE_CLIENTS GOOPDATE_APP_ID,
                            0,
                            KEY_READ,
                            &key);
  if (ERROR_SUCCESS != res) {
    return HRESULT_FROM_WIN32(res);
  }

  DWORD type = 0;
  DWORD version_length = 50;
  res = ::SHQueryValueEx(key,
                         omaha::kRegValueProductVersion,
                         NULL,
                         &type,
                         CStrBuf(*version, version_length),
                         &version_length);
  if (ERROR_SUCCESS != res) {
    return HRESULT_FROM_WIN32(res);
  }

  if (REG_SZ != type) {
    return E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT GetDllPath(HINSTANCE instance, bool is_machine, CString* dll_path) {
  TCHAR base_path[MAX_PATH] = {0};
  TCHAR path[MAX_PATH] = {0};

  if (!::GetModuleFileName(instance, base_path, arraysize(base_path))) {
    return HRESULTFromLastError();
  }
  ::PathRemoveFileSpec(base_path);

  // Try the side-by-side DLL first.
  _tcscpy_s(path, arraysize(path), base_path);
  if (!::PathAppend(path, omaha::kOmahaDllName)) {
    return HRESULTFromLastError();
  }
  if (FileExists(path)) {
    *dll_path = path;
    return S_OK;
  }

  // Try the version subdirectory.
  _tcscpy_s(path, arraysize(path), base_path);
  CString version;
  HRESULT hr = GetRegisteredVersion(is_machine, &version);
  if (FAILED(hr)) {
    return hr;
  }
  if (!::PathAppend(path, version)) {
    return HRESULTFromLastError();
  }
  if (!::PathAppend(path, omaha::kOmahaDllName)) {
    return HRESULTFromLastError();
  }
  if (!FileExists(path)) {
    return GOOGLEUPDATE_E_DLL_NOT_FOUND;
  }

  *dll_path = path;
  return S_OK;
}

}  // namespace omaha

// Algorithm:
//  * Looks for goopdate.dll in the current directory.
//  * If it is not found, looks for goopdate.dll in a version subdirectory based
//    on the version found in the registry.
//  * Loads the DLL and calls the entry point.
int WINAPI _tWinMain(HINSTANCE instance,
                     HINSTANCE,
                     LPTSTR,
                     int cmd_show) {
  omaha::EnableSecureDllLoading();

  // We assume here that running from program files means we should check
  // the machine install version and otherwise we should check the user
  // version. This should be true in all end user cases except for initial
  // installs from the temp directory, in which case the DLL should be in the
  // same directory so this value does not get used.
  // For developer use cases, the DLL should also be in the same directory.
  bool is_machine = omaha::IsRunningFromDir(CSIDL_PROGRAM_FILES);

  CString dll_path;
  HRESULT hr = omaha::GetDllPath(instance, is_machine, &dll_path);
  if (FAILED(hr)) {
    return hr;
  }

  HMODULE module(::LoadLibraryEx(dll_path, NULL, 0));
  if (!module) {
    return omaha::HRESULTFromLastError();
  }

  DllEntry dll_entry = reinterpret_cast<DllEntry>(
      ::GetProcAddress(module, omaha::kGoopdateDllEntryAnsi));
  if (dll_entry) {
    // We must send in GetCommandLine() and not cmd_line because the command
    // line parsing code expects to have the program name as the first argument
    // and cmd_line does not provide this.
    hr = dll_entry(::GetCommandLine(), cmd_show);
  } else {
    hr = E_FAIL;
  }

  ::FreeLibrary(module);

  return hr;
}
