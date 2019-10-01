// Copyright 2019 Google Inc.
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

// Launches the core process by starting the shell exe with /c argument

#ifndef OMAHA_CORE_CORE_LAUNCHER_H_
#define OMAHA_CORE_CORE_LAUNCHER_H_

#include <winerror.h>

namespace omaha {

bool ShouldRunCore(bool is_system);

HRESULT StartCoreIfNeeded(bool is_system);

}  // namespace omaha

#endif  // OMAHA_CORE_CORE_LAUNCHER_H_
