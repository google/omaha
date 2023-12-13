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

#include "omaha/base/system.h"

#include <objidl.h>
#include <psapi.h>
#include <winternl.h>
#include <wtsapi32.h>
#include "omaha/base/app_util.h"
#include "omaha/base/commands.h"
#include "omaha/base/commontypes.h"
#include "omaha/base/const_config.h"
#include "omaha/base/debug.h"
#include "omaha/base/dynamic_link_kernel32.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/string.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"

namespace omaha {

namespace {

// Disables critical error dialogs on the current thread.
// The system does not display the critical-error-handler message box.
// Instead, the system returns the error to the calling process.
class DisableThreadErrorUI {
 public:
  DisableThreadErrorUI() {
    // Set the error mode
    prev_mode_ = SetErrorMode(SEM_FAILCRITICALERRORS);
  }

  ~DisableThreadErrorUI() {
    // Restore the error mode
    SetErrorMode(prev_mode_);
  }

 protected:
  UINT prev_mode_;
};

}  // namespace

HRESULT System::GetDiskStatistics(const TCHAR* path,
                                  uint64 *free_bytes_current_user,
                                  uint64 *total_bytes_current_user,
                                  uint64 *free_bytes_all_users) {
  ASSERT1(path);
  ASSERT1(free_bytes_current_user);
  ASSERT1(total_bytes_current_user);
  ASSERT1(free_bytes_all_users);
  ASSERT1(sizeof(LARGE_INTEGER) == sizeof(uint64));  // NOLINT

  DisableThreadErrorUI disable_error_dialog_box;

  if (!::GetDiskFreeSpaceEx(
           path,
           reinterpret_cast<PULARGE_INTEGER>(free_bytes_current_user),
           reinterpret_cast<PULARGE_INTEGER>(total_bytes_current_user),
           reinterpret_cast<PULARGE_INTEGER>(free_bytes_all_users))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Failed to GetDiskFreeSpaceEx][%s][0x%x]"), path, hr));
    return hr;
  }

  return S_OK;
}

HRESULT System::GetProcessMemoryStatistics(uint64 *current_working_set,
                                           uint64 *peak_working_set,
                                           uint64 *min_working_set_size,
                                           uint64 *max_working_set_size) {
  HANDLE process_handle = GetCurrentProcess();
  HRESULT hr = S_OK;

  SIZE_T min_size(0), max_size(0);
  if (GetProcessWorkingSetSize(process_handle, &min_size, &max_size)) {
    UTIL_LOG(L2, (_T("[working set][min: %Iu][max: %Iu]"), min_size, max_size));
    if (min_working_set_size) {
      *min_working_set_size = min_size;
    }
    if (max_working_set_size) {
      *max_working_set_size = max_size;
    }
  } else {
    if (min_working_set_size) {
      *min_working_set_size = 0;
    }
    if (max_working_set_size) {
      *max_working_set_size = 0;
    }
    hr = E_FAIL;
  }

  if (current_working_set) { *current_working_set = 0; }
  if (peak_working_set) { *peak_working_set = 0; }

  // including this call (w/psapi.lib) adds 24k to the process memory
  // according to task manager in one test, memory usage according to task
  // manager increased by 4k after calling this
  PROCESS_MEMORY_COUNTERS counters = { sizeof(counters), 0 };
  if (GetProcessMemoryInfo(process_handle,
                           &counters,
                           sizeof(PROCESS_MEMORY_COUNTERS))) {
    UTIL_LOG(L2, (_T("[working set][current: %s][peak: %s]"),
                  String_Uint64ToString(counters.WorkingSetSize, 10),
                  String_Uint64ToString(counters.PeakWorkingSetSize, 10)));
    if (current_working_set) {
      *current_working_set = counters.WorkingSetSize;
    }
    if (peak_working_set) {
      *peak_working_set = counters.PeakWorkingSetSize;
    }
  } else {
    if (current_working_set) {
      *current_working_set = 0;
    }
    if (peak_working_set) {
      *peak_working_set = 0;
    }
    hr = E_FAIL;
  }

  return hr;
}

