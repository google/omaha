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

#include "omaha/recovery/repair_exe/repair_goopdate.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/recovery/repair_exe/mspexecutableelevator.h"

namespace omaha {

// Returns without launching the repair file if is_machine is false, running
// on a pre-Windows Vista OS, or the current user is Local System.
// This method does not always launch the repair file because we expect this
// to be called from the repair file, and there is no reason to launch another
// process.
bool LaunchRepairFileElevated(bool is_machine,
                              const TCHAR* repair_file,
                              const TCHAR* args,
                              HRESULT* elevation_hr) {
  ASSERT1(elevation_hr);

  *elevation_hr = S_OK;

  if (!is_machine) {
    UTIL_LOG(L2,  (_T("[user instance - not elevating]")));
    return false;
  }
  if (!vista_util::IsVistaOrLater()) {
    UTIL_LOG(L2,  (_T("[Pre-Windows Vista OS - not elevating]")));
    return false;
  }

  bool is_user_local_system = false;
  HRESULT hr = IsSystemProcess(&is_user_local_system);
  if (SUCCEEDED(hr) && is_user_local_system) {
    UTIL_LOG(L2,  (_T("[User is already SYSTEM - not elevating]")));
    return false;
  }

  HANDLE process = NULL;
  *elevation_hr = msp_executable_elevator::ExecuteGoogleSignedExe(
                                               repair_file,
                                               args,
                                               kHelperInstallerProductGuid,
                                               kHelperPatchGuid,
                                               kHelperPatchName,
                                               &process);
  // Our implementation of msp_executable_elevator does not set the process
  // handle parameter, but we did not remove it from the borrowed code.
  ASSERT1(!process);

  if (FAILED(*elevation_hr)) {
    UTIL_LOG(LE, (_T("[ExecuteGoogleSignedExe failed][error 0x%08x]"),
                  *elevation_hr));
    return false;
  }

  return true;
}

}  // namespace omaha
