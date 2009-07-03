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
//
// dynamic loading of Windows kernel32.dll API dll functions
// wrappers for win32 functions not supported on windows 95/98/ME

#include "omaha/common/debug.h"
#include "omaha/common/dynamic_link_kernel32.h"

namespace omaha {

typedef BOOL (WINAPI * Module32FirstFunc)(HANDLE, LPMODULEENTRY32);
typedef BOOL (WINAPI * Module32NextFunc)(HANDLE, LPMODULEENTRY32);
typedef BOOL (WINAPI * Process32FirstFunc)(HANDLE, LPPROCESSENTRY32);
typedef BOOL (WINAPI * Process32NextFunc)(HANDLE, LPPROCESSENTRY32);
typedef BOOL (WINAPI * IsWow64ProcessFunc)(HANDLE, PBOOL);

#define kKernel32Module L"kernel32"

BOOL WINAPI Kernel32::Module32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme) {
  static Module32FirstFunc f = NULL;

  if (f == NULL) {
    HMODULE handle = GetModuleHandle(kKernel32Module);
    ASSERT(handle, (L""));
    f = (Module32FirstFunc) GetProcAddress(handle, "Module32FirstW");
  }

  if (f == NULL)
    return FALSE;

  return f(hSnapshot, lpme);
}

BOOL WINAPI Kernel32::Module32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme) {
  static Module32NextFunc f = NULL;

  if (f == NULL) {
    HMODULE handle = GetModuleHandle(kKernel32Module);
    ASSERT(handle, (L""));
    f = (Module32NextFunc) GetProcAddress(handle, "Module32NextW");
  }

  if (f == NULL)
    return FALSE;

  return f(hSnapshot, lpme);
}

BOOL WINAPI Kernel32::Process32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe) {
  static Process32FirstFunc f = NULL;

  if (f == NULL) {
    HMODULE handle = GetModuleHandle(kKernel32Module);
    ASSERT(handle, (L""));
    f = (Process32FirstFunc) GetProcAddress(handle, "Process32FirstW");
  }

  if (f == NULL)
    return FALSE;

  return f(hSnapshot, lppe);
}

BOOL WINAPI Kernel32::Process32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe) {
  static Process32NextFunc f = NULL;

  if (f == NULL) {
    HMODULE handle = GetModuleHandle(kKernel32Module);
    ASSERT(handle, (L""));
    f = (Process32NextFunc) GetProcAddress(handle, "Process32NextW");
  }

  if (f == NULL)
    return FALSE;

  return f(hSnapshot, lppe);
}

BOOL WINAPI Kernel32::IsWow64Process(HANDLE hProcess, PBOOL Wow64Process) {
  static IsWow64ProcessFunc f = NULL;

  if (f == NULL) {
    HMODULE handle = GetModuleHandle(kKernel32Module);
    ASSERT(handle, (L""));
    f = (IsWow64ProcessFunc) GetProcAddress(handle, "IsWow64Process");
  }

  if (f == NULL)
    return FALSE;

  return f(hProcess, Wow64Process);
}

}  // namespace omaha