HRESULT System::GetGlobalMemoryStatistics(uint32 *memory_load_percentage,
                                          uint64 *free_physical_memory,
                                          uint64 *total_physical_memory,
                                          uint64 *free_paged_memory,
                                          uint64 *total_paged_memory,
                                          uint64 *process_free_virtual_memory,
                                          uint64 *process_total_virtual_mem) {
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  if (!GlobalMemoryStatusEx(&status)) {
    UTIL_LOG(LEVEL_ERROR, (_T("memory status error %u"), GetLastError()));
    return E_FAIL;
  }
  if (memory_load_percentage) { *memory_load_percentage = status.dwMemoryLoad; }
  if (free_physical_memory) { *free_physical_memory = status.ullAvailPhys; }
  if (total_physical_memory) { *total_physical_memory = status.ullTotalPhys; }
  if (free_paged_memory) { *free_paged_memory = status.ullAvailPageFile; }
  if (total_paged_memory) { *total_paged_memory = status.ullTotalPageFile; }
  if (process_free_virtual_memory) {
    *process_free_virtual_memory = status.ullAvailVirtual;
  }
  if (process_total_virtual_mem) {
    *process_total_virtual_mem = status.ullTotalVirtual;
  }
  // GetPerformanceInfo;
  return S_OK;
}

HRESULT System::EmptyProcessWorkingSet() {
  // -1,-1 is a special signal to the OS to temporarily trim the working set
  // size to 0.  See MSDN for further information.
  if (0 == ::SetProcessWorkingSetSize(::GetCurrentProcess(),
                                      static_cast<SIZE_T>(-1),
                                      static_cast<SIZE_T>(-1))) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

// Adaptor for System::StartProcessWithEnvironment().
HRESULT System::StartProcess(const TCHAR* process_name,
                             TCHAR* command_line,
                             PROCESS_INFORMATION* pi) {
  return System::StartProcessWithEnvironment(process_name,
                                             command_line,
                                             NULL,
                                             pi);
}

HRESULT System::StartProcessWithEnvironment(
    const TCHAR* process_name,
    TCHAR* command_line,
    LPVOID env_block,
    PROCESS_INFORMATION* pi) {
  return System::StartProcessWithEnvironment(process_name,
                                             command_line,
                                             env_block,
                                             FALSE,
                                             pi);
}

// start another process painlessly via ::CreateProcess. Use the
// ShellExecuteProcessXXX variants instead of these methods where possible,
// since ::ShellExecuteEx has better behavior on Windows Vista.
// When using this method, avoid using process_name - see
// http://blogs.msdn.com/oldnewthing/archive/2006/05/15/597984.aspx.
HRESULT System::StartProcessWithEnvironment(
    const TCHAR* process_name,
    TCHAR* command_line,
    LPVOID env_block,
    BOOL inherit_handles,
    PROCESS_INFORMATION* pi) {
  ASSERT1(pi);
  ASSERT1(command_line || process_name);
  ASSERT(!process_name, (_T("Avoid using process_name. See method comment.")));

  // Prevents elevation of privilege by reverting to the process token before
  // starting the process. Otherwise, a lower privilege token could for instance
  // symlink `C:\` to a different folder (per-user DosDevice) and allow an
  // elevation of privilege attack.
  scoped_revert_to_self revert_to_self;

  STARTUPINFO si = {sizeof(si), 0};

  // Feedback cursor is off while the process is starting.
  si.dwFlags = STARTF_FORCEOFFFEEDBACK;

  UTIL_LOG(L3, (_T("[System::StartProcessWithEnvironment][process %s][cmd %s]"),
                process_name, command_line));

  BOOL success = FALSE;
  if (env_block == NULL) {
    success = ::CreateProcess(
        process_name,     // Module name
        command_line,     // Command line
        NULL,             // Process handle not inheritable
        NULL,             // Thread handle not inheritable
        inherit_handles,  // Set handle inheritance to FALSE
        0,                // No creation flags
        NULL,             // Use parent's environment block
        NULL,             // Use parent's starting directory
        &si,              // Pointer to STARTUPINFO structure
        pi);              // Pointer to PROCESS_INFORMATION structure
  } else {
    success = ::CreateProcess(
        process_name,     // Module name
        command_line,     // Command line
        NULL,             // Process handle not inheritable
        NULL,             // Thread handle not inheritable
        inherit_handles,  // Set handle inheritance to FALSE
        CREATE_UNICODE_ENVIRONMENT,  // Creation flags
        env_block,        // Environment block
        NULL,             // Use parent's starting directory
        &si,              // Pointer to STARTUPINFO structure
        pi);              // Pointer to PROCESS_INFORMATION structure
  }

  if (!success) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[System::StartProcess][::CreateProcess failed][0x%x]"), hr));
    return hr;
  }

  OPT_LOG(L1, (_T("[Started process][%u]"), pi->dwProcessId));

  return S_OK;
}

