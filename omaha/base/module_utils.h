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

#ifndef OMAHA_COMMON_MODULE_UTILS_H_
#define OMAHA_COMMON_MODULE_UTILS_H_

#include "omaha/base/string.h"

namespace omaha {

// Utilities for working with modules in processes.

// Returns the module handle of a module, given a pointer to a static
// member of the module (e.g. a static function or static variable).
HMODULE ModuleFromStatic(void* pointer_to_static_in_module);

// Copies the path of the directory that contains the file for 'module' into
// 'directory'.
//
// @param module Must be a valid, non-NULL module handle.
// @param directory MUST have room for MAX_PATH characters or more.  The path
// copied into this buffer will not have a trailing backslash.
//
// @return false iff there is an error.
bool GetModuleDirectory(HMODULE module, TCHAR* directory);

/**
* Returns a path to a module.  Uses the
* Win32 GetModuleFileName function, so you
* can pass NULL for module.
*
* @param module Handle to the module or NULL for the current module.
* @param path Holds the path to the module on successful return.
* @returns S_OK if successful, otherwise an error value.
*/
HRESULT GetModuleFileName(HMODULE module, OUT CString* path);

}  // namespace omaha

#endif  // OMAHA_COMMON_MODULE_UTILS_H_
