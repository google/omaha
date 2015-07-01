// Copyright 2010 Google Inc.
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

#ifndef OMAHA_COMMON_OEM_INSTALL_UTILS_H_
#define OMAHA_COMMON_OEM_INSTALL_UTILS_H_

#include <windows.h>

namespace omaha {

namespace oem_install_utils {

// Writes OEM install beginning timestamp in the registry.
HRESULT SetOemInstallState(bool is_machine);

// Removes OEM install beginning timestamp from the registry.
HRESULT ResetOemInstallState(bool is_machine);

// Returns true if running in the context of an OEM install.
bool IsOemInstalling(bool is_machine);

}  // namespace oem_install_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_OEM_INSTALL_UTILS_H_