// Adaptor for System::StartProcessWithArgsAndInfoWithEnvironment().
HRESULT System::StartProcessWithArgsAndInfo(const TCHAR *process_name,
                                            const TCHAR *cmd_line_arguments,
                                            PROCESS_INFORMATION *pi) {
  return System::StartProcessWithArgsAndInfoWithEnvironment(process_name,
                                                            cmd_line_arguments,
                                                            NULL,
                                                            pi);
}

// start another process painlessly via ::CreateProcess. Use the
// ShellExecuteProcessXXX variants instead of these methods where possible,
// since ::ShellExecuteEx has better behavior on Windows Vista.
HRESULT System::StartProcessWithArgsAndInfoWithEnvironment(
    const TCHAR* process_name,
    const TCHAR* cmd_line_arguments,
    LPVOID env_block,
    PROCESS_INFORMATION* pi) {
  ASSERT1(process_name && cmd_line_arguments && pi);

  CString command_line(process_name);
  EnclosePath(&command_line);
  command_line.AppendChar(_T(' '));
  command_line.Append(cmd_line_arguments);
  return System::StartProcessWithEnvironment(NULL,
                                             CStrBuf(command_line),
                                             env_block,
                                             pi);
}

// start another process painlessly via ::CreateProcess. Use the
// ShellExecuteProcessXXX variants instead of these methods where possible,
// since ::ShellExecuteEx has better behavior on Windows Vista.
HRESULT System::StartProcessWithArgs(const TCHAR *process_name,
                                     const TCHAR *cmd_line_arguments) {
  ASSERT1(process_name && cmd_line_arguments);
  PROCESS_INFORMATION pi = {0};
  HRESULT hr = System::StartProcessWithArgsAndInfo(process_name,
                                                   cmd_line_arguments,
                                                   &pi);
  if (SUCCEEDED(hr)) {
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
  }
  return hr;
}

HRESULT System::StartCommandLine(const TCHAR* command_line_to_execute) {
  ASSERT1(command_line_to_execute);

  CString command_line(command_line_to_execute);
  PROCESS_INFORMATION pi = {0};
  HRESULT hr = System::StartProcess(NULL, CStrBuf(command_line), &pi);
  if (SUCCEEDED(hr)) {
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
  }
  return hr;
}

// TODO(omaha3): Unit test this method.
HRESULT System::StartProcessAsUserWithEnvironment(
    HANDLE user_token,
    const CString& executable_path,
    const CString& parameters,
    const TCHAR* desktop,
    LPVOID env_block,
    PROCESS_INFORMATION* pi) {
  UTIL_LOG(L3, (_T("[StartProcessAsUserWithEnvironment][%s][%s][%s]"),
                executable_path, parameters, desktop));
  ASSERT1(pi);

  // Prevents elevation of privilege by reverting to the process token before
  // starting the process. Otherwise, a lower privilege token could for instance
  // symlink `C:\` to a different folder (per-user DosDevice) and allow an
  // elevation of privilege attack.
  scoped_revert_to_self revert_to_self;

  CString cmd(executable_path);
  EnclosePath(&cmd);
  cmd.AppendChar(_T(' '));
  cmd.Append(parameters);

  STARTUPINFO startup_info = { sizeof(startup_info) };
  startup_info.lpDesktop = const_cast<TCHAR*>(desktop);
  DWORD creation_flags(0);

  creation_flags |= CREATE_UNICODE_ENVIRONMENT;
  BOOL success = ::CreateProcessAsUser(user_token,
                                       0,
                                       CStrBuf(cmd, MAX_PATH),
                                       0,
                                       0,
                                       false,
                                       creation_flags,
                                       env_block,
                                       0,
                                       &startup_info,
                                       pi);
  if (!success) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[::CreateProcessAsUser failed][%s][0x%x]"), cmd, hr));
    return hr;
  }

  return S_OK;
}

