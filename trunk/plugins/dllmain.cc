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
//
// DLLMain boilerplate
//

#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"

// Force the linker to include the ATLModule instance
// (see plugins/oneclick_atl_module.cpp)
#pragma comment(linker, "/INCLUDE:__AtlModule")

// Force the linker to include the object map definition. Each class that
// requires COM registration must be added here, otherwise its object entry
// will be missing from the object map.
#pragma comment(linker, "/INCLUDE:___pobjMap_GoopdateCtrl")

void OneClickOutOfMemoryHandler() {
  ::RaiseException(EXCEPTION_ACCESS_VIOLATION,
                   EXCEPTION_NONCONTINUABLE,
                   0,
                   NULL);
}

extern "C" {
DLLEXPORT
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      VERIFY1(set_new_handler(&OneClickOutOfMemoryHandler) == 0);
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
      break;

    default:
      break;
  }
  return TRUE;
}
}
