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
// This is the implementation of the API for verifying and executing
// an executable under high integrity using an Msi Patch.
//
// This class assumes the following:
//  1) its needed Msi has already been installed,
//  2) its needed Msp is in the same directory as this module,
//  3) the name of the Msp file,
//  4) the name of the property passed as CustomActionData to the custom action,
//  5) the guid of the patch, and
//  6) the guid of the Msi install which will be patched.

#define _WIN32_MSI 300

#include "omaha/recovery/repair_exe/mspexecutableelevator.h"
#include <atlpath.h>
#include <msi.h>
#include "omaha/common/debug.h"
#include "omaha/common/string.h"

namespace omaha {

namespace msp_executable_elevator {

// Used to return information back to the process that called
// ExecuteGoogleSignedExe.
struct SharedMemoryInfo {
  HANDLE process;
  HRESULT launch_result;
};

// Used to store the name of the shared memory.  The name is retrieved from
// the MSP command line when parsing the command line.  The code assumes that
// only one thread per process will call ParseMSPCommandLine followed by
// SetResultOfExecute (which is a safe assumption if this functionality is only
// used for the purpose for which it was originally written).
// Assumes that the MSP is in the same directory as the current process.
static TCHAR parsed_shared_memory_name[200];

HRESULT ExecuteGoogleSignedExe(const TCHAR* exe,
                               const TCHAR* args,
                               const TCHAR* kProductGuid,
                               const TCHAR* kPatchGuid,
                               const TCHAR* kPatchName,
                               HANDLE* process) {
  ASSERT1(exe);
  ASSERT1(args);
  ASSERT1(process);
  ASSERT1(kProductGuid);
  ASSERT1(kPatchGuid);
  ASSERT1(kPatchName);

  // Create shared memory in which to receive result of attempt to launch
  // process and a handle to the launched process.
  HRESULT hr = E_FAIL;
  GUID random_guid = {0};
  TCHAR shared_memory_name[200] = {0};
  if (SUCCEEDED(::CoCreateGuid(&random_guid)) &&
      0 < ::StringFromGUID2(random_guid,
                            shared_memory_name,
                            ARRAYSIZE(shared_memory_name))) {
    HANDLE file_mapping = ::CreateFileMapping(INVALID_HANDLE_VALUE,
                                              NULL,
                                              PAGE_READWRITE,
                                              0,
                                              sizeof(file_mapping),
                                              shared_memory_name);
    if (file_mapping) {
      SharedMemoryInfo* shared_info = reinterpret_cast<SharedMemoryInfo*>
          (::MapViewOfFileEx(file_mapping,
                             FILE_MAP_ALL_ACCESS,
                             0,
                             0,
                             sizeof(*shared_info),
                             0));
      if (shared_info) {
        shared_info->launch_result = E_FAIL;
        shared_info->process = NULL;
        // Create command line to pass to patch.  Parameters are the name of the
        // shared memory just created, the current process id, the executable
        // to launch, and the executable's arguments.
        CString command_line;
        command_line.Format(_T("EXECUTABLECOMMANDLINE=\"%s %u \"\"%s\"\" %s\" ")
                            _T("REINSTALL=ALL"),
                            shared_memory_name,
                            GetCurrentProcessId(),
                            exe,
                            args);
        // Generate path to patch using path to current module.
        TCHAR module_name[MAX_PATH] = {0};
        DWORD len = ::GetModuleFileName(_AtlBaseModule.GetModuleInstance(),
                                        module_name,
                                        ARRAYSIZE(module_name));
        module_name[ARRAYSIZE(module_name) - 1] = '\0';
        if (0 < len && len < ARRAYSIZE(module_name) &&
            0 < command_line.GetLength()) {
          CPath path = module_name;
          if (path.RemoveFileSpec() && path.Append(kPatchName)) {
            path.Canonicalize();
            // Set install level to none so that user does not see
            // an Msi window and so that a restore point is not created.
            ::MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
            UINT res = ::MsiApplyPatch(path,
                                       NULL,
                                       INSTALLTYPE_DEFAULT,
                                       command_line);
            // MsiApplyPatch will not return until the passed executable has
            // been launched (or not) and the shared memory has been updated.
            *process = shared_info->process;
            hr = HRESULT_FROM_WIN32(res);
            if (SUCCEEDED(hr))
              hr = shared_info->launch_result;
          }
        }
        ::MsiRemovePatches(kPatchGuid,
                           kProductGuid,
                           INSTALLTYPE_SINGLE_INSTANCE,
                           NULL);
        VERIFY1(::UnmapViewOfFile(shared_info));
      }
      VERIFY1(::CloseHandle(file_mapping));
    }
  }
  return hr;
}

bool ParseMSPCommandLine(TCHAR* command_line,
                         TCHAR** executable_result,
                         TCHAR** arguments_result,
                         DWORD* calling_process_id) {
  ASSERT1(command_line);
  ASSERT1(executable_result);
  ASSERT1(arguments_result);
  ASSERT1(calling_process_id);
  // Parse command line.  First extract name of shared memory into which
  // the process handle of launched executable will be written.  Then extract
  // the process id of calling process.  This process id will be used to create
  // for the calling process a handle to the launched executable. Finally,
  // extract the path to executable so that we can verify the executable.
  TCHAR* shared_memory_name = NULL;
  TCHAR* process_id_param = NULL;
  TCHAR* arguments = NULL;
  if (SplitCommandLineInPlace(command_line, &shared_memory_name, &arguments) &&
      shared_memory_name && arguments &&
      SplitCommandLineInPlace(arguments, &process_id_param, &arguments) &&
      process_id_param && arguments &&
      SplitCommandLineInPlace(arguments, executable_result, &arguments) &&
      *executable_result && arguments) {
    *calling_process_id = static_cast<DWORD>(_wtoi(process_id_param));
    *arguments_result = arguments;
    _tcsncpy(parsed_shared_memory_name,
             shared_memory_name,
             ARRAYSIZE(parsed_shared_memory_name));
    parsed_shared_memory_name[ARRAYSIZE(parsed_shared_memory_name) - 1] =
        _T('\0');
    return true;
  }
  return false;
}

// Copy process handle and result to shared memory.
// Process can be NULL.
bool SetResultOfExecute(HANDLE process, HRESULT result) {
  bool success = false;
  if (_T('\0') != *parsed_shared_memory_name) {
    HANDLE file_mapping = ::OpenFileMapping(FILE_MAP_WRITE,
                                            FALSE,
                                            parsed_shared_memory_name);
    if (file_mapping) {
      SharedMemoryInfo* shared_info = reinterpret_cast<SharedMemoryInfo*>
          (::MapViewOfFileEx(file_mapping,
                             FILE_MAP_WRITE,
                             0,
                             0,
                             sizeof(SharedMemoryInfo),
                             0));
      if (shared_info) {
        shared_info->process = process;
        shared_info->launch_result = result;
        VERIFY1(::UnmapViewOfFile(shared_info));
        success = true;
      } else {
        ASSERT(false, (_T("::MapViewOfFileEx failed.")));
      }

      VERIFY1(::CloseHandle(file_mapping));
    } else {
      ASSERT(false, (_T("::OpenFileMapping failed.")));
    }
  } else {
    ASSERT(false, (_T("parsed_shared_memory_name is empty.")));
  }
  return success;
}

}  // namespace msp_executable_elevator

}  // namespace omaha