// start another process painlessly via ::ShellExecuteEx. Use this method
// instead of the StartProcessXXX methods that use ::CreateProcess where
// possible, since ::ShellExecuteEx has better behavior on Windows Vista.
//
// ShellExecuteExEnsureParent displays the PID of the started process if it is
// returned. It is only returned if the mask includes SEE_MASK_NOCLOSEPROCESS.
// Therefore, we always set this flag and pass a handle. If the caller did not
// request the handle, we close it.
HRESULT System::ShellExecuteProcess(const TCHAR* file_name_to_execute,
                                    const TCHAR* command_line_parameters,
                                    HWND hwnd,
                                    HANDLE* process_handle) {
  ASSERT1(file_name_to_execute);

  UTIL_LOG(L3, (_T("[System::ShellExecuteProcess]")
                _T("[file_name_to_execute '%s' command_line_parameters '%s']"),
                file_name_to_execute, command_line_parameters));

  SHELLEXECUTEINFO sei = {0};
  sei.cbSize = sizeof(sei);
  // SEE_MASK_NOZONECHECKS is set below to work around a problem in systems that
  // had Internet Explorer 7 Beta installed. See http://b/804674.
  // This only works for Windows XP SP1 and later.
  sei.fMask = SEE_MASK_NOCLOSEPROCESS |  // Set hProcess to process handle.
              SEE_MASK_FLAG_NO_UI     |  // Do not display an error message box.
              SEE_MASK_NOZONECHECKS   |  // Do not perform a zone check.
              SEE_MASK_NOASYNC;          // Wait to complete before returning.
  sei.lpVerb = _T("open");
  sei.lpFile = file_name_to_execute;
  sei.lpParameters = command_line_parameters;
  sei.nShow = SW_SHOWNORMAL;
  sei.hwnd = hwnd;

  // Use ShellExecuteExEnsureParent to ensure that we always have a parent
  // window. We need to use the HWND property to be acknowledged as a foreground
  // application on Windows Vista. Otherwise, the elevation prompt will appear
  // minimized on the taskbar.
  if (!ShellExecuteExEnsureParent(&sei)) {
    HRESULT hr(HRESULTFromLastError());
    OPT_LOG(LEVEL_ERROR, (_T("[Failed to ::ShellExecuteEx][%s][%s][0x%08x]"),
                          file_name_to_execute, command_line_parameters, hr));
    return hr;
  }

  if (process_handle) {
    *process_handle = sei.hProcess;
  } else {
    ::CloseHandle(sei.hProcess);
  }

  return S_OK;
}

uint32 System::GetProcessHandleCount() {
  typedef LONG (CALLBACK *Fun)(HANDLE, int32, PVOID, ULONG, PULONG);

  // This new version of getting the number of open handles works on win2k.
  HMODULE module = GetModuleHandle(_T("ntdll.dll"));
  if (!module) {
    return 0;
  }
  Fun NtQueryInformationProcess =
      reinterpret_cast<Fun>(::GetProcAddress(module,
                                             "NtQueryInformationProcess"));

  if (!NtQueryInformationProcess) {
    UTIL_LOG(LEVEL_ERROR, (_T("[NtQueryInformationProcess failed][0x%x]"),
                           HRESULTFromLastError()));
    return 0;
  }

  DWORD count = 0;
  VERIFY(NtQueryInformationProcess(GetCurrentProcess(),
                                   kProcessHandleCount,
                                   &count,
                                   sizeof(count),
                                   NULL) >= 0, (L""));
  return count;
}

HRESULT System::CoCreateInstanceAsAdmin(HWND hwnd,
                                        REFCLSID rclsid,
                                        REFIID riid,
                                        void** ppv) {
  UTIL_LOG(L6, (_T("[CoCreateInstanceAsAdmin][%d][%s][%s]"),
                hwnd, GuidToString(rclsid), GuidToString(riid)));

  if (vista_util::IsUserAdmin()) {
    return ::CoCreateInstance(rclsid, NULL, CLSCTX_LOCAL_SERVER, riid, ppv);
  }

  if (!SystemInfo::IsRunningOnVistaOrLater()) {
    return E_ACCESSDENIED;
  }

  CString moniker_name(_T("Elevation:Administrator!new:"));
  moniker_name += GuidToString(rclsid);
  BIND_OPTS3 bo;
  SetZero(bo);
  bo.cbStruct = sizeof(bo);
  bo.hwnd = hwnd;
  bo.dwClassContext = CLSCTX_LOCAL_SERVER;

  return ::CoGetObject(moniker_name, &bo, riid, ppv);
}


