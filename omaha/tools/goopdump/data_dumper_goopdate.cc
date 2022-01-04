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


#include "omaha/tools/goopdump/data_dumper_goopdate.h"

#include <atltime.h>
#include <mstask.h>
#include <psapi.h>
#include <regstr.h>
#include <tlhelp32.h>

#include <list>
#include <memory>

#include "omaha/common/constants.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/file_reader.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_ptr_cotask.h"
#include "omaha/common/service_utils.h"
#include "omaha/common/time.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/event_logger.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/tools/goopdump/dump_log.h"
#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"
#include "omaha/tools/goopdump/process_commandline.h"

namespace {

CString FormatRunTimeString(SYSTEMTIME* system_time) {
  SYSTEMTIME local_time = {0};
  ::SystemTimeToTzSpecificLocalTime(NULL, system_time, &local_time);
  CString str;
  str.Format(_T("%02d/%02d/%04d %02d:%02d:%02d"),
             local_time.wMonth,
             local_time.wDay,
             local_time.wYear,
             local_time.wHour,
             local_time.wMinute,
             local_time.wSecond);
  return str;
}

}  // namespace

namespace omaha {

DataDumperGoopdate::DataDumperGoopdate() {
}

DataDumperGoopdate::~DataDumperGoopdate() {
}

HRESULT DataDumperGoopdate::Process(const DumpLog& dump_log,
                                    const GoopdumpCmdLineArgs& args) {
  UNREFERENCED_PARAMETER(args);

  DumpHeader header(dump_log, _T("Goopdate Data"));

  dump_log.WriteLine(_T(""));
  dump_log.WriteLine(_T("-- GENERAL / GLOBAL DATA --"));
  DumpHostsFile(dump_log);
  DumpGoogleUpdateIniFile(dump_log);
  DumpUpdateDevKeys(dump_log);
  DumpLogFile(dump_log);
  DumpEventLog(dump_log);
  DumpGoogleUpdateProcessInfo(dump_log);

  if (args.is_machine) {
    dump_log.WriteLine(_T(""));
    dump_log.WriteLine(_T("-- PER-MACHINE DATA --"));
    DumpDirContents(dump_log, true);
    DumpServiceInfo(dump_log);
  }

  if (args.is_user) {
    dump_log.WriteLine(_T(""));
    dump_log.WriteLine(_T("-- PER-USER DATA --"));
    DumpDirContents(dump_log, false);
    DumpRunKeys(dump_log);
  }

  DumpScheduledTaskInfo(dump_log, args.is_machine);

  return S_OK;
}

void DataDumperGoopdate::DumpDirContents(const DumpLog& dump_log,
                                         bool is_machine) {
  DumpHeader header(dump_log, _T("Directory Contents"));

  CString registered_version;
  if (FAILED(GetRegisteredVersion(is_machine, &registered_version))) {
    dump_log.WriteLine(_T("Failed to get registered version."));
    return;
  }

  CString dll_dir;
  if (FAILED(GetDllDir(is_machine, &dll_dir))) {
    dump_log.WriteLine(_T("Failed to get dlldir."));
    return;
  }

  dump_log.WriteLine(_T("Version:\t%s"), registered_version);
  dump_log.WriteLine(_T("Dll Dir:\t%s"), dll_dir);

  // Enumerate all files in the DllPath and log them.
  std::vector<CString> matching_paths;
  HRESULT hr = File::GetWildcards(dll_dir, _T("*.*"), &matching_paths);
  if (SUCCEEDED(hr)) {
    dump_log.WriteLine(_T(""));
    dump_log.WriteLine(_T("Files in DllDir:"));
    for (size_t i = 0; i < matching_paths.size(); ++i) {
      dump_log.WriteLine(matching_paths[i]);
    }
    dump_log.WriteLine(_T(""));
  } else {
    dump_log.WriteLine(_T("Failure getting files in DllDir (0x%x)."), hr);
  }
}

HRESULT DataDumperGoopdate::GetRegisteredVersion(bool is_machine,
                                                 CString* version) {
  HKEY key = NULL;
  LONG res = ::RegOpenKeyEx(is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                            GOOPDATE_REG_RELATIVE_CLIENTS GOOPDATE_APP_ID,
                            0,
                            KEY_READ,
                            &key);
  if (ERROR_SUCCESS != res) {
    return HRESULT_FROM_WIN32(res);
  }

  DWORD type = 0;
  DWORD version_length = 50;
  res = ::SHQueryValueEx(key,
                         omaha::kRegValueProductVersion,
                         NULL,
                         &type,
                         CStrBuf(*version, version_length),
                         &version_length);
  if (ERROR_SUCCESS != res) {
    return HRESULT_FROM_WIN32(res);
  }

  if (REG_SZ != type) {
    return E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT DataDumperGoopdate::GetDllDir(bool is_machine, CString* dll_path) {
  TCHAR path[MAX_PATH] = {0};

  CString base_path = goopdate_utils::BuildGoogleUpdateExeDir(is_machine);

  // Try the side-by-side DLL first.
  _tcscpy_s(path, arraysize(path), base_path);
  if (!::PathAppend(path, omaha::kGoopdateDllName)) {
    return HRESULTFromLastError();
  }
  if (File::Exists(path)) {
    *dll_path = base_path;
    return S_OK;
  }

  // Try the version subdirectory.
  _tcscpy_s(path, arraysize(path), base_path);
  CString version;
  HRESULT hr = GetRegisteredVersion(is_machine, &version);
  if (FAILED(hr)) {
    return hr;
  }
  if (!::PathAppend(path, version)) {
    return HRESULTFromLastError();
  }
  base_path = path;
  if (!::PathAppend(path, omaha::kGoopdateDllName)) {
    return HRESULTFromLastError();
  }
  if (!File::Exists(path)) {
    return GOOGLEUPDATE_E_DLL_NOT_FOUND;
  }

  *dll_path = base_path;
  return S_OK;
}

void DataDumperGoopdate::DumpGoogleUpdateIniFile(const DumpLog& dump_log) {
  DumpHeader header(dump_log, MAIN_EXE_BASE_NAME _T(".ini File Contents"));
  DumpFileContents(dump_log, _T("c:\\googleupdate.ini"), 0);
}

void DataDumperGoopdate::DumpHostsFile(const DumpLog& dump_log) {
  DumpHeader header(dump_log, _T("Hosts File Contents"));
  TCHAR system_path[MAX_PATH] = {0};
  HRESULT hr = ::SHGetFolderPath(NULL,
                                 CSIDL_SYSTEM,
                                 NULL,
                                 SHGFP_TYPE_CURRENT,
                                 system_path);
  if (FAILED(hr)) {
    dump_log.WriteLine(_T("Can't get System folder: 0x%x"), hr);
    return;
  }

  CPath full_path = system_path;
  full_path.Append(_T("drivers\\etc\\hosts"));
  DumpFileContents(dump_log, full_path, 0);
}

void DataDumperGoopdate::DumpUpdateDevKeys(const DumpLog& dump_log) {
  DumpHeader header(dump_log, _T("UpdateDev Keys"));

  DumpRegistryKeyData(dump_log, _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\UpdateDev"));
}

void DataDumperGoopdate::DumpLogFile(const DumpLog& dump_log) {
  DumpHeader header(dump_log, _T("Debug Log File Contents"));

  Logging logger;
  CString log_file_path(logger.GetLogFilePath());
  DumpFileContents(dump_log, log_file_path, 500);
}

CString EventLogTypeToString(WORD event_log_type) {
  CString str = _T("Unknown");
  switch (event_log_type) {
    case EVENTLOG_ERROR_TYPE:
      str = _T("ERROR");
      break;
    case EVENTLOG_WARNING_TYPE:
      str = _T("WARNING");
      break;
    case EVENTLOG_INFORMATION_TYPE:
      str = _T("INFORMATION");
      break;
    case EVENTLOG_AUDIT_SUCCESS:
      str = _T("AUDIT_SUCCESS");
      break;
    case EVENTLOG_AUDIT_FAILURE:
      str = _T("AUDIT_FAILURE");
      break;
    default:
      str = _T("Unknown");
      break;
  }

  return str;
}

void DataDumperGoopdate::DumpEventLog(const DumpLog& dump_log) {
  DumpHeader header(dump_log, _T("Google Update Event Log Entries"));

  HANDLE handle_event_log = ::OpenEventLog(NULL, _T("Application"));
  if (handle_event_log == NULL) {
    return;
  }

  const int kInitialBufferSize = 8192;
  int buffer_size = kInitialBufferSize;
  std::unique_ptr<TCHAR[]> buffer(new TCHAR[buffer_size]);

  while (1) {
    EVENTLOGRECORD* record = reinterpret_cast<EVENTLOGRECORD*>(buffer.get());
    DWORD num_bytes_read = 0;
    DWORD bytes_needed = 0;
    if (!::ReadEventLog(handle_event_log,
                        EVENTLOG_FORWARDS_READ | EVENTLOG_SEQUENTIAL_READ,
                        0,
                        record,
                        buffer_size,
                        &num_bytes_read,
                        &bytes_needed)) {
      const int err = ::GetLastError();
      if (ERROR_INSUFFICIENT_BUFFER == err) {
        buffer_size = bytes_needed;
        buffer.reset(new TCHAR[buffer_size]);
        continue;
      } else {
        if (ERROR_HANDLE_EOF != err) {
          dump_log.WriteLine(_T("ReadEventLog failed: %d"), err);
        }
        break;
      }
    }

    while (num_bytes_read > 0) {
      const TCHAR* source_name = reinterpret_cast<const TCHAR*>(
          reinterpret_cast<BYTE*>(record) + sizeof(*record));

      if (_tcscmp(source_name, EventLogger::kSourceName) == 0) {
        CString event_log_type = EventLogTypeToString(record->EventType);

        const TCHAR* message_data_buffer = reinterpret_cast<const TCHAR*>(
            reinterpret_cast<BYTE*>(record) + record->StringOffset);

        CString message_data(message_data_buffer);

        FILETIME filetime = {0};
        TimeTToFileTime(record->TimeWritten, &filetime);
        SYSTEMTIME systemtime = {0};
        ::FileTimeToSystemTime(&filetime, &systemtime);

        CString message_line;
        message_line.Format(_T("[%s] (%d)|(%s) %s"),
                            FormatRunTimeString(&systemtime),
                            record->EventID,
                            event_log_type,
                            message_data);
        dump_log.WriteLine(message_line);
      }

      num_bytes_read -= record->Length;
      record = reinterpret_cast<EVENTLOGRECORD*>(
          reinterpret_cast<BYTE*>(record) + record->Length);
    }

    record = reinterpret_cast<EVENTLOGRECORD*>(&buffer);
  }

  ::CloseEventLog(handle_event_log);
}

void DataDumperGoopdate::DumpGoogleUpdateProcessInfo(const DumpLog& dump_log) {
  DumpHeader header(dump_log, MAIN_EXE_BASE_NAME _T(".exe Process Info"));

  EnableDebugPrivilege();

  scoped_handle handle_snap;
  reset(handle_snap,
        ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0));
  if (!valid(handle_snap)) {
    return;
  }

  PROCESSENTRY32 process_entry32 = {0};
  process_entry32.dwSize = sizeof(PROCESSENTRY32);

  if (!::Process32First(get(handle_snap), &process_entry32)) {
    return;
  }

  bool first = true;

  do {
    CString exe_file_name = process_entry32.szExeFile;
    exe_file_name.MakeLower();

    CString main_exe_file_name(MAIN_EXE_BASE_NAME _T(".exe"));
    main_exe_file_name.MakeLower();
    if (exe_file_name.Find(main_exe_file_name) >= 0) {
      if (first) {
        first = false;
      } else {
        dump_log.WriteLine(_T("-------------------"));
      }
      dump_log.WriteLine(_T("Process ID: %d"), process_entry32.th32ProcessID);
      scoped_handle process_handle;
      reset(process_handle, ::OpenProcess(PROCESS_ALL_ACCESS,
                                          FALSE,
                                          process_entry32.th32ProcessID));
      if (get(process_handle)) {
        CString command_line;
        if (SUCCEEDED(GetProcessCommandLine(process_entry32.th32ProcessID,
                                            &command_line))) {
          dump_log.WriteLine(_T("Command Line: %s"), command_line);
        } else {
          dump_log.WriteLine(_T("Command Line: Failed to retrieve"));
        }

        PROCESS_MEMORY_COUNTERS memory_counters = {0};
        memory_counters.cb = sizeof(memory_counters);
        if (GetProcessMemoryInfo(get(process_handle),
                                 &memory_counters,
                                 sizeof(memory_counters))) {
          dump_log.WriteLine(_T("Page Fault Count:      %d"),
                             memory_counters.PageFaultCount);
          dump_log.WriteLine(_T("Peak Working Set Size: %d"),
                             memory_counters.PeakWorkingSetSize);
          dump_log.WriteLine(_T("Working Set Size:      %d"),
                             memory_counters.WorkingSetSize);
          dump_log.WriteLine(_T("Page File Usage:       %d"),
                             memory_counters.PagefileUsage);
          dump_log.WriteLine(_T("Peak Page File Usage:  %d"),
                             memory_counters.PeakPagefileUsage);
        } else {
          dump_log.WriteLine(_T("Unable to get process memory info"));
        }

        THREADENTRY32 thread_entry = {0};
        thread_entry.dwSize = sizeof(thread_entry);
        int thread_count = 0;
        if (Thread32First(get(handle_snap), &thread_entry)) {
          do {
            if (thread_entry.th32OwnerProcessID ==
                process_entry32.th32ProcessID) {
              ++thread_count;
            }
          } while (::Thread32Next(get(handle_snap), &thread_entry));
        }

        dump_log.WriteLine(_T("Thread Count:          %d"), thread_count);

        FILETIME creation_time = {0};
        FILETIME exit_time = {0};
        FILETIME kernel_time = {0};
        FILETIME user_time = {0};
        if (::GetProcessTimes(get(process_handle),
                              &creation_time,
                              &exit_time,
                              &kernel_time,
                              &user_time)) {
          SYSTEMTIME creation_system_time = {0};
          FileTimeToSystemTime(&creation_time, &creation_system_time);
          CString creation_str = FormatRunTimeString(&creation_system_time);
          dump_log.WriteLine(_T("Process Start Time:    %s"), creation_str);

          CTime time_creation(creation_time);
          CTime time_now = CTime::GetCurrentTime();
          CTimeSpan time_span = time_now - time_creation;
          CString time_span_format =
              time_span.Format(_T("%D days, %H hours, %M minutes, %S seconds"));
          dump_log.WriteLine(_T("Process Uptime:        %s"), time_span_format);
        } else {
          dump_log.WriteLine(_T("Unable to get Process Times"));
        }
      }
    }
  } while (::Process32Next(get(handle_snap), &process_entry32));
}

void DataDumperGoopdate::DumpServiceInfo(const DumpLog& dump_log) {
  DumpHeader header(dump_log, _T("Google Update Service Info"));

  CString current_service_name = ConfigManager::GetCurrentServiceName();
  bool is_service_installed =
      ServiceInstall::IsServiceInstalled(current_service_name);

  dump_log.WriteLine(_T("Service Name: %s"), current_service_name);
  dump_log.WriteLine(_T("Is Installed: %s"),
                     is_service_installed ? _T("YES") : _T("NO"));
}

void DataDumperGoopdate::DumpScheduledTaskInfo(const DumpLog& dump_log,
                                               bool is_machine) {
  DumpHeader header(dump_log, _T("Scheduled Task Info"));

  CComPtr<ITaskScheduler> scheduler;
  HRESULT hr = scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                          NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    dump_log.WriteLine(_T("ITaskScheduler.CoCreateInstance failed: 0x%x"),
                       hr);
    return;
  }

  CComPtr<ITask> task;
  hr = scheduler->Activate(ConfigManager::GetCurrentTaskNameCore(is_machine),
                           __uuidof(ITask),
                           reinterpret_cast<IUnknown**>(&task));

  if (FAILED(hr)) {
    dump_log.WriteLine(_T("ITaskScheduler.Activate failed: 0x%x"), hr);
    return;
  }

  scoped_ptr_cotask<TCHAR> app_name;
  hr = task->GetApplicationName(address(app_name));
  dump_log.WriteLine(_T("ApplicationName: %s"),
                     SUCCEEDED(hr) ? app_name.get() : _T("Not Found"));

  scoped_ptr_cotask<TCHAR> creator;
  hr = task->GetCreator(address(creator));
  dump_log.WriteLine(_T("Creator: %s"),
                     SUCCEEDED(hr) ? creator.get() : _T("Not Found"));

  scoped_ptr_cotask<TCHAR> parameters;
  hr = task->GetParameters(address(parameters));
  dump_log.WriteLine(_T("Parameters: %s"),
                     SUCCEEDED(hr) ? parameters.get() : _T("Not Found"));

  scoped_ptr_cotask<TCHAR> comment;
  hr = task->GetComment(address(comment));
  dump_log.WriteLine(_T("Comment: %s"),
                     SUCCEEDED(hr) ? comment.get() : _T("Not Found"));

  scoped_ptr_cotask<TCHAR> working_dir;
  hr = task->GetWorkingDirectory(address(working_dir));
  dump_log.WriteLine(_T("Working Directory: %s"),
                     SUCCEEDED(hr) ? working_dir.get() : _T("Not Found"));

  scoped_ptr_cotask<TCHAR> account_info;
  hr = task->GetAccountInformation(address(account_info));
  dump_log.WriteLine(_T("Account Info: %s"),
                     SUCCEEDED(hr) ? account_info.get() : _T("Not Found"));

  dump_log.WriteLine(_T("Triggers:"));
  WORD trigger_count = 0;
  hr = task->GetTriggerCount(&trigger_count);
  if (SUCCEEDED(hr)) {
    for (WORD i = 0; i < trigger_count; ++i) {
      CComPtr<ITaskTrigger> trigger;
      if (SUCCEEDED(task->GetTrigger(i, &trigger))) {
        scoped_ptr_cotask<TCHAR> trigger_string;
        if (SUCCEEDED(trigger->GetTriggerString(address(trigger_string)))) {
          dump_log.WriteLine(_T("   %s"), trigger_string.get());
        }
      }
    }
  }

  SYSTEMTIME next_run_time = {0};
  hr = task->GetNextRunTime(&next_run_time);
  if (SUCCEEDED(hr)) {
    dump_log.WriteLine(_T("Next Run Time: %s"),
                       FormatRunTimeString(&next_run_time));
  } else {
    dump_log.WriteLine(_T("Next Run Time: Not Found"));
  }

  SYSTEMTIME recent_run_time = {0};
  hr = task->GetMostRecentRunTime(&recent_run_time);
  if (SUCCEEDED(hr)) {
    dump_log.WriteLine(_T("Most Recent Run Time: %s"),
                       FormatRunTimeString(&recent_run_time));
  } else {
    dump_log.WriteLine(_T("Most Recent Run Time: Not Found"));
  }

  DWORD max_run_time = 0;
  hr = task->GetMaxRunTime(&max_run_time);
  if (SUCCEEDED(hr)) {
    dump_log.WriteLine(_T("Max Run Time: %d ms"), max_run_time);
  } else {
    dump_log.WriteLine(_T("Max Run Time: [Not Available]"));
  }
}

void DataDumperGoopdate::DumpRunKeys(const DumpLog& dump_log) {
  DumpHeader header(dump_log, _T("Google Update Run Keys"));

  CString key_path = AppendRegKeyPath(USER_KEY_NAME, REGSTR_PATH_RUN);
  DumpRegValueStr(dump_log, key_path, kRunValueName);
}

void DataDumperGoopdate::DumpFileContents(const DumpLog& dump_log,
                                          const CString& file_path,
                                          int num_tail_lines) {
  if (num_tail_lines > 0) {
    dump_log.WriteLine(_T("Tailing last %d lines of file"), num_tail_lines);
  }
  dump_log.WriteLine(_T("-------------------------------------"));
  if (File::Exists(file_path)) {
    FileReader reader;
    HRESULT hr = reader.Init(file_path, 2048);
    if (FAILED(hr)) {
      dump_log.WriteLine(_T("Unable to open %s: 0x%x."), file_path, hr);
      return;
    }

    CString current_line;
    std::list<CString> tail_lines;

    while (SUCCEEDED(reader.ReadLineString(&current_line))) {
      if (num_tail_lines == 0) {
        // We're not doing a tail, so just print the entire file.
        dump_log.WriteLine(current_line);
      } else {
        // Collect the lines in a queue until we're done.
        tail_lines.push_back(current_line);
        if (tail_lines.size() > static_cast<size_t>(num_tail_lines)) {
          tail_lines.pop_front();
        }
      }
    }

    // Print out the tail lines collected from the file, if they exist.
    if (num_tail_lines > 0) {
      for (std::list<CString>::const_iterator it = tail_lines.begin();
           it != tail_lines.end();
           ++it) {
        dump_log.WriteLine(*it);
      }
    }
  } else {
    dump_log.WriteLine(_T("File does not exist."));
  }
}

}  // namespace omaha

