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
// Dynamic loading of Windows kernel32.dll API dll functions not supported
// on Windows 95/98/ME.

#ifndef OMAHA_COMMON_DYNAMIC_LINK_KERNEL32_H_
#define OMAHA_COMMON_DYNAMIC_LINK_KERNEL32_H_

#include <windows.h>
#include <tlhelp32.h>
#include "base/basictypes.h"

namespace omaha {

class Kernel32 {
 public:

  static BOOL WINAPI Module32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
  static BOOL WINAPI Module32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
  static BOOL WINAPI Process32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
  static BOOL WINAPI Process32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);

  // Is process running under WOW64?
  // WOW64 is the emulator for win32 applications running on win64.
  static BOOL WINAPI IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(Kernel32);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_DYNAMIC_LINK_KERNEL32_H_
