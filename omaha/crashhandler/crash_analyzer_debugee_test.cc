// Copyright 2013 Google Inc.
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

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>

#include <string>
#include <map>

#include "omaha/base/utils.h"

typedef void (*DebugeeFunction)(void);
typedef std::map<std::wstring, DebugeeFunction> DebugeeMap;
DebugeeMap debugee_map;
HANDLE ready_event = NULL;

void AwaitTheReaper() {
  ::SetEvent(ready_event);
  ::CloseHandle(ready_event);
  while (true)
    ::Sleep(1);
}

void TestNormal() {
  AwaitTheReaper();
}

void MaxExecMappings() {
  ::VirtualAlloc(NULL, 0x19001000, MEM_RESERVE | MEM_COMMIT,
                 PAGE_EXECUTE_READWRITE);
  AwaitTheReaper();
}

void NtFunctionsOnStack() {
  HMODULE ntdll = omaha::LoadSystemLibrary(_T("ntdll.dll"));
  FARPROC ptr = ::GetProcAddress(ntdll, "ZwProtectVirtualMemory");
  AwaitTheReaper();
}

void TestWildStackPointer() {
  char* new_stack = reinterpret_cast<char*>(
      ::VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) +
          4096;
  char* old_stack = NULL;
  __asm {
    mov old_stack, esp
    mov esp, new_stack
  };
  AwaitTheReaper();
}

void TestPENotInModuleList() {
  MEMORY_BASIC_INFORMATION mbi = {0};
  BYTE* ntdll = reinterpret_cast<BYTE*>(
      omaha::LoadSystemLibrary(_T("ntdll.dll")));
  ::VirtualQuery(ntdll, &mbi, sizeof(mbi));
  LPVOID buffer = ::VirtualAlloc(NULL,
                                 mbi.RegionSize,
                                 MEM_RESERVE | MEM_COMMIT,
                                 PAGE_EXECUTE_READWRITE);
  memcpy(buffer, ntdll, mbi.RegionSize);
  AwaitTheReaper();
}

void TestShellcodeSprayPattern() {
  LPVOID buffer = ::VirtualAlloc(NULL,
                                 0x4000,
                                 MEM_RESERVE | MEM_COMMIT,
                                 PAGE_EXECUTE_READWRITE);
  memset(buffer, 0x0c, 0x4000);
  AwaitTheReaper();
}
void TestTiBDereference() {
  void* ex_mapping = ::VirtualAlloc(
      reinterpret_cast<LPVOID>(0x41410000),
      sizeof(EXCEPTION_POINTERS*) +
          sizeof(EXCEPTION_POINTERS) + sizeof(CONTEXT),
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  EXCEPTION_POINTERS** epp =
      reinterpret_cast<EXCEPTION_POINTERS**>(ex_mapping);
  *epp = reinterpret_cast<EXCEPTION_POINTERS*>(epp + 1);
  EXCEPTION_POINTERS* ep = *epp;
  ep->ContextRecord = reinterpret_cast<PCONTEXT>(ep + 1);
  ep->ExceptionRecord =
      reinterpret_cast<PEXCEPTION_RECORD>(ep->ContextRecord + 1);
  memset(ep->ContextRecord, 0x00, sizeof(CONTEXT));
  ep->ExceptionRecord->ExceptionAddress = reinterpret_cast<PVOID>(0x41414141);
  ep->ExceptionRecord->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  ep->ExceptionRecord->ExceptionFlags = EXCEPTION_NONCONTINUABLE;
  ep->ExceptionRecord->ExceptionInformation[0] = 0;
  ep->ExceptionRecord->ExceptionInformation[1] = 0x7efdd000;
  ep->ExceptionRecord->ExceptionRecord = 0;
  ep->ExceptionRecord->NumberParameters = 2;
  AwaitTheReaper();
}

void TestShellcodeJmpCallPop() {
  const char sc[] = "\x0c\x0c\x90\x90\xeb\x10\x5b\x4b\x33\xc9\x66\xb9\xcc\x03"
                    "\x80\x34\x0b\xe2\xe2\xfa\xeb\x05\xe8\xeb\xff\xff\xff\x5e"
                    "\xe2\x42\x94\xef\x0b\xa3\xe1\xe2\xe2\xbd\xd1\x22\x86\x43"
                    "\xd2";
  void* exec_mapping = ::VirtualAlloc(NULL, sizeof(sc),
                                      MEM_RESERVE | MEM_COMMIT,
                                      PAGE_EXECUTE_READWRITE);
  memcpy(exec_mapping, sc, sizeof(sc));
  void* ex_mapping = ::VirtualAlloc(
      reinterpret_cast<LPVOID>(0x41410000),
      sizeof(EXCEPTION_POINTERS*) +
          sizeof(EXCEPTION_POINTERS) + sizeof(CONTEXT),
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  EXCEPTION_POINTERS** epp =
      reinterpret_cast<EXCEPTION_POINTERS**>(ex_mapping);
  *epp = reinterpret_cast<EXCEPTION_POINTERS*>(epp + 1);
  EXCEPTION_POINTERS* ep = *epp;
  ep->ContextRecord = reinterpret_cast<PCONTEXT>(ep + 1);
  ep->ExceptionRecord =
      reinterpret_cast<PEXCEPTION_RECORD>(ep->ContextRecord + 1);
  memset(ep->ContextRecord, 0x00, sizeof(ep->ContextRecord));
  ep->ExceptionRecord->ExceptionAddress =
      reinterpret_cast<PVOID>(exec_mapping);
  ep->ExceptionRecord->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  ep->ExceptionRecord->ExceptionFlags = EXCEPTION_NONCONTINUABLE;
  ep->ExceptionRecord->ExceptionInformation[0] = 8;
  ep->ExceptionRecord->ExceptionInformation[1] =
      reinterpret_cast<ULONG_PTR>(exec_mapping);
  ep->ExceptionRecord->ExceptionRecord = 0;
  ep->ExceptionRecord->NumberParameters = 2;
  AwaitTheReaper();
}

int RunTestDebugee(std::wstring event_name, std::wstring test_name) {
  if (event_name != std::wstring(L"None")) {
    ready_event = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, event_name.c_str());
    if (ready_event == NULL) {
      return -1;
    }
  }
  DebugeeMap::iterator it = debugee_map.find(test_name);
  if (it == debugee_map.end()) {
    return -1;
  }

  (*it).second();
  return 0;
}

void InitDebugees() {
  debugee_map[L"Normal"] = TestNormal;
  debugee_map[L"MaxExecMappings"] = MaxExecMappings;
  debugee_map[L"NtFunctionsOnStack"] = NtFunctionsOnStack;
  debugee_map[L"WildStackPointer"] = TestWildStackPointer;
  debugee_map[L"PENotInModuleList"] = TestPENotInModuleList;
  debugee_map[L"ShellcodeSprayPattern"] = TestShellcodeSprayPattern;
  debugee_map[L"ShellcodeJmpCallPop"] = TestShellcodeJmpCallPop;
  debugee_map[L"TiBDereference"] = TestTiBDereference;
}

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR lpCommandLine, int) {
  int argc = 0;
  LPWSTR* argv = ::CommandLineToArgvW(lpCommandLine, &argc);
  if (!argv || argc != 2)
    return -1;

  InitDebugees();
  std::wstring event_name = argv[0];
  std::wstring test_name = argv[1];
  return RunTestDebugee(event_name, test_name);
}
