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

#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/utils.h"

namespace omaha {

const int kLongPath = (_MAX_PATH * 2);
const int kReallyLongPath = (kLongPath * 2);

HMODULE ModuleFromStatic(void* pointer_to_static_in_module) {
  ASSERT(pointer_to_static_in_module, (L""));

  MEMORY_BASIC_INFORMATION info = { 0 };
  VirtualQuery(reinterpret_cast<void*>(pointer_to_static_in_module),
    &info, sizeof(info));
  // Module handles are just the allocation base address of the module.
  return reinterpret_cast<HMODULE>(info.AllocationBase);
}

bool GetModuleDirectory(HMODULE module, TCHAR* directory) {
  ASSERT(directory, (L"Invalid arguments"));
  if (!directory) {
    return false;
  }

  // PathRemoveFileSpec only supports buffers up to MAX_PATH so we must
  // limit ourselves to this.  It will "always" work anyway, given that
  // our installation path is not absurdly deep.
  if (0 == GetModuleFileName(module, directory, MAX_PATH)) {
    ASSERT(false, (L"Path longer than MAX_PATH"));
    return false;
  }

  if (!String_PathRemoveFileSpec(directory)) {
    ASSERT(false, (L"PathRemoveFileSpec failed"));
    // Ensure we don't return with an incorrect path in the buffer that was
    // passed in.
    ZeroMemory(directory, MAX_PATH * sizeof(TCHAR));
    return false;
  }

  return true;
}

HRESULT GetModuleFileName(HMODULE module, CString* path) {
  ASSERT(path, (_T("must be valid")));

  // _MAX_PATH should cover at least 99% of the paths
  int buf_size = _MAX_PATH;
  int chars_copied = 0;
  while ((chars_copied = ::GetModuleFileName(module,
                                             CStrBuf(*path, buf_size + 1),
                                             buf_size)) == buf_size) {
    // We'll stop before things get ridiculous
    if (buf_size >= kReallyLongPath) {
      UTIL_LOG(LEVEL_ERROR,
               (_T("[GetModuleFileName - unusually long path '%s']"), path));
      chars_copied = 0;
      ::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      break;
    }

    buf_size *= 2;
  }

  if (!chars_copied) {
    path->Empty();
    return GetCurError();
  }

  return S_OK;
}

}  // namespace omaha

