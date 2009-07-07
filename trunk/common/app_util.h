// Copyright 2003-2009 Google Inc.
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
//
// Utility functions for getting app and module information

#ifndef OMAHA_COMMON_APP_UTIL_H__
#define OMAHA_COMMON_APP_UTIL_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/constants.h"

namespace omaha {

namespace app_util {

// Gets the handle to the module containing the given executing address.
HMODULE GetModuleHandleFromAddress(void* address);

// Gets the handle to the currently executing module.
HMODULE GetCurrentModuleHandle();

// Gets the path of the loaded module.
// If module_handle == NULL returns the path of the current executable.
CString GetModulePath(HMODULE module_handle);

// Gets the directory of the specified module
// Returns the part of the module path, before the last '\'.
// Returns the dir from where the module was loaded (could be exe or dll).
CString GetModuleDirectory(HMODULE module_handle);

// Gets the name of the specified module
// Returns the part of the module path, after the last '\'.
CString GetModuleName(HMODULE module_handle);

// Gets the name of the specified module without the extension.
CString GetModuleNameWithoutExtension(HMODULE module_handle);

// Gets the current app name (i.e. exe name).
CString GetAppName();

// Gets the current app name without the extension.
CString GetAppNameWithoutExtension();

// Gets the current module path
// returns the path from where the module was loaded (could be exe or dll).
CString GetCurrentModulePath();

// Gets the current module directory
// returns the dir from where the module was loaded (could be exe or dll).
CString GetCurrentModuleDirectory();

// Gets the current module name.
CString GetCurrentModuleName();

// Gets the current module name without the extension.
CString GetCurrentModuleNameWithoutExtension();

// Checks if the given module is a dll or an exe.
// Assumes a correct HMODULE.
bool IsModuleDll(HMODULE module_handle);

// Checks if the current module is a dll or an exe.
bool IsCurrentModuleDll();

// Checks if the given address is in the current module.
bool IsAddressInCurrentModule(void* address);

// Gets the host machine name.
CString GetHostName();

// Gets the Windows directory.
CString GetWindowsDir();

// Gets the System directory.
CString GetSystemDir();

// Gets the TEMP directory for the current user.
CString GetTempDir();

// Helper that gets us the version of a DLL in a DWORD format,
// with the major and minor versions squeezed into it.
DWORD DllGetVersion(const CString& dll_path);

// Helper that gets us the version of a System DLL in a DWORD format,
// with the major and minor versions squeezed into it. The assumption
// is that the dll_name is only a name, and not a path. Using this
// function (over DllGetVersion directly) for System DLLs is recommended
// from a security perspective.
// However, this may not work for DLLs that are loaded from the side-by-side
// location (WinSxS) instead of the system directory.
DWORD SystemDllGetVersion(const TCHAR* dll_name);

// Gets the version from a module.
ULONGLONG GetVersionFromModule(HMODULE instance);

// Gets the version from a file path.
ULONGLONG GetVersionFromFile(const CString& file_path);

// Gets a guid from the system (for user id, etc.)
// The guid will be of the form: 00000000-0000-0000-0000-000000000000
HRESULT GetGuid(CString* guid);

// Helper to check if a module handle is valid.
bool IsModuleHandleValid(HMODULE module_handle);

inline CString GetAppName() {
  return GetModuleName(NULL);
}

inline CString GetAppNameWithoutExtension() {
  return GetModuleNameWithoutExtension(NULL);
}

}  // namespace app_util

}  // namespace omaha

#endif  // OMAHA_COMMON_APP_UTIL_H__

