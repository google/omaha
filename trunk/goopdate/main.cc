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

#include <windows.h>
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/google_update_idl_datax.h"
#include "omaha/goopdate/goopdate.h"

namespace {

HINSTANCE dll_instance = NULL;

}  // namespace

// Captures the module instance. Never call ::DisableThreadLibraryCalls in a
// module that statically links with LIBC and it is not using
// _beginthreadex for the thread creation. This will leak the _tiddata.
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* res) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      dll_instance = instance;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
    default:
      break;
  }

  return PrxDllMain(instance, reason, res);
}

// Since this entry point is called by a tiny shell we've built without
// CRT support, the command line parameters contain the command line that
// the OS created the process with. By convention, the first token in the
// command line string is the filename of the program executable. The CRT takes
// care of that for program that link with the standard libraries. Therefore,
// there is a difference in the command line that the OS knows and the
// command line that is passed to WinMain by the CRT. Our command line
// parsing code takes care of this difference.
extern "C" int APIENTRY DllEntry(TCHAR* cmd_line, int cmd_show) {
  OPT_LOG(L1, (_T("[DllEntry][%s]"), cmd_line));

  bool is_local_system =  false;
  HRESULT hr = omaha::IsSystemProcess(&is_local_system);
  if (SUCCEEDED(hr)) {
    omaha::Goopdate goopdate(is_local_system);
    hr = goopdate.Main(dll_instance, cmd_line, cmd_show);
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[Main failed][%s][0x%08x]"), cmd_line, hr));
    }
  }

  OPT_LOG(L1, (_T("[DllEntry exit][0x%08x]"), hr));
  return static_cast<int>(hr);
}

extern "C" STDAPI DllCanUnloadNow() {
  return PrxDllCanUnloadNow();
}

extern "C" STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* ptr) {
  return PrxDllGetClassObject(clsid, iid, ptr);
}

