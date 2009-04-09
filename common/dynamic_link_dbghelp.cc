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

#include "omaha/common/debug.h"
#include "omaha/common/dynamic_link_dbghelp.h"

namespace omaha {

BOOL (CALLBACK *Dbghelp::SymInitialize)(HANDLE, PCSTR, BOOL);
BOOL (CALLBACK *Dbghelp::SymCleanup)(HANDLE);
BOOL (CALLBACK *Dbghelp::SymEnumSymbols)(HANDLE, ULONG64, PCSTR, PSYM_ENUMERATESYMBOLS_CALLBACK, PVOID);
DWORD (CALLBACK *Dbghelp::SymSetOptions)(DWORD);
BOOL (CALLBACK *Dbghelp::SymSetContext)(HANDLE, PIMAGEHLP_STACK_FRAME, PIMAGEHLP_CONTEXT);
BOOL (CALLBACK *Dbghelp::SymGetLineFromAddr)(HANDLE, DWORD, PDWORD, PIMAGEHLP_LINE);
BOOL (CALLBACK *Dbghelp::SymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
BOOL (CALLBACK *Dbghelp::SymFromAddr)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
BOOL (CALLBACK *Dbghelp::StackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64,
        PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
BOOL (CALLBACK *Dbghelp::StackWalk)(DWORD, HANDLE, HANDLE, LPSTACKFRAME, PVOID, PREAD_PROCESS_MEMORY_ROUTINE,
        PFUNCTION_TABLE_ACCESS_ROUTINE, PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE);
PVOID (CALLBACK *Dbghelp::SymFunctionTableAccess64)(HANDLE, DWORD64);
PVOID (CALLBACK *Dbghelp::SymFunctionTableAccess)(HANDLE, DWORD);
DWORD64 (CALLBACK *Dbghelp::SymGetModuleBase64)(HANDLE, DWORD64);
DWORD64 (CALLBACK *Dbghelp::SymGetModuleBase)(HANDLE, DWORD);
BOOL (CALLBACK *Dbghelp::SymGetTypeInfo)(HANDLE, DWORD64, ULONG, IMAGEHLP_SYMBOL_TYPE_INFO, PVOID);
BOOL (CALLBACK *Dbghelp::MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
        PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

Dbghelp::LoadedState Dbghelp::loaded_state_ = Dbghelp::NOT_LOADED;
HINSTANCE Dbghelp::library_ = NULL;

template <typename T>
bool Dbghelp::GPA(const char * function_name, T& function_pointer) {
  ASSERT(function_name, (L""));

  function_pointer = reinterpret_cast<T>(::GetProcAddress(library_,
                                                          function_name));
  return function_pointer != NULL;
}

HRESULT Dbghelp::Load() {
  // If we've already tried to load, don't try again.
  if (loaded_state_ != NOT_LOADED) goto Exit;

  Clear();

  // UTIL_LOG((L2, _T("dbghelp loading")));
  library_ = ::LoadLibrary(_T("dbghelp"));
  // UTIL_LOG((L2, _T("dbghelp loaded")));

  if (!library_) return E_FAIL;

  bool all_valid = (GPA("SymInitialize", SymInitialize))
    & (GPA("SymCleanup", SymCleanup))
    & (GPA("SymSetOptions", SymSetOptions))
    & (GPA("SymGetLineFromAddr", SymGetLineFromAddr))
    & (GPA("SymGetLineFromAddr64", SymGetLineFromAddr64))
    & (GPA("StackWalk", StackWalk))
    & (GPA("StackWalk64", StackWalk64))
    & (GPA("SymFunctionTableAccess", SymFunctionTableAccess))
    & (GPA("SymFunctionTableAccess64", SymFunctionTableAccess64))
    & (GPA("SymGetModuleBase", SymGetModuleBase))
    & (GPA("SymGetModuleBase64", SymGetModuleBase64))
    & (GPA("MiniDumpWriteDump", MiniDumpWriteDump));

  // These are not supported in the Win2k version of DbgHelp;
  // failing to load them is not an error.
  GPA("SymEnumSymbols", SymEnumSymbols);
  GPA("SymGetTypeInfo", SymGetTypeInfo);
  GPA("SymSetContext", SymSetContext);
  GPA("SymFromAddr", SymFromAddr);

  if (!all_valid) Unload();
  loaded_state_ = all_valid ? LOAD_SUCCEEDED : LOAD_FAILED;

 Exit:
  return (loaded_state_ == LOAD_SUCCEEDED) ? S_OK : E_FAIL;
}

void Dbghelp::Unload() {
  if (library_) {
    VERIFY(::FreeLibrary(library_), (L""));

    // Must set library_ to NULL so that Loaded() will return FALSE.
    library_ = NULL;
    }
  Clear();
}

void Dbghelp::Clear() {
  // just clear the main entry points
  SymInitialize = NULL;
  MiniDumpWriteDump = NULL;
}

}  // namespace omaha