// Attempts to adjust current process privileges.
// Only process running with administrator privileges will succeed.
HRESULT System::AdjustPrivilege(const TCHAR* privilege, bool enable) {
  ASSERT1(privilege);

  scoped_handle token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          address(token))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::AdjustPrivilege - failed to ")
                           _T("OpenProcessToken][0x%x]"), hr));
    return hr;
  }

  LUID luid = {0};
  if (!::LookupPrivilegeValue(NULL, privilege, &luid)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::AdjustPrivilege - failed to")
                           _T("LookupPrivilegeValue][0x%x]"), hr));
    return hr;
  }

  TOKEN_PRIVILEGES privs;
  privs.PrivilegeCount = 1;
  privs.Privileges[0].Luid = luid;
  privs.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

  if (!::AdjustTokenPrivileges(get(token), FALSE, &privs, 0, NULL, 0)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::AdjustPrivilege - failed to ")
                           _T("AdjustTokenPrivileges][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

DWORD System::WTSGetActiveConsoleSessionId()  {
  typedef DWORD (* Fun)();

  HINSTANCE hInst = ::GetModuleHandle(_T("kernel32.dll"));
  ASSERT1(hInst);
  Fun pfn = reinterpret_cast<Fun>(::GetProcAddress(
                                      hInst,
                                      "WTSGetActiveConsoleSessionId"));
  return !pfn ? kInvalidSessionId : (*pfn)();
}

// Get the session the current process is running under
DWORD System::GetCurrentSessionId() {
  DWORD session_id = kInvalidSessionId;
  DWORD* session_id_ptr = NULL;
  DWORD bytes_returned = 0;

  if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                   WTS_CURRENT_SESSION,
                                   WTSSessionId,
                                   reinterpret_cast<LPTSTR*>(&session_id_ptr),
                                   &bytes_returned)) {
    ASSERT1(bytes_returned == sizeof(*session_id_ptr));
    session_id = *session_id_ptr;
    ::WTSFreeMemory(session_id_ptr);
    UTIL_LOG(L6, (_T("[System::GetCurrentSessionId]")
                  _T("[session_id from ::WTSQuerySessionInformation][%d]"),
                  session_id));
    return session_id;
  }

  // ::WTSQuerySessionInformation can fail if we are not running
  // in a Terminal Services scenario, in which case, we use
  // ::ProcessIdToSessionId()
  if (::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id)) {
    UTIL_LOG(L6,  (_T("[System::GetCurrentSessionId]")
                   _T("[session_id from ::ProcessIdToSessionId][%d]"),
                   session_id));
    return session_id;
  }

  UTIL_LOG(LEVEL_ERROR,
           (_T("[System::GetCurrentSessionId - both")
            _T("::WTSQuerySessionInformation and ")
            _T("::ProcessIdToSessionId failed][0x%x]"),
            ::GetLastError()));

  return kInvalidSessionId;
}

// Get the best guess as to the currently active session, or kInvalidSessionId
// if there is no active session.
DWORD System::GetActiveSessionId() {
  // WTSGetActiveConsoleSessionId retrieves the Terminal Services session
  // currently attached to the physical console.
  DWORD active_session_id = WTSGetActiveConsoleSessionId();

  if (IsSessionActive(active_session_id)) {
    UTIL_LOG(L6, (_T("[System::GetActiveSessionId]")
                  _T("[Active session id from ::WTSGetActiveConsoleSessionId]")
                  _T("[%d]"), active_session_id));

    return active_session_id;
  }

  // WTSGetActiveConsoleSessionId works for FUS, but it does not work for TS
  // servers where the current active session is always the console. We then use
  // a different method as below. We get all the sessions that are present on
  // the system, to see if we can find an active session.
  active_session_id = kInvalidSessionId;
  WTS_SESSION_INFO* session_info = NULL;
  DWORD num_sessions = 0;
  if (::WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                              &session_info, &num_sessions)) {
    // Pick the first active session we can find
    for (DWORD i = 0 ; i < num_sessions; ++i) {
      if (session_info[i].State == WTSActive) {
        // There is a user logged on to the WinStation associated with the
        // session.
        active_session_id = session_info[i].SessionId;
        break;
      }
    }

    ::WTSFreeMemory(session_info);
    UTIL_LOG(L6, (_T("[System::GetActiveSessionId]")
                  _T("[Active session id from ::WTSEnumerateSessions][0x%x]"),
                  active_session_id));

    return active_session_id;
  }

  UTIL_LOG(LEVEL_ERROR,
           (_T("[System::GetActiveSessionId - ")
           _T("Both ::WTSGetActiveConsoleSessionId and ::WTSEnumerateSessions ")
           _T("failed][0x%x]"),
           ::GetLastError()));

  return kInvalidSessionId;
}

