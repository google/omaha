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
// Helper for the Google Update repair file. It launches the same repair file
// elevated using the MSP.

#ifndef OMAHA_RECOVERY_REPAIR_EXE_REPAIR_GOOPDATE_H__
#define OMAHA_RECOVERY_REPAIR_EXE_REPAIR_GOOPDATE_H__

#include <windows.h>
#include <tchar.h>

namespace omaha {

// Launches the repair file elevated if necessary.
// Returns whether the repair file was successfully launched elevated.
// When the method returns false, the caller should execute the repair
// operations unelevated. Otherwise, the caller is done.
// Example:
//   if (!LaunchRepairFileElevated(...)) {
//     DoRepairUnelevated();
//   }
bool LaunchRepairFileElevated(bool is_machine,
                              const TCHAR* repair_file,
                              const TCHAR* args,
                              HRESULT* elevation_hr);

}  // namespace omaha

#endif  // OMAHA_RECOVERY_REPAIR_EXE_REPAIR_GOOPDATE_H__
