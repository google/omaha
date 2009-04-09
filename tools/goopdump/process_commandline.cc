// Copyright 2008-2009 Google Inc.
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

#include "omaha/tools/goopdump/process_commandline.h"

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/utils.h"

namespace omaha {

namespace {

typedef const wchar_t* (__stdcall *GetCommandLineWFunc)();

const DWORD kMaxInjectSize = 4096;
const DWORD kCmdLineBufferSize = 2000;

struct SharedBlock {
  DWORD last_error;  // Last error from remote thread.
  GetCommandLineWFunc get_commandline_w_ptr;  // Address of GetCommandLineW().
  wchar_t cmd_line_buffer[kCmdLineBufferSize];  // The command line buffer.
};

// A number of assumptions are made:
// * The target process is a Win32 process.
// * Kernel32.dll is loaded at same address in each process (safe).
// * InjectFunction() is shorter than kMaxInjectSize
// * InjectFunction() does not rely on the C/C++ runtime.
// * Compiler option /GZ is not used. (If it is, the compiler generates calls to
//   functions which are not injected into the target.  The runtime_checks()
//   pragma below removes this for debug builds which will have /GZ enabled by
//   default.

#pragma runtime_checks("", off)

DWORD __stdcall InjectFunction(SharedBlock* shared_block) {
  const wchar_t* source = shared_block->get_commandline_w_ptr();
  wchar_t* target = &shared_block->cmd_line_buffer[0];
  wchar_t* end = &shared_block->cmd_line_buffer[
      arraysize(shared_block->cmd_line_buffer) - 1];
  if (source == 0 || target == 0 || end == 0) {
    shared_block->last_error = ERROR_INVALID_DATA;
    return 0;
  }

  // This is effectively a wcscpy but we can't use library functions since this
  // code will be injected into the target process space and we can't make
  // assumptions about what's linked into that process.
  for (; *source != 0 && target < end; ++source, ++target) {
    *target = *source;
  }

  *target = 0;
  shared_block->last_error = 0;

  return 0;
}

#pragma runtime_checks("", restore)

// Internal helper function to deal with the logic of injecting the
// function/data into the process without the memory alloc/free since we don't
// have smart pointers to handle VirtualAllocEx.
HRESULT GetCommandLineFromHandleWithMemory(HANDLE process_handle,
                                           void* function_memory,
                                           SharedBlock* shared_block,
                                           CString* command_line) {
  ASSERT1(process_handle);
  ASSERT1(function_memory);
  ASSERT1(shared_block);

  // Copy function into other process.
  if (!::WriteProcessMemory(process_handle,
                            function_memory,
                            &InjectFunction,
                            kMaxInjectSize,
                            0)) {
    return HRESULTFromLastError();
  }

  // Initialize data area for remote process.
  scoped_library hmodule_kernel32(::LoadLibrary(L"kernel32.dll"));
  if (!hmodule_kernel32) {
    return HRESULTFromLastError();
  }

  SharedBlock shared_block_local;
  shared_block_local.last_error = 0;
  if (!GPA(get(hmodule_kernel32),
           "GetCommandLineW",
           &shared_block_local.get_commandline_w_ptr)) {
    return HRESULTFromLastError();
  }

  shared_block_local.cmd_line_buffer[0] = L'\0';

  if (!::WriteProcessMemory(process_handle,
                            shared_block,
                            &shared_block_local,
                            sizeof(shared_block_local),
                            0)) {
    return HRESULTFromLastError();
  }

  // Create the remote thread.
  scoped_handle remote_thread;
  reset(remote_thread, ::CreateRemoteThread(
      process_handle,
      0,
      0,
      reinterpret_cast<LPTHREAD_START_ROUTINE>(function_memory),
      shared_block,
      0,
      NULL));
  if (!remote_thread) {
    return HRESULTFromLastError();
  }

  const DWORD kWaitTimeoutMs = 3000;
  DWORD wait_result = ::WaitForSingleObject(get(remote_thread), kWaitTimeoutMs);
  switch (wait_result) {
    case WAIT_TIMEOUT:
      return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    case WAIT_FAILED:
      return HRESULTFromLastError();
    case WAIT_OBJECT_0:
      // This might just have worked, pick up the result.
      if (!::ReadProcessMemory(process_handle,
                               shared_block,
                               &shared_block_local,
                               sizeof(shared_block_local),
                               0)) {
        return HRESULTFromLastError();
      }
      if (shared_block_local.last_error == 0) {
        *command_line = shared_block_local.cmd_line_buffer;
      } else {
        return HRESULT_FROM_WIN32(shared_block_local.last_error);
      }
      break;
    default:
      return HRESULTFromLastError();
  }

  return S_OK;
}

// Allocates memory in a remote process given the process handle.
// Returns a block of memory for the function that will be injected and a block
// of memory to store results in that we copy back out.
HRESULT AllocateProcessMemory(HANDLE process_handle,
                              void** function_memory,
                              void** shared_block) {
  ASSERT1(process_handle);
  ASSERT1(function_memory);
  ASSERT1(shared_block);

  *function_memory = ::VirtualAllocEx(process_handle,
                                      0,
                                      kMaxInjectSize,
                                      MEM_COMMIT,
                                      PAGE_EXECUTE_READWRITE);
  if (!(*function_memory)) {
    return HRESULTFromLastError();
  }

  *shared_block = ::VirtualAllocEx(process_handle,
                                  0,
                                  sizeof(SharedBlock),
                                  MEM_COMMIT,
                                  PAGE_READWRITE);
  if (!(*shared_block)) {
    ::VirtualFreeEx(process_handle, *function_memory, 0, MEM_RELEASE);
    *function_memory = NULL;
    return HRESULTFromLastError();
  }

  return S_OK;
}

HRESULT GetCommandLineFromHandle(HANDLE process_handle, CString* command_line) {
  ASSERT1(command_line);
  ASSERT1(process_handle != NULL);

  void* function_memory = NULL;
  void* shared_block = NULL;

  HRESULT hr = AllocateProcessMemory(process_handle,
                                     &function_memory,
                                     &shared_block);

  if (SUCCEEDED(hr) && function_memory && shared_block) {
    hr = GetCommandLineFromHandleWithMemory(
        process_handle,
        function_memory,
        reinterpret_cast<SharedBlock*>(shared_block),
        command_line);
  }

  if (function_memory) {
    ::VirtualFreeEx(process_handle, function_memory, 0, MEM_RELEASE);
    function_memory = NULL;
  }

  if (shared_block) {
    ::VirtualFreeEx(process_handle, shared_block, 0, MEM_RELEASE);
    shared_block = NULL;
  }

  return hr;
}

}  // namespace

HRESULT GetProcessCommandLine(DWORD process_id, CString* command_line) {
  ASSERT1(command_line);

  EnableDebugPrivilege();
  scoped_handle process_handle;
  reset(process_handle, ::OpenProcess(PROCESS_CREATE_THREAD |
                                      PROCESS_VM_OPERATION |
                                      PROCESS_VM_WRITE |
                                      PROCESS_VM_READ,
                                      FALSE,
                                      process_id));
  if (!valid(process_handle)) {
    return HRESULTFromLastError();
  }
  return GetCommandLineFromHandle(get(process_handle), command_line);
}

bool EnableDebugPrivilege() {
  scoped_handle token;

  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          address(token))) {
    return false;
  }

  LUID se_debug_name_value = {0};
  if (!::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &se_debug_name_value)) {
    return false;
  }

  TOKEN_PRIVILEGES token_privs = {0};
  token_privs.PrivilegeCount = 1;
  token_privs.Privileges[0].Luid = se_debug_name_value;
  token_privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  if (!::AdjustTokenPrivileges(get(token),
                               FALSE,
                               &token_privs,
                               sizeof(token_privs),
                               NULL,
                               NULL)) {
    return false;
  }

  return true;
}

}  // namespace omaha

