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
// Verifies and executes the repair file for the MSP custom action.

#ifndef OMAHA_RECOVERY_REPAIR_EXE_CUSTOM_ACTION_EXECUTE_REPAIR_FILE_H__
#define OMAHA_RECOVERY_REPAIR_EXE_CUSTOM_ACTION_EXECUTE_REPAIR_FILE_H__

#include <windows.h>
#include <atlstr.h>

namespace omaha {

// Verifies the repair file and executes it.
HRESULT VerifyFileAndExecute(const CString& filename, const CString& args);

}  // namespace omaha

#endif  // OMAHA_RECOVERY_REPAIR_EXE_CUSTOM_ACTION_EXECUTE_REPAIR_FILE_H__
