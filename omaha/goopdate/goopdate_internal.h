// Copyright 2009 Google Inc.
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

#ifndef OMAHA_GOOPDATE_GOOPDATE_INTERNAL_H_
#define OMAHA_GOOPDATE_GOOPDATE_INTERNAL_H_

#include <windows.h>
#include "omaha/base/constants.h"
#include "omaha/common/command_line.h"

namespace omaha {

namespace internal {

// Marks Google Update's EULA as accepted if an app EULA has been accepted.
HRESULT PromoteAppEulaAccepted(bool is_machine);

// Returns whether a process is a machine process.
// Does not determine whether the process has the appropriate privileges.
bool IsMachineProcess(
    CommandLineMode mode,
    bool is_running_from_official_machine_directory,
    bool is_local_system,  // Whether process is running as local system.
    bool is_machine_override,  // True if machine cmd line override specified.
    Tristate needs_admin);  // needsadmin value for primary app if present.

// Returns whether UI can be displayed.
bool CanDisplayUi(CommandLineMode mode, bool is_silent);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_INTERNAL_H_

