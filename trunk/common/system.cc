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

#include "omaha/common/system.h"

#include <objidl.h>
#include <psapi.h>
#include <winioctl.h>
#include <wtsapi32.h>
#include "omaha/common/commands.h"
#include "omaha/common/const_config.h"
#include "omaha/common/debug.h"
#include "omaha/common/disk.h"
#include "omaha/common/dynamic_link_kernel32.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/path.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/system_info.h"
#include "omaha/common/utils.h"

namespace omaha {

// Constant
const TCHAR kNeedRebootHiddenFileSuffix[] = _T(".needreboot");

HRESULT System::WaitForDiskActivity(const uint32 max_delay_milliseconds,
                                    const uint32 sleep_time_ms,
                                    uint32 *time_waited) {
  ASSERT(time_waited, (L""));
  uint32 sleep_time = sleep_time_ms;
  if (sleep_time < 20) { sleep_time = 20; }
  else if (sleep_time > 1000) { sleep_time = 1000; }
  HRESULT r;
  *time_waited = 0;
  uint64 writes = 0;
  uint64 new_writes = 0;
  // get current counters
  if (FAILED(r=GetDiskActivityCounters(NULL, &writes, NULL, NULL))) {
    return r;
  }

  // wait until a write - reads may be cached
  while (1) {
    if (FAILED(r=GetDiskActivityCounters(NULL, &new_writes, NULL, NULL))) {
      return r;
    }
    if (new_writes > writes) { return S_OK; }
    if (*time_waited > max_delay_milliseconds) { return E_FAIL; }
    SleepEx(sleep_time, TRUE);
    *time_waited += sleep_time;
  }
}

HRESULT System::GetDiskActivityCounters(uint64* reads,
                                        uint64* writes,
                                        uint64* bytes_read,
                                        uint64* bytes_written) {
  if (reads) {
    *reads = 0;
  }

  if (writes) {
    *writes = 0;
  }

  if (bytes_read) {
    *bytes_read = 0;
  }

  if (bytes_written) {
    *bytes_written = 0;
  }

  // Don't want to risk displaying UI errors here
  DisableThreadErrorUI disable_error_dialog_box;

  // for all drives
  for (int drive = 0; ; drive++) {
    struct _DISK_PERFORMANCE perf_data;
    const int max_device_len = 50;

    // check whether we can access this device
    CString device_name;
    device_name.Format(_T("\\\\.\\PhysicalDrive%d"), drive);
    scoped_handle device(::CreateFile(device_name, 0,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL));

    if (get(device) == INVALID_HANDLE_VALUE) {
      if (!drive) {
        UTIL_LOG(LEVEL_ERROR, (_T("[Failed to access drive %i][0x%x]"),
                               drive,
                               HRESULTFromLastError()));
      }
      break;
    }

    // disk performance counters must be on (diskperf -y on older machines;
    // defaults to on on newer windows)
    DWORD size = 0;
    if (::DeviceIoControl(get(device),
                          IOCTL_DISK_PERFORMANCE,
                          NULL,
                          0,
                          &perf_data,
                          sizeof(_DISK_PERFORMANCE),
                          &size,
                          NULL)) {
      if (reads) {
        *reads += perf_data.ReadCount;
      }

      if (writes) {
        *writes += perf_data.WriteCount;
      }

      if (bytes_read) {
        *bytes_read += perf_data.BytesRead.QuadPart;
      }

      if (bytes_written) {
        *bytes_written += perf_data.BytesWritten.QuadPart;
      }
    } else {
      HRESULT hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR,
               (_T("[System::GetDiskActivityCounters - failed to ")
                _T("DeviceIoControl][0x%x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

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

  DWORD min_size(0), max_size(0);
  if (GetProcessWorkingSetSize(process_handle, &min_size, &max_size)) {
    UTIL_LOG(L2, (_T("working set %lu %lu"), min_size, max_size));
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
    if (current_working_set) {
      *current_working_set = counters.WorkingSetSize;
    }
    if (peak_working_set) {
      *peak_working_set = counters.PeakWorkingSetSize;
    }
    UTIL_LOG(L2, (_T("current/peak working set %s %s"),
                  String_Int64ToString(*current_working_set, 10),
                  String_Int64ToString(*peak_working_set, 10)));
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

HRESULT System::MaxPhysicalMemoryAvailable(uint64* max_bytes) {
  ASSERT1(max_bytes);

  *max_bytes = 0;

  uint32 memory_load_percentage = 0;
  uint64 free_physical_memory = 0;

  RET_IF_FAILED(System::GetGlobalMemoryStatistics(&memory_load_percentage,
    &free_physical_memory, NULL, NULL, NULL, NULL, NULL));

  UTIL_LOG(L4, (_T("mem load %u max physical memory available %s"),
                memory_load_percentage,
                String_Int64ToString(free_physical_memory, 10)));

  *max_bytes = free_physical_memory;

  return S_OK;
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

void System::FreeProcessWorkingSet() {
  // -1,-1 is a special signal to the OS to temporarily trim the working set
  // size to 0.  See MSDN for further information.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

HRESULT System::SetThreadPriority(enum Priority priority) {
  int pri;

  switch (priority) {
    case LOW: pri = THREAD_PRIORITY_BELOW_NORMAL; break;
    case HIGH: pri = THREAD_PRIORITY_HIGHEST; break;
    case NORMAL: pri = THREAD_PRIORITY_NORMAL; break;
    case IDLE: pri = THREAD_PRIORITY_IDLE; break;
    default: return E_FAIL;
  }

  if (::SetThreadPriority(GetCurrentThread(), pri)) {
    return S_OK;
  } else {
    return E_FAIL;
  }
}

HRESULT System::SetProcessPriority(enum Priority priority) {
  DWORD pri = 0;
  switch (priority) {
    case LOW: pri = BELOW_NORMAL_PRIORITY_CLASS; break;
    case HIGH: pri = ABOVE_NORMAL_PRIORITY_CLASS; break;
    case NORMAL: pri = NORMAL_PRIORITY_CLASS; break;
    case IDLE: return E_INVALIDARG;
    default: return E_INVALIDARG;
  }

  DWORD pid = ::GetCurrentProcessId();

  scoped_handle handle(::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
  if (!valid(handle)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[::OpenProcess failed][%u][0x%x]"), pid, hr));
    return hr;
  }

  if (!::SetPriorityClass(get(handle), pri)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[::SetPriorityClass failed][%u][0x%x]"), pid, hr));
    return hr;
  }

  return S_OK;
}

// start another process painlessly via ::CreateProcess. Use the
// ShellExecuteProcessXXX variants instead of these methods where possible,
// since ::ShellExecuteEx has better behavior on Windows Vista.
// When using this method, avoid using process_name - see
// http://blogs.msdn.com/oldnewthing/archive/2006/05/15/597984.aspx.
HRESULT System::StartProcess(const TCHAR* process_name,
                             TCHAR* command_line,
                             PROCESS_INFORMATION* pi) {
  ASSERT1(pi);
  ASSERT1(command_line || process_name);
  ASSERT(!process_name, (_T("Avoid using process_name. See method comment.")));

  STARTUPINFO si = {sizeof(si), 0};

  // Feedback cursor is off while the process is starting.
  si.dwFlags = STARTF_FORCEOFFFEEDBACK;

  UTIL_LOG(L3, (_T("[System::StartProcess][process %s][cmd %s]"),
                process_name, command_line));

  BOOL success = ::CreateProcess(
      process_name,     // Module name
      command_line,     // Command line
      NULL,             // Process handle not inheritable
      NULL,             // Thread handle not inheritable
      FALSE,            // Set handle inheritance to FALSE
      0,                // No creation flags
      NULL,             // Use parent's environment block
      NULL,             // Use parent's starting directory
      &si,              // Pointer to STARTUPINFO structure
      pi);              // Pointer to PROCESS_INFORMATION structure

  if (!success) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[System::StartProcess][::CreateProcess failed][0x%x]"), hr));
    return hr;
  }

  OPT_LOG(L1, (_T("[Started process][%u]"), pi->dwProcessId));

  return S_OK;
}

// start another process painlessly via ::CreateProcess. Use the
// ShellExecuteProcessXXX variants instead of these methods where possible,
// since ::ShellExecuteEx has better behavior on Windows Vista.
HRESULT System::StartProcessWithArgsAndInfo(const TCHAR *process_name,
                                            const TCHAR *cmd_line_arguments,
                                            PROCESS_INFORMATION *pi) {
  ASSERT1(process_name && cmd_line_arguments && pi);

  CString command_line(process_name);
  EnclosePath(&command_line);
  command_line.AppendChar(_T(' '));
  command_line.Append(cmd_line_arguments);
  return System::StartProcess(NULL, command_line.GetBuffer(), pi);
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

// start another process painlessly via ::ShellExecuteEx. Use this method
// instead of the StartProcessXXX methods that use ::CreateProcess where
// possible, since ::ShellExecuteEx has better behavior on Windows Vista.
HRESULT System::ShellExecuteCommandLine(const TCHAR* command_line_to_execute,
                                        HWND hwnd,
                                        HANDLE* process_handle) {
  ASSERT1(command_line_to_execute);

  CString exe;
  CString args;

  HRESULT hr = CommandParsingSimple::SplitExeAndArgs(command_line_to_execute,
                                                     &exe,
                                                     &args);

  if (SUCCEEDED(hr)) {
    hr = System::ShellExecuteProcess(exe, args, hwnd, process_handle);
    if (FAILED(hr)) {
      UTIL_LOG(LEVEL_ERROR, (_T("[System::ShellExecuteProcess failed]")
                             _T("[%s][%s][0x%08x]"), exe, args, hr));
    }
  }

  return hr;
}

// returns the number of ms the system has had no user input
int System::GetUserIdleTime() {
  LASTINPUTINFO last_input_info;
  last_input_info.cbSize = sizeof(LASTINPUTINFO);
  // get time in windows ticks since system start of last activity
  BOOL b = GetLastInputInfo(&last_input_info);
  if (b == TRUE) {
    return (GetTickCount()-last_input_info.dwTime);  // compute idle time
  }
  return 0;
}

bool System::IsUserIdle() {
  // Only notify when the user has been idle less than this time
  static int user_idle_threshold_ms = kUserIdleThresholdMs;

  bool is_user_idle = (GetUserIdleTime() > user_idle_threshold_ms);
  UTIL_LOG(L2, (_T("System::IsUserIdle() %s; user_idle_threshold_ms = %d"),
                is_user_idle ? _T("TRUE") : _T("FALSE"),
                user_idle_threshold_ms));
  return is_user_idle;
}

bool System::IsUserBusy() {
  // The user is busy typing or interacting with another application
  // if the user is below the minimum threshold:
  static int user_idle_min_threshold_ms = kUserIdleMinThresholdMs;
  // The user is probably not paying attention
  // if the user is above the maximum threshold:
  static int user_idle_max_threshold_ms = kUserIdleMaxThresholdMs;

  int user_idle_time = GetUserIdleTime();
  bool is_user_busy = user_idle_time < user_idle_min_threshold_ms ||
    user_idle_time > user_idle_max_threshold_ms;
  UTIL_LOG(L2, (_T("[System::IsUserBusy() %s][user_idle_time = %d]")
                _T("[user_idle_min_threshold_ms = %d]")
                _T("[user_idle_max_threshold_ms = %d]"),
                is_user_busy? _T("TRUE") : _T("FALSE"),
                user_idle_time,
                user_idle_min_threshold_ms,
                user_idle_max_threshold_ms));
  return is_user_busy;
}

bool System::IsScreensaverRunning() {
  // NT 4.0 and below require testing OpenDesktop("screen-saver")
  // We require W2K or better so we have an easier way
  DWORD result = 0;
  ::SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &result, 0);
  bool is_screensaver_running = (result != FALSE);
  UTIL_LOG(L2, (_T("System::IsScreensaverRunning() %s"),
                is_screensaver_running? _T("TRUE") : _T("FALSE")));
  return is_screensaver_running;
}

bool System::IsWorkstationLocked() {
  bool is_workstation_locked = true;
  HDESK inputdesk = ::OpenInputDesktop(0, 0, GENERIC_READ);
  if (NULL != inputdesk)  {
    TCHAR name[256];
    DWORD needed = arraysize(name);
    BOOL ok = ::GetUserObjectInformation(inputdesk,
                                         UOI_NAME,
                                         name,
                                         sizeof(name),
                                         &needed);
    ::CloseDesktop(inputdesk);
    if (ok) {
      is_workstation_locked = (0 != lstrcmpi(name, NOTRANSL(_T("default"))));
    }
  }

  UTIL_LOG(L2, (_T("System::IsWorkstationLocked() %s"),
                is_workstation_locked? _T("TRUE") : _T("FALSE")));
  return is_workstation_locked;
}

bool System::IsUserAway() {
  return IsScreensaverRunning() || IsWorkstationLocked();
}

uint32 System::GetProcessHandleCount() {
  typedef LONG (CALLBACK *Fun)(HANDLE, int32, PVOID, ULONG, PULONG);

  // This new version of getting the number of open handles works on win2k.
  HMODULE h = GetModuleHandle(_T("ntdll.dll"));
  Fun NtQueryInformationProcess =
      reinterpret_cast<Fun>(::GetProcAddress(h, "NtQueryInformationProcess"));

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

uint32 System::GetProcessHandleCountOld() {
  typedef BOOL (CALLBACK * Fun)(HANDLE, PDWORD);

  // GetProcessHandleCount not available on win2k
  HMODULE handle = GetModuleHandle(_T("kernel32"));
  Fun f = reinterpret_cast<Fun>(GetProcAddress(handle,
                                               "GetProcessHandleCount"));

  if (!f) return 0;

  DWORD count = 0;
  VERIFY((*f)(GetCurrentProcess(), &count), (L""));
  return count;

  //  DWORD GetGuiResources (HANDLE hProcess, DWORD uiFlags);
  //  Parameters, hProcess
  //  [in] Handle to the process. The handle must have the
  //  PROCESS_QUERY_INFORMATION access right. For more information, see Process
  //  Security and Access Rights.
  //  uiFlags
  //  [in] GUI object type. This parameter can be one of the following values.
  //  Value          Meaning
  //  GR_GDIOBJECTS  Return the count of GDI objects.
  //  GR_USEROBJECTS Return the count of USER objects.
}

void System::GetGuiObjectCount(uint32 *gdi, uint32 *user) {
  if (gdi) {
    *gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  }
  if (user) {
    *user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
  }
}

HRESULT System::GetRebootCheckDummyFileName(const TCHAR* base_file,
                                            CString* dummy_file) {
  ASSERT1(dummy_file);

  if (base_file && *base_file) {
    ASSERT1(File::Exists(base_file));
    dummy_file->SetString(base_file);
  } else {
    RET_IF_FAILED(GetModuleFileName(NULL, dummy_file));
  }
  dummy_file->Append(_T(".needreboot"));
  return S_OK;
}

// Is the system being rebooted?
bool System::IsRebooted(const TCHAR* base_file) {
  CString dummy_file;
  if (SUCCEEDED(GetRebootCheckDummyFileName(base_file, &dummy_file))) {
    if (File::Exists(dummy_file)) {
      // If the file exists but it is not found in the
      // PendingFileRenameOperations, (probably becaused that this key is messed
      // up and thus the system restart fails to delete the file), re-add it
      if (!File::AreMovesPendingReboot(dummy_file, true)) {
        File::MoveAfterReboot(dummy_file, NULL);
      }
      return false;
    } else {
      return true;
    }
  }
  return false;
}

// Mark the system as reboot required
HRESULT System::MarkAsRebootRequired(const TCHAR* base_file) {
  // Create a dummy file if needed
  CString dummy_file;
  RET_IF_FAILED(GetRebootCheckDummyFileName(base_file, &dummy_file));
  if (File::Exists(dummy_file)) {
    return S_OK;
  }

  File file;
  RET_IF_FAILED(file.Open(dummy_file, true, false));
  RET_IF_FAILED(file.Close());

  // Hide it
  DWORD file_attr = ::GetFileAttributes(dummy_file);
  if (file_attr == INVALID_FILE_ATTRIBUTES ||
      !::SetFileAttributes(dummy_file, file_attr | FILE_ATTRIBUTE_HIDDEN)) {
    return HRESULTFromLastError();
  }

  // Mark it as being deleted after reboot
  return File::MoveAfterReboot(dummy_file, NULL);
}

// Unmark the system as reboot required
HRESULT System::UnmarkAsRebootRequired(const TCHAR* base_file) {
  CString dummy_file;
  RET_IF_FAILED(GetRebootCheckDummyFileName(base_file, &dummy_file));

  return File::RemoveFromMovesPendingReboot(dummy_file, false);
}

// Restart the computer
HRESULT System::RestartComputer() {
  RET_IF_FAILED(AdjustPrivilege(SE_SHUTDOWN_NAME, true));

  if (!::ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_APPLICATION |
                       SHTDN_REASON_MINOR_INSTALLATION |
                       SHTDN_REASON_FLAG_PLANNED)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::RestartComputer - failed to")
                           _T(" ExitWindowsEx][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

// The implementation works on all Windows versions. On NT and XP the screen
// saver is actually stored in registry at
// HKEY_CURRENT_USER\Control Panel\Desktop\SCRNSAVE.EXE but the
// GetPrivateProfileString call is automatically mapped to the registry
HRESULT System::GetCurrentScreenSaver(CString* fileName) {
  if (!fileName) return E_POINTER;

  DWORD nChars = ::GetPrivateProfileString(_T("boot"),
                                           _T("SCRNSAVE.EXE"),
                                           _T(""),
                                           fileName->GetBuffer(MAX_PATH),
                                           MAX_PATH,
                                           _T("system.ini"));
  fileName->ReleaseBufferSetLength(nChars);

  return S_OK;
}

// Create an instance of a COM Local Server class using either plain vanilla
// CoCreateInstance, or using the Elevation moniker depending on the operating
// system
HRESULT System::CoCreateInstanceAsAdmin(HWND hwnd,
                                        REFCLSID rclsid,
                                        REFIID riid,
                                        void** ppv) {
  if (SystemInfo::IsRunningOnVistaOrLater()) {
    // Use the Elevation Moniker to create the Install Manager in Windows Vista.
    // If the UI is running in medium integrity, this will result in a
    // elevation prompt

    scoped_window hwnd_parent;

    if (!hwnd) {
      reset(hwnd_parent, CreateForegroundParentWindowForUAC());

      if (!hwnd_parent) {
        return HRESULTFromLastError();
      }
      // Use the newly created dummy window as the hwnd
      hwnd = get(hwnd_parent);
    }

    CString moniker_name(_T("Elevation:Administrator!new:"));
    moniker_name += GuidToString(rclsid);
    BIND_OPTS3 bo;
    SetZero(bo);
    bo.cbStruct = sizeof(bo);
    bo.hwnd = hwnd;
    bo.dwClassContext = CLSCTX_LOCAL_SERVER;

    return ::CoGetObject(moniker_name, &bo, riid, ppv);
  } else {
    // Use plain-vanilla ::CoCreateInstance()
    return ::CoCreateInstance(rclsid, NULL, CLSCTX_LOCAL_SERVER, riid, ppv);
  }
}

HRESULT System::IsPrivilegeEnabled(const TCHAR* privilege, bool* present) {
  ASSERT1(privilege);
  ASSERT1(present);

  *present = false;

  scoped_handle token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          address(token))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::IsPrivilegeEnabled - failed to ")
                           _T("OpenProcessToken][0x%x]"), hr));
    return hr;
  }

  LUID luid = {0};
  if (!::LookupPrivilegeValue(NULL, privilege, &luid)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::IsPrivilegeEnabled - failed to")
                           _T("LookupPrivilegeValue][0x%x]"), hr));
    return hr;
  }

  PRIVILEGE_SET required_privilege = {0};
  required_privilege.PrivilegeCount = 1;
  required_privilege.Control = PRIVILEGE_SET_ALL_NECESSARY;
  required_privilege.Privilege[0].Luid = luid;

  BOOL result = FALSE;
  if (!::PrivilegeCheck(get(token), &required_privilege, &result)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[System::IsPrivilegeEnabled - failed to")
                           _T("PrivilegeCheck][0x%x]"), hr));
    return hr;
  }

  if (required_privilege.Privilege[0].Attributes &
      SE_PRIVILEGE_USED_FOR_ACCESS) {
    *present = true;
  }

  return S_OK;
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

// Is the current process running under WinSta0
bool System::IsCurrentProcessInteractive() {
  // Use a non-scoped handle, since a handle retrieved via
  // ::GetProcessWindowStation() should not be closed.
  HWINSTA handle_window_station(::GetProcessWindowStation());
  DWORD len = 0;
  CString str_window_station;

  if (!handle_window_station || handle_window_station == INVALID_HANDLE_VALUE) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[System::IsCurrentProcessInteractive - ")
              _T("::GetProcessWindowStation() failed (%d)]"),
              ::GetLastError()));
    return false;
  }

  if (!::GetUserObjectInformation(handle_window_station,
                                  UOI_NAME,
                                  CStrBuf(str_window_station, MAX_PATH),
                                  MAX_PATH,
                                  &len)) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[System::IsCurrentProcessInteractive - ")
              _T("::GetUserObjectInfoformation(hWinSta) failed (%d)]"),
              ::GetLastError()));
    return false;
  }

  UTIL_LOG(L6, (_T("[System::IsCurrentProcessInteractive]")
                _T("[WindowStation name][%s]"),
                str_window_station));
  return (str_window_station == _T("WinSta0"));
}

// is the current process running under WinSta0 for the currently active session
bool System::IsCurrentProcessActiveAndInteractive() {
  return IsSessionActive(GetCurrentSessionId()) &&
         IsCurrentProcessInteractive();
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

}  // namespace omaha

