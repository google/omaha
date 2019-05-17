// Copyright 2004-2010 Google Inc.
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
// system functions for checking disk space / memory usage / etc.

#ifndef OMAHA_BASE_SYSTEM_H_
#define OMAHA_BASE_SYSTEM_H_

#include <atlstr.h>
#include <base/basictypes.h>

namespace omaha {

#define kMaxRegistryBackupWaitMs 3000
#define kMaxRegistryRestoreWaitMs 30000

// amount of time the user must have no input before we declare them idle
// used by outlook addin, outlook_cap, and filecap.
#define kUserIdleThresholdMs 30000

// The user is busy typing or interacting with another application
// if the user is below the minimum threshold.
#define kUserIdleMinThresholdMs 30000

// The user is probably not paying attention
// if the user is above the maximum threshold.
#define kUserIdleMaxThresholdMs 600000

const DWORD kInvalidSessionId = 0xFFFFFFFF;

class System {
  public:

    // disk statistics.

    // disk total and free space.
    // Path is either the root of a drive or an existing folder on a drive; the
    // statistics are for that drive.
    static HRESULT GetDiskStatistics(const TCHAR* path,
                                     uint64 *free_bytes_current_user,
                                     uint64 *total_bytes_current_user,
                                     uint64 *free_bytes_all_users);

    // The three functions below start background processes via ::CreateProcess.
    // Use the ShellExecuteProcessXXX functions when starting foreground
    // processes.
    static HRESULT StartProcessWithArgs(const TCHAR* process_name,
                                        const TCHAR* cmd_line_arguments);
    static HRESULT StartProcessWithArgsAndInfo(const TCHAR* process_name,
                                               const TCHAR* cmd_line_arguments,
                                               PROCESS_INFORMATION* pi);
    static HRESULT StartProcessWithArgsAndInfoWithEnvironment(
        const TCHAR* process_name,
        const TCHAR* cmd_line_arguments,
        LPVOID env_block,
        PROCESS_INFORMATION* pi);

    static HRESULT StartCommandLine(const TCHAR* command_line_to_execute);

    static HRESULT StartProcess(const TCHAR* process_name,
                                TCHAR* command_line,
                                PROCESS_INFORMATION* pi);
    static HRESULT StartProcessWithEnvironment(
        const TCHAR* process_name,
        TCHAR* command_line,
        LPVOID env_block,
        PROCESS_INFORMATION* pi);
    static HRESULT StartProcessWithEnvironment(
        const TCHAR* process_name,
        TCHAR* command_line,
        LPVOID env_block,
        BOOL inherit_handles,
        PROCESS_INFORMATION* pi);

    // This function is similar with the function above, and it allows an
    // optional environment block to be passed as an argument.
    // When provided, the environment block should match the user which
    // the user_token belongs to. The function is not enforcing this match,
    // however the behavior of the newly created process could be undefined if
    // the wrong environment was provided.
    static HRESULT StartProcessAsUserWithEnvironment(
        HANDLE user_token,
        const CString& executable_path,
        const CString& parameters,
        const TCHAR* desktop,
        LPVOID env_block,
        PROCESS_INFORMATION* pi);

    // start another process painlessly via ::ShellExecuteEx. Use this method
    // instead of the StartProcessXXX methods that use ::CreateProcess where
    // possible, since ::ShellExecuteEx has better behavior on Vista.
    static HRESULT ShellExecuteProcess(const TCHAR* file_name_to_execute,
                                       const TCHAR* command_line_parameters,
                                       HWND hwnd,
                                       HANDLE* process_handle);

    // memory statistics.

    // global memory stats
    static HRESULT GetGlobalMemoryStatistics(
                       uint32 *memory_load_percentage,
                       uint64 *free_physical_memory,
                       uint64 *total_physical_memory,
                       uint64 *free_paged_memory,
                       uint64 *total_paged_memory,
                       uint64 *process_free_virtual_memory,
                       uint64 *process_total_virtual_memory);

    // process memory stats
    static HRESULT GetProcessMemoryStatistics(uint64 *current_working_set,
                                              uint64 *peak_working_set,
                                              uint64 *min_working_set_size,
                                              uint64 *max_working_set_size);

    // TODO(omaha): determine if using this where we do with machines
    // with slow disks causes noticeable slowdown

    // reduce process working set - beware of possible negative performance
    // implications - this function frees (to the page cache) all used pages,
    // minimizing the working set - but could lead to additional page faults
    // when the process continues. If the process continues soon enough the
    // pages will still be in the page cache so they'll be relatively cheap
    // soft page faults. This function is best used to reduce memory footprint
    // when a component is about to go idle for "awhile".
    static HRESULT EmptyProcessWorkingSet();

    // from ntddk.h, used as a parameter to get the process handle count.
    static const int kProcessHandleCount = 20;
    static uint32 GetProcessHandleCount();

    // Create an instance of a COM Local Server class using either plain vanilla
    // CoCreateInstance, or using the Elevation moniker if Vista or later and
    // the user is not an elevated admin.
    static HRESULT CoCreateInstanceAsAdmin(HWND hwnd,
                                           REFCLSID rclsid,
                                           REFIID riid,
                                           void** ppv);

    // Attempts to adjust current process privileges.
    // Only process running with administrator privileges will succeed.
    static HRESULT AdjustPrivilege(const TCHAR* privilege, bool enable);

    // Dynamically links and calls ::WTSGetActiveConsoleSessionId(). Returns
    // kInvalidSessionId if it cannot find the export in kernel32.dll.
    static DWORD WTSGetActiveConsoleSessionId();

    // Get the session the current process is running under.
    static DWORD GetCurrentSessionId();

    // Get the best guess as to the currently active session,
    // or kInvalidSessionId if there is no active session.
    static DWORD GetActiveSessionId();

    // Is there a user logged on and active in the specified session?
    static bool IsSessionActive(DWORD session_id);

    // Returns true if a system battery is detected and the AC line
    // status is 'offline', otherwise it returns false.
    static bool IsRunningOnBatteries();

    // Creates an anonymous pipe whose write HANDLE is inheritable. Both pipes
    // are rendered accessible to System, Admins, and the current user.
    static HRESULT CreateChildOutputPipe(HANDLE* read, HANDLE* write);

  private:
    DISALLOW_COPY_AND_ASSIGN(System);
};

}  // namespace omaha

#endif  // OMAHA_BASE_SYSTEM_H_