// Is there a user logged on and active in the specified session?
bool System::IsSessionActive(DWORD session_id) {
  if (kInvalidSessionId == session_id) {
    return false;
  }

  WTS_CONNECTSTATE_CLASS wts_connect_state = WTSDisconnected;
  WTS_CONNECTSTATE_CLASS* ptr_wts_connect_state = NULL;
  DWORD bytes_returned = 0;
  if (::WTSQuerySessionInformation(
          WTS_CURRENT_SERVER_HANDLE,
          session_id,
          WTSConnectState,
          reinterpret_cast<LPTSTR*>(&ptr_wts_connect_state),
          &bytes_returned)) {
    ASSERT1(bytes_returned == sizeof(*ptr_wts_connect_state));
    wts_connect_state = *ptr_wts_connect_state;
    ::WTSFreeMemory(ptr_wts_connect_state);

    UTIL_LOG(L6, (_T("[System::IsSessionActive]")
                  _T("[wts_connect_state %d]"), wts_connect_state));
    return WTSActive == wts_connect_state;
  }

  UTIL_LOG(LE, (_T("[WTSQuerySessionInformation failed][0x%x]"),
                ::GetLastError()));
  return false;
}

bool System::IsRunningOnBatteries() {
  SYSTEM_POWER_STATUS system_power_status = {0};
  if (::GetSystemPowerStatus(&system_power_status)) {
    bool has_battery = !(system_power_status.BatteryFlag & 128);
    bool ac_status_offline = system_power_status.ACLineStatus == 0;
    return ac_status_offline && has_battery;
  }
  return false;
}

HRESULT System::CreateChildOutputPipe(HANDLE* read, HANDLE* write) {
  scoped_handle pipe_read;
  scoped_handle pipe_write;

  CAccessToken effective_token;
  if (!effective_token.GetEffectiveToken(TOKEN_QUERY)) {
    UTIL_LOG(LW, (_T("[CAccessToken::GetEffectiveToken failed]")));
    return E_FAIL;
  }
  CSid user;
  if (!effective_token.GetUser(&user)) {
    UTIL_LOG(LW, (_T("[CSid::GetUser failed]")));
    return E_FAIL;
  }

  CSecurityDesc security_descriptor;
  CDacl dacl;
  ACCESS_MASK access_mask = GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE;
  dacl.AddAllowedAce(Sids::System(), access_mask);
  dacl.AddAllowedAce(Sids::Admins(), access_mask);
  dacl.AddAllowedAce(user, access_mask);
  security_descriptor.SetDacl(dacl);
  security_descriptor.MakeAbsolute();

  SECURITY_ATTRIBUTES pipe_security_attributes;
  pipe_security_attributes.nLength = sizeof(pipe_security_attributes);
  pipe_security_attributes.lpSecurityDescriptor =
      const_cast<SECURITY_DESCRIPTOR*>(
          security_descriptor.GetPSECURITY_DESCRIPTOR());

  pipe_security_attributes.bInheritHandle = TRUE;

  if (::CreatePipe(address(pipe_read), address(pipe_write),
                   &pipe_security_attributes, 0) == 0) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LW, (_T("[CreatePipe failed][0x%x]"), hr));
    return hr;
  }

  if (::SetHandleInformation(get(pipe_read), HANDLE_FLAG_INHERIT, 0) == 0) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LW, (_T("[SetHandleInformation failed][0x%x]"), hr));
    return hr;
  }

  *read = release(pipe_read);
  *write = release(pipe_write);

  return S_OK;
}

}  // namespace omaha

