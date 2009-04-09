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
//
// Tool to crash the specified process.
// Injects a remote thread into the named process and causes
// this thread to crash.
// Options:
// 1. Crash by injecting a thread that tries to execute a non-existent code
//    location.
// 2. Crash by Raising a specific exception.
// TODO(omaha): Figure out a way to implement:
// 3. Crash another thread of the process.
// 4. Crash another thread when it enters a particular function.
// 5. Crash another thread when it leaves a particular function.
//
// For now the process name and the address of the crash are hard coded.

#include <windows.h>
#include <wtypes.h>
#include <tchar.h>
#include <psapi.h>

const int kMaxProcesses = 1024;

int _tmain(int argc, TCHAR* argv[]) {
  if (2 != argc) {
    wprintf(_T("Incorrect syntax. The single required argument is the ")
            _T("executable to crash.\n"));
    return -1;
  }

  const TCHAR* target_process_name = argv[1];

  DWORD num_processes = 0;
  DWORD process_ids[kMaxProcesses] = {0};
  if (!::EnumProcesses(process_ids, kMaxProcesses, &num_processes)) {
    wprintf(_T("EnumProcesses failed.\n"));
    return -1;
  }

  wprintf(_T("Found %u processes.\n"), num_processes);
  bool found = false;
  for (size_t i = 0; i < num_processes && !found; ++i) {
    const DWORD access_rights = PROCESS_CREATE_THREAD |
                                PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE |
                                PROCESS_VM_READ;
    HANDLE process_handle = ::OpenProcess(access_rights,
                                          FALSE,
                                          process_ids[i]);
    if (process_handle) {
      TCHAR process_name[1024] = {0};
      if (::GetProcessImageFileName(process_handle, process_name, 1024) != 0) {
        size_t len = wcslen(target_process_name);
        size_t total_len = wcslen(process_name);
        int offset = total_len - len;
        if (offset >= 0 &&
            _wcsicmp(&(process_name[offset]), target_process_name) == 0) {
          found = true;
          wprintf(_T("Found %s %u. Injecting a crash.\n"),
                  target_process_name, process_ids[i]);
          DWORD thread_id = 0;
          LPTHREAD_START_ROUTINE func =
              reinterpret_cast<LPTHREAD_START_ROUTINE>(0x12345678);
          void* addr_of_param = reinterpret_cast<void*>(0x2491ed8);
          HANDLE thread_handle = ::CreateRemoteThread(process_handle,
                                                      NULL,
                                                      0,
                                                      func,
                                                      addr_of_param,
                                                      CREATE_SUSPENDED,
                                                      &thread_id);
          if (!thread_handle) {
            HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
            wprintf(_T("CreateRemoteThread failed.\n"));
          }
          if (::ResumeThread(thread_handle) == -1) {
            wprintf(_T("Resume thread failed.\n"));
          } else {
            wprintf(_T("Waiting for process to quit.\n"));
            ::WaitForSingleObject(thread_handle, 2000);
          }
        }
      }

      ::CloseHandle(process_handle);
    }
  }

  if (!found) {
    wprintf(_T("No %s processes found.\n"), target_process_name);
  } else {
    wprintf(_T("Done.\n"));
  }

  return 0;
}

