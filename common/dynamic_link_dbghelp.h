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
// dynamic loading of dbghelp dll functions
//

#ifndef OMAHA_COMMON_DYNAMIC_LINK_DBGHELP_H_
#define OMAHA_COMMON_DYNAMIC_LINK_DBGHELP_H_

#include <dbghelp.h>

namespace omaha {

class Dbghelp {
 public:
  static BOOL (CALLBACK *SymInitialize)(HANDLE, PCSTR, BOOL);
  static BOOL (CALLBACK *SymCleanup)(HANDLE);
  static BOOL (CALLBACK *SymEnumSymbols)(HANDLE, ULONG64, PCSTR, PSYM_ENUMERATESYMBOLS_CALLBACK, PVOID);
  static DWORD (CALLBACK *SymSetOptions)(DWORD);
  static BOOL (CALLBACK *SymSetContext)(HANDLE, PIMAGEHLP_STACK_FRAME, PIMAGEHLP_CONTEXT);
  static BOOL (CALLBACK *SymGetLineFromAddr)(HANDLE, DWORD, PDWORD, PIMAGEHLP_LINE);
  static BOOL (CALLBACK *SymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
  static BOOL (CALLBACK *SymFromAddr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
  static BOOL (CALLBACK *StackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64,
      PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
  static BOOL (CALLBACK *StackWalk)(DWORD, HANDLE, HANDLE, LPSTACKFRAME, PVOID, PREAD_PROCESS_MEMORY_ROUTINE,
      PFUNCTION_TABLE_ACCESS_ROUTINE, PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE);
  static PVOID (CALLBACK *SymFunctionTableAccess64)(HANDLE, DWORD64);
  static PVOID (CALLBACK *SymFunctionTableAccess)(HANDLE, DWORD);
  static DWORD64 (CALLBACK *SymGetModuleBase64)(HANDLE, DWORD64);
  static DWORD64 (CALLBACK *SymGetModuleBase)(HANDLE, DWORD);
  static BOOL (CALLBACK *SymGetTypeInfo)(HANDLE, DWORD64, ULONG, IMAGEHLP_SYMBOL_TYPE_INFO, PVOID);
  static BOOL (CALLBACK *MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
      PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

  static HRESULT Load();
  static void Unload();
  static bool Loaded() { return (loaded_state_ == LOAD_SUCCEEDED && library_ != NULL); }

 private:
  // If we tried to load and failed, do not try again.
  static enum LoadedState {NOT_LOADED, LOAD_FAILED, LOAD_SUCCEEDED};

  static LoadedState loaded_state_;
  static HINSTANCE library_;

  static void Clear();

  // wrapper around GetProcAddress()
  template <typename T>
  static bool GPA(const char * function_name, T& function_pointer);

  DISALLOW_EVIL_CONSTRUCTORS(Dbghelp);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_DYNAMIC_LINK_DBGHELP_H_
