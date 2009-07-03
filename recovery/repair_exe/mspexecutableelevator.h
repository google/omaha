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
// This is the API for verifying and executing an executable under high
// integrity using an Msi Patch.  This API assumes its needed Msi has already
// been installed and its needed Msp is in the same directory as this module.
//
// This class encapsulates the code for passing information between a process
// requesting that an executable be elevated and the custom action DLL
// in the patch which actually elevates the executable.

#ifndef OMAHA_RECOVERY_REPAIR_EXE_MSPEXECUTABLEELEVATOR_H__
#define OMAHA_RECOVERY_REPAIR_EXE_MSPEXECUTABLEELEVATOR_H__

#include <windows.h>
#include <tchar.h>

namespace omaha {

namespace msp_executable_elevator {

// The following function should be called by the code requesting that
// an executable be elevated:

// Use an MSI patch to verify and execute an executable.  Returns a handle
// to the process executed.
HRESULT ExecuteGoogleSignedExe(const TCHAR* executable,
                               const TCHAR* arguments,
                               const TCHAR* kProductGuid,
                               const TCHAR* kPatchGuid,
                               const TCHAR* kPatchName,
                               HANDLE* process);

// The following functions should be called by the code (i.e., the custom action
// DLL) that actually elevates the executable:

// From the command line passed to the MSP, retrieve the parameters that will be
// passed to VerifyFileAndExecute.
// This function is destructive to the passed command line buffer.  The pointers
// "executable" and "arguments" will point into the command line buffer.
bool ParseMSPCommandLine(TCHAR* command_line,
                         TCHAR** executable,
                         TCHAR** arguments,
                         DWORD* calling_process_id);

// Records the result of the call to VerifyFileAndExecute.
bool SetResultOfExecute(HANDLE process, HRESULT result);

}  // namespace msp_executable_elevator

}  // namespace omaha

#endif  // OMAHA_RECOVERY_REPAIR_EXE_MSPEXECUTABLEELEVATOR_H__
