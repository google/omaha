// Copyright 2007-2010 Google Inc.
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
// TODO(omaha): for reliability sake, the code that sets up the exception
// handler should be very minimalist and not call so much outside of this
// module. One idea is to split the crash module in two: one minimalist part
// responsible for setting up the exception handler and one that is uploading
// the crash.

#include "omaha/goopdate/crash.h"

#include <windows.h>
#include <shlwapi.h>
#include <atlbase.h>
#include <atlstr.h>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/app_util.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/exception_barrier.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/event_logger.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/goopdate_metrics.h"
#include "omaha/goopdate/stats_uploader.h"
#include "third_party/breakpad/src/client/windows/common/ipc_protocol.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "third_party/breakpad/src/client/windows/sender/crash_report_sender.h"

using google_breakpad::ClientInfo;
using google_breakpad::CrashGenerationServer;
using google_breakpad::CrashReportSender;
using google_breakpad::CustomClientInfo;
using google_breakpad::ExceptionHandler;
using google_breakpad::ReportResult;

namespace omaha {

const TCHAR kPipeNamePrefix[] = _T("\\\\.\\pipe\\GoogleCrashServices");

const ACCESS_MASK kPipeAccessMask = FILE_READ_ATTRIBUTES  |
                                    FILE_READ_DATA        |
                                    FILE_WRITE_ATTRIBUTES |
                                    FILE_WRITE_DATA       |
                                    SYNCHRONIZE;

CString Crash::module_filename_;
CString Crash::crash_dir_;
CString Crash::checkpoint_file_;
CString Crash::version_postfix_  = kCrashVersionPostfixString;
CString Crash::crash_report_url_ = kUrlCrashReport;
int Crash::max_reports_per_day_  = kCrashReportMaxReportsPerDay;
ExceptionHandler* Crash::exception_handler_ = NULL;
CrashGenerationServer* Crash::crash_server_ = NULL;

bool Crash::is_machine_ = false;
const TCHAR* const Crash::kDefaultProductName = _T("Google Error Reporting");

HRESULT Crash::Initialize(bool is_machine) {
  is_machine_ = is_machine;

  HRESULT hr = GetModuleFileName(NULL, &module_filename_);
  if (FAILED(hr)) {
    return hr;
  }

  hr = InitializeCrashDir();
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(!crash_dir_.IsEmpty());
  CORE_LOG(L2, (_T("[crash dir %s]"), crash_dir_));

  // The checkpoint file maintains state information for the crash report
  // client, such as the number of reports per day successfully sent.
  checkpoint_file_ = ConcatenatePath(crash_dir_, _T("checkpoint"));
  if (checkpoint_file_.IsEmpty()) {
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }

  return S_OK;
}

HRESULT Crash::InstallCrashHandler(bool is_machine) {
  CORE_LOG(L3, (_T("[Crash::InstallCrashHandler][is_machine %d]"), is_machine));

  HRESULT hr = Initialize(is_machine);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to initialize Crash][0x%08x]"), hr));
    return hr;
  }

  // Only installs the exception handler if the process is not a crash report
  // process. If the crash reporter crashes as well, this results in an
  // infinite loop.
  bool is_crash_report_process = false;
  if (FAILED(IsCrashReportProcess(&is_crash_report_process)) ||
      is_crash_report_process) {
    return S_OK;
  }

  // Allocate this instance dynamically so that it is going to be
  // around until the process terminates. Technically, this instance "leaks",
  // but this is actually the correct behavior.
  if (exception_handler_) {
    delete exception_handler_;
  }
  exception_handler_ = new ExceptionHandler(crash_dir_.GetString(),
                                            NULL,
                                            &Crash::MinidumpCallback,
                                            NULL,
                                            ExceptionHandler::HANDLER_ALL);

  // Breakpad does not get the exceptions that are not propagated to the
  // UnhandledExceptionFilter. This is the case where we crashed on a stack
  // which we do not own, such as an RPC stack. To get these exceptions we
  // initialize a static ExceptionBarrier object.
  ExceptionBarrier::set_handler(&Crash::EBHandler);

  CORE_LOG(L2, (_T("[exception handler has been installed]")));
  return S_OK;
}

void Crash::UninstallCrashHandler() {
  ExceptionBarrier::set_handler(NULL);
  delete exception_handler_;
  exception_handler_ = NULL;

  CORE_LOG(L2, (_T("[exception handler has been uninstalled]")));
}

HRESULT Crash::CrashHandler(bool is_machine,
                            const google_breakpad::ClientInfo& client_info,
                            const CString& crash_filename) {
  // GoogleCrashHandler.exe is only aggregating metrics at process exit. Since
  // GoogleCrashHandler.exe is long-running however, we hardly ever exit. As a
  // consequence, the metrics below will be reported very infrequently. This
  // call below will do additional aggregation of metrics, so that we can report
  // the metrics below in a timely manner.
  ON_SCOPE_EXIT(AggregateMetrics, is_machine);

  // Count the number of crashes requested by applications.
  ++metric_oop_crashes_requested;

  DWORD pid = client_info.pid();
  OPT_LOG(L1, (_T("[client requested dump][pid %d]"), pid));

  ASSERT1(!crash_filename.IsEmpty());
  if (crash_filename.IsEmpty()) {
    OPT_LOG(L1, (_T("[no crash file]")));
    ++metric_oop_crashes_crash_filename_empty;
    return E_UNEXPECTED;
  }

  CString custom_info_filename;
  HRESULT hr = CreateCustomInfoFile(crash_filename,
                                    client_info.GetCustomInfo(),
                                    &custom_info_filename);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[CreateCustomInfoFile failed][0x%08x]"), hr));
    ++metric_oop_crashes_createcustominfofile_failed;
    return hr;
  }

  // Start a sender process to handle the crash.
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(crash_filename);
  builder.set_custom_info_filename(custom_info_filename);
  builder.set_is_machine_set(is_machine);
  CString cmd_line = builder.GetCommandLine(module_filename_);
  hr = StartSenderWithCommandLine(&cmd_line);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[StartSenderWithCommandLine failed][0x%08x]"), hr));
    ++metric_oop_crashes_startsenderwithcommandline_failed;
    return hr;
  }

  OPT_LOG(L1, (_T("[client dump handled][pid %d]"), pid));
  return S_OK;
}

// The implementation must be as simple as possible and use only the
// resources it needs to start a reporter process and then exit.
bool Crash::MinidumpCallback(const wchar_t* dump_path,
                             const wchar_t* minidump_id,
                             void*,
                             EXCEPTION_POINTERS*,
                             MDRawAssertionInfo*,
                             bool succeeded) {
  if (succeeded && *dump_path && *minidump_id) {
    // We need a way to see if the crash happens while we are installing
    // something. This is a tough spot to be doing anything at all since
    // we've been handling a crash.
    // TODO(omaha): redesign a better mechanism.
    bool is_interactive = Crash::IsInteractive();

    // TODO(omaha): format a command line without extra memory allocations.
    CString crash_filename;
    crash_filename.Format(_T("%s\\%s.dmp"), dump_path, minidump_id);
    EnclosePath(&crash_filename);

    // CommandLineBuilder escapes the program name before returning the
    // command line to the caller.
    CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
    builder.set_is_interactive_set(is_interactive);
    builder.set_crash_filename(crash_filename);
    builder.set_is_machine_set(is_machine_);
    CString cmd_line = builder.GetCommandLine(module_filename_);

    // Set an environment variable which the crash reporter process will
    // inherit. We don't want to install a crash handler for the reporter
    // process to avoid an infinite loop in the case the reporting process
    // crashes also. When the reporting process begins execution, the presence
    // of this environment variable is tested, and the crash handler will not be
    // installed.
    if (::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName, (_T("1")))) {
      STARTUPINFO si = {sizeof(si)};
      PROCESS_INFORMATION pi = {0};
      if (::CreateProcess(NULL,
                          cmd_line.GetBuffer(),
                          NULL,
                          NULL,
                          false,
                          0,
                          NULL,
                          NULL,
                          &si,
                          &pi)) {
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
      }
    }
  }

  // There are two ways to stop execution of the current process: ExitProcess
  // and TerminateProcess. Calling ExitProcess results in calling the
  // destructors of the static objects before the process exits.
  // TerminateProcess unconditionally stops the process so no user mode code
  // executes beyond this point.
  ::TerminateProcess(::GetCurrentProcess(),
                     static_cast<UINT>(GOOPDATE_E_CRASH));
  return true;
}

HRESULT Crash::StartSenderWithCommandLine(CString* cmd_line) {
  TCHAR* env_vars = ::GetEnvironmentStrings();
  if (env_vars == NULL) {
    return HRESULTFromLastError();
  }

  // Add an environment variable to the crash reporter process to indicate it
  // not to install a crash handler. This avoids an infinite loop in the case
  // the reporting process crashes also. When the reporting process begins
  // execution, the presence of this environment variable is tested, and the
  // crash handler will not be installed.
  CString new_var;
  new_var.Append(kNoCrashHandlerEnvVariableName);
  new_var.Append(_T("=1"));

  // Compute the length of environment variables string. The format of the
  // string is Name1=Value1\0Name2=Value2\0Name3=Value3\0\0.
  const TCHAR* current = env_vars;
  size_t env_vars_char_count = 0;
  while (*current) {
    size_t sub_length = _tcslen(current) + 1;
    env_vars_char_count += sub_length;
    current += sub_length;
  }
  // Add one to length to count the trailing NULL character marking the end of
  // all environment variables.
  ++env_vars_char_count;

  // Copy the new environment variable and the existing variables in a new
  // buffer.
  size_t new_var_char_count = new_var.GetLength() + 1;
  scoped_array<TCHAR> new_env_vars(
      new TCHAR[env_vars_char_count + new_var_char_count]);
  size_t new_var_byte_count = new_var_char_count * sizeof(TCHAR);
  memcpy(new_env_vars.get(),
         static_cast<const TCHAR*>(new_var),
         new_var_byte_count);
  size_t env_vars_byte_count = env_vars_char_count * sizeof(TCHAR);
  memcpy(new_env_vars.get() + new_var_char_count,
         env_vars,
         env_vars_byte_count);
  ::FreeEnvironmentStrings(env_vars);

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(NULL,
                       cmd_line->GetBuffer(),
                       NULL,
                       NULL,
                       false,
                       CREATE_UNICODE_ENVIRONMENT,
                       new_env_vars.get(),
                       NULL,
                       &si,
                       &pi)) {
    return HRESULTFromLastError();
  }

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  return S_OK;
}

HRESULT Crash::CreateCustomInfoFile(const CString& dump_file,
                                    const CustomClientInfo& custom_client_info,
                                    CString* custom_info_filepath) {
  // Since goopdate_utils::WriteNameValuePairsToFile is implemented in terms
  // of WritePrivateProfile API, relative paths are relative to the Windows
  // directory instead of the current directory of the process.
  ASSERT(!::PathIsRelative(dump_file), (_T("Path must be absolute")));

  // Determine the path for custom info file.
  CString filepath = GetPathRemoveExtension(dump_file);
  filepath.AppendFormat(_T(".txt"));

  // Create a map of name/value pairs from custom client info.
  std::map<CString, CString> custom_info_map;
  for (int i = 0; i < custom_client_info.count; ++i) {
    custom_info_map[custom_client_info.entries[i].name] =
                    custom_client_info.entries[i].value;
  }

  HRESULT hr = goopdate_utils::WriteNameValuePairsToFile(filepath,
                                                         kCustomClientInfoGroup,
                                                         custom_info_map);
  if (FAILED(hr)) {
    return hr;
  }

  *custom_info_filepath = filepath;
  return S_OK;
}

// Backs up the crash and uploads it if allowed to.
HRESULT Crash::DoSendCrashReport(bool can_upload,
                                 bool is_out_of_process,
                                 const CString& crash_filename,
                                 const ParameterMap& parameters,
                                 CString* report_id) {
  ASSERT1(!crash_filename.IsEmpty());
  ASSERT1(report_id);
  report_id->Empty();

  if (!File::Exists(crash_filename)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  CString product_name = GetProductName(parameters);
  VERIFY1(SUCCEEDED(SaveLastCrash(crash_filename, product_name)));

  HRESULT hr = S_OK;
  if (can_upload) {
    hr = UploadCrash(is_out_of_process, crash_filename, parameters, report_id);
  }

  return hr;
}

HRESULT Crash::UploadCrash(bool is_out_of_process,
                           const CString& crash_filename,
                           const ParameterMap& parameters,
                           CString* report_id) {
  ASSERT1(report_id);
  report_id->Empty();

  // Calling this avoids crashes in WinINet. See: http://b/1258692
  EnsureRasmanLoaded();

  ASSERT1(!crash_dir_.IsEmpty());
  ASSERT1(!checkpoint_file_.IsEmpty());

  // Do best effort to send the crash. If it can't communicate with the backend,
  // it retries a few times over a few hours time interval.
  HRESULT hr = S_OK;
  for (int i = 0; i != kCrashReportAttempts; ++i) {
    std::wstring report_code;
    CrashReportSender sender(checkpoint_file_.GetString());
    sender.set_max_reports_per_day(max_reports_per_day_);
    CORE_LOG(L2, (_T("[Uploading crash report]")
                  _T("[%s][%s]"), crash_report_url_, crash_filename));
    ASSERT1(!crash_report_url_.IsEmpty());
    ReportResult res = sender.SendCrashReport(crash_report_url_.GetString(),
                                              parameters,
                                              crash_filename.GetString(),
                                              &report_code);
    switch (res) {
      case google_breakpad::RESULT_SUCCEEDED:
        report_id->SetString(report_code.c_str());
        hr = S_OK;
        break;

      case google_breakpad::RESULT_FAILED:
        OPT_LOG(L2, (_T("[Crash report failed but it will retry sending]")));
        ::Sleep(kCrashReportResendPeriodMs);
        hr = E_FAIL;
        break;

      case google_breakpad::RESULT_REJECTED:
        hr = GOOPDATE_E_CRASH_REJECTED;
        break;

      case google_breakpad::RESULT_THROTTLED:
        hr = GOOPDATE_E_CRASH_THROTTLED;
        break;

      default:
        hr = E_FAIL;
        break;
    };

    // Continue the retry loop only when it could not contact the server.
    if (res != google_breakpad::RESULT_FAILED) {
      break;
    }
  }

  // The event source for the out-of-process crashes is the product name.
  // Therefore, in the event of an out-of-process crash, the log entry
  // appears to be generated by the product that crashed.
  CString product_name = is_out_of_process ? GetProductName(parameters) :
                                             kAppName;
  CString event_text;
  uint16 event_type(0);
  if (!report_id->IsEmpty()) {
    event_type = EVENTLOG_INFORMATION_TYPE;
    event_text.Format(_T("Crash uploaded. Id=%s."), *report_id);
  } else {
    ASSERT1(FAILED(hr));
    event_type = EVENTLOG_WARNING_TYPE;
    event_text.Format(_T("Crash not uploaded. Error=0x%x."), hr);
  }
  VERIFY1(SUCCEEDED(Crash::Log(event_type,
                               kCrashUploadEventId,
                               product_name,
                               event_text)));

  UpdateCrashUploadMetrics(is_out_of_process, hr);

  return hr;
}

HRESULT Crash::SaveLastCrash(const CString& crash_filename,
                             const CString& product_name) {
  if (product_name.IsEmpty()) {
    return E_INVALIDARG;
  }
  CString tmp;
  tmp.Format(_T("%s-last.dmp"), product_name);
  CString save_filename = ConcatenatePath(crash_dir_, tmp);
  if (save_filename.IsEmpty()) {
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }

  CORE_LOG(L2, (_T("[Crash::SaveLastCrash]")
                 _T("[to %s][from %s]"), save_filename, crash_filename));

  return ::CopyFile(crash_filename,
                    save_filename,
                    false) ? S_OK : HRESULTFromLastError();
}

HRESULT Crash::CleanStaleCrashes() {
  CORE_LOG(L3, (_T("[Crash::CleanStaleCrashes]")));

  // ??- sequence is a c++ trigraph corresponding to a ~. Escape it.
  const TCHAR kWildCards[] = _T("???????\?-???\?-???\?-???\?-????????????.dmp");
  std::vector<CString> crash_files;
  HRESULT hr = File::GetWildcards(crash_dir_, kWildCards, &crash_files);
  if (FAILED(hr)) {
    return hr;
  }

  time64 now = GetCurrent100NSTime();
  for (size_t i = 0; i != crash_files.size(); ++i) {
    CORE_LOG(L3, (_T("[found crash file][%s]"), crash_files[i]));
    FILETIME creation_time = {0};
    if (SUCCEEDED(File::GetFileTime(crash_files[i],
                                    &creation_time,
                                    NULL,
                                    NULL))) {
      double time_diff =
          static_cast<double>(now - FileTimeToTime64(creation_time));
      if (abs(time_diff) >= kDaysTo100ns) {
        VERIFY1(::DeleteFile(crash_files[i]));
      }
    }
  }

  return S_OK;
}

HRESULT Crash::Report(bool can_upload_in_process,
                      const CString& crash_filename,
                      const CString& custom_info_filename,
                      const CString& lang) {
  const bool is_out_of_process = !custom_info_filename.IsEmpty();
  HRESULT hr = S_OK;
  if (is_out_of_process) {
    hr = ReportProductCrash(true, crash_filename, custom_info_filename, lang);
    ::DeleteFile(custom_info_filename);
  } else {
    hr = ReportGoogleUpdateCrash(can_upload_in_process,
                                 crash_filename,
                                 custom_info_filename,
                                 lang);
  }
  ::DeleteFile(crash_filename);
  CleanStaleCrashes();
  return hr;
}

HRESULT Crash::ReportGoogleUpdateCrash(bool can_upload,
                                       const CString& crash_filename,
                                       const CString& custom_info_filename,
                                       const CString& lang) {
  OPT_LOG(L1, (_T("[Crash::ReportGoogleUpdateCrash]")));

  UNREFERENCED_PARAMETER(custom_info_filename);

  // Build the map of additional parameters to report along with the crash.
  CString ver(GetVersionString() + version_postfix_);

  ParameterMap parameters;
  parameters[_T("prod")] = _T("Update2");
  parameters[_T("ver")]  = ver;
  parameters[_T("lang")] = lang;

  CString event_text;
  event_text.Format(
      _T("Google Update has encountered a fatal error.\r\n")
      _T("ver=%s;lang=%s;is_machine=%d;upload=%d;minidump=%s"),
      ver, lang, is_machine_, can_upload ? 1 : 0, crash_filename);
  VERIFY1(SUCCEEDED(Crash::Log(EVENTLOG_ERROR_TYPE,
                               kCrashReportEventId,
                               kAppName,
                               event_text)));

  ++metric_crashes_total;

  CString report_id;
  return DoSendCrashReport(can_upload,
                           false,             // Google Update crash.
                           crash_filename,
                           parameters,
                           &report_id);
}

HRESULT Crash::ReportProductCrash(bool can_upload,
                                  const CString& crash_filename,
                                  const CString& custom_info_filename,
                                  const CString& lang) {
  OPT_LOG(L1, (_T("[Crash::ReportProductCrash]")));

  UNREFERENCED_PARAMETER(lang);

  // All product crashes must be uploaded.
  ASSERT1(can_upload);

  // Count the number of crashes the sender was requested to handle.
  ++metric_oop_crashes_total;

  std::map<CString, CString> parameters_temp;
  HRESULT hr = goopdate_utils::ReadNameValuePairsFromFile(
      custom_info_filename,
      kCustomClientInfoGroup,
      &parameters_temp);
  if (FAILED(hr)) {
    return hr;
  }

  ParameterMap parameters;
  std::map<CString, CString>::const_iterator iter;
  for (iter = parameters_temp.begin(); iter != parameters_temp.end(); ++iter) {
    parameters[iter->first.GetString()] = iter->second.GetString();
  }

  CString report_id;
  return hr = DoSendCrashReport(can_upload,
                                true,                  // Out of process crash.
                                crash_filename,
                                parameters,
                                &report_id);
}

void __stdcall Crash::EBHandler(EXCEPTION_POINTERS* ptrs) {
  if (exception_handler_) {
    exception_handler_->WriteMinidumpForException(ptrs);
  }
}

HRESULT Crash::GetExceptionInfo(const CString& crash_filename,
                                MINIDUMP_EXCEPTION* ex_info) {
  ASSERT1(ex_info);
  ASSERT1(!crash_filename.IsEmpty());

  // Dynamically link with the dbghelp to avoid runtime resource bloat.
  scoped_library dbghelp(::LoadLibrary(_T("dbghelp.dll")));
  if (!dbghelp) {
    return HRESULTFromLastError();
  }

  typedef BOOL (WINAPI *MiniDumpReadDumpStreamFun)(void* base_of_dump,
                                                   ULONG stream_number,
                                                   MINIDUMP_DIRECTORY* dir,
                                                   void** stream_pointer,
                                                   ULONG* stream_size);

  MiniDumpReadDumpStreamFun minidump_read_dump_stream =
      reinterpret_cast<MiniDumpReadDumpStreamFun>(::GetProcAddress(get(dbghelp),
                                                  "MiniDumpReadDumpStream"));
  ASSERT1(minidump_read_dump_stream);
  if (!minidump_read_dump_stream) {
    return HRESULTFromLastError();
  }

  // The minidump file must be mapped in memory before reading the streams.
  scoped_hfile file(::CreateFile(crash_filename,
                                 GENERIC_READ,
                                 FILE_SHARE_READ,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_READONLY,
                                 NULL));
  ASSERT1(file);
  if (!file) {
    return HRESULTFromLastError();
  }
  scoped_file_mapping file_mapping(::CreateFileMapping(get(file),
                                                       NULL,
                                                       PAGE_READONLY,
                                                       0,
                                                       0,
                                                       NULL));
  if (!file_mapping) {
    return HRESULTFromLastError();
  }
  scoped_file_view base_of_dump(::MapViewOfFile(get(file_mapping),
                                                FILE_MAP_READ,
                                                0,
                                                0,
                                                0));
  if (!base_of_dump) {
    return HRESULTFromLastError();
  }

  // Read the exception stream and pick up the exception record.
  MINIDUMP_DIRECTORY minidump_directory = {0};
  void* stream_pointer = NULL;
  ULONG stream_size = 0;
  bool result = !!(*minidump_read_dump_stream)(get(base_of_dump),
                                               ExceptionStream,
                                               &minidump_directory,
                                               &stream_pointer,
                                               &stream_size);
  if (!result) {
    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
  }
  MINIDUMP_EXCEPTION_STREAM* exception_stream =
      static_cast<MINIDUMP_EXCEPTION_STREAM*>(stream_pointer);
  ASSERT1(stream_pointer);
  ASSERT1(stream_size);

  *ex_info = exception_stream->ExceptionRecord;
  return S_OK;
}

bool Crash::IsInteractive() {
  bool result = false;
  ::EnumWindows(&Crash::EnumWindowsCallback, reinterpret_cast<LPARAM>(&result));
  return result;
}

// Finds if the given window is in the current process.
BOOL CALLBACK Crash::EnumWindowsCallback(HWND hwnd, LPARAM param) {
  DWORD pid = 0;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (::IsWindowVisible(hwnd) && pid == ::GetCurrentProcessId() && param) {
    *reinterpret_cast<bool*>(param) = true;
    return false;
  }
  return true;
}

HRESULT Crash::InitializeCrashDir() {
  crash_dir_.Empty();
  ConfigManager* cm = ConfigManager::Instance();
  CString dir = is_machine_ ? cm->GetMachineCrashReportsDir() :
                              cm->GetUserCrashReportsDir();

  if (is_machine_ && !dir.IsEmpty()) {
    HRESULT hr = InitializeDirSecurity(&dir);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[failed to initialize crash dir security][0x%x]"), hr));
      ::RemoveDirectory(dir);
    }
  }

  // Use the temporary directory of the process if the crash directory can't be
  // initialized for any reason. Users can't read files in other users'
  // temporary directories so the temp dir is good option to still have
  // crash handling.
  if (dir.IsEmpty()) {
    dir = app_util::GetTempDir();
  }

  if (dir.IsEmpty()) {
    return GOOPDATE_E_CRASH_NO_DIR;
  }

  crash_dir_ = dir;
  return S_OK;
}

HRESULT Crash::InitializeDirSecurity(CString* dir) {
  ASSERT1(dir);

  // Users can only read permissions on the crash dir.
  CDacl dacl;
  const uint8 kAceFlags = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  if (!dacl.AddAllowedAce(Sids::System(), GENERIC_ALL, kAceFlags) ||
      !dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL, kAceFlags) ||
      !dacl.AddAllowedAce(Sids::Users(), READ_CONTROL, kAceFlags)) {
    return GOOPDATE_E_CRASH_SECURITY_FAILED;
  }

  SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION |
                            PROTECTED_DACL_SECURITY_INFORMATION;
  DWORD error = ::SetNamedSecurityInfo(dir->GetBuffer(),
                                       SE_FILE_OBJECT,
                                       si,
                                       NULL,
                                       NULL,
                                       const_cast<ACL*>(dacl.GetPACL()),
                                       NULL);
  if (error != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(error);
  }
  return S_OK;
}

// Checks for the presence of an environment variable. We are not interested
// in the value of the variable but only in its presence.
HRESULT Crash::IsCrashReportProcess(bool* is_crash_report_process) {
  ASSERT1(is_crash_report_process);
  if (::GetEnvironmentVariable(kNoCrashHandlerEnvVariableName, NULL, 0)) {
    *is_crash_report_process = true;
    return S_OK;
  } else {
    DWORD error(::GetLastError());
    *is_crash_report_process = false;
    return error == ERROR_ENVVAR_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(error);
  }
}

HRESULT Crash::Log(uint16 type,
                   uint32 id,
                   const TCHAR* source,
                   const TCHAR* description) {
  ASSERT1(source);
  ASSERT1(description);
  return EventLogger::ReportEvent(source,
                                  type,
                                  0,            // Category.
                                  id,
                                  1,            // Number of strings.
                                  &description,
                                  0,            // Raw data size.
                                  NULL);        // Raw data.
}

CString Crash::GetProductName(const ParameterMap& parameters) {
  // The crash is logged using the value of 'prod' if available or
  // a default constant string otherwise.
  CString product_name;
  ParameterMap::const_iterator it = parameters.find(_T("prod"));
  const bool is_found = it != parameters.end() && !it->second.empty();
  return is_found ? it->second.c_str() : kDefaultProductName;
}

void Crash::UpdateCrashUploadMetrics(bool is_out_of_process, HRESULT hr) {
  switch (hr) {
    case S_OK:
      if (is_out_of_process) {
        ++metric_oop_crashes_uploaded;
      } else {
        ++metric_crashes_uploaded;
      }
      break;

    case E_FAIL:
      if (is_out_of_process) {
        ++metric_oop_crashes_failed;
      } else {
        ++metric_crashes_failed;
      }
      break;

    case GOOPDATE_E_CRASH_THROTTLED:
      if (is_out_of_process) {
        ++metric_oop_crashes_throttled;
      } else {
        ++metric_crashes_throttled;
      }
      break;

    case GOOPDATE_E_CRASH_REJECTED:
      if (is_out_of_process) {
        ++metric_oop_crashes_rejected;
      } else {
        ++metric_crashes_rejected;
      }
      break;

    default:
      ASSERT1(false);
      break;
  }
}

int Crash::CrashNow() {
#ifdef DEBUG
  CORE_LOG(LEVEL_ERROR, (_T("[Crash::CrashNow]")));
  int foo = 10;
  int bar = foo - 10;
  int baz = foo / bar;
  return baz;
#else
  return 0;
#endif
}

HRESULT Crash::CreateLowIntegrityDesc(CSecurityDesc* sd) {
  ASSERT1(sd);
  ASSERT1(!sd->GetPSECURITY_DESCRIPTOR());

  if (!vista_util::IsVistaOrLater()) {
    return S_FALSE;
  }

  return sd->FromString(LOW_INTEGRITY_SDDL_SACL) ? S_OK :
                                                   HRESULTFromLastError();
}

bool Crash::AddPipeSecurityDaclToDesc(bool is_machine, CSecurityDesc* sd) {
  ASSERT1(sd);

  CAccessToken current_token;
  if (!current_token.GetEffectiveToken(TOKEN_QUERY)) {
    OPT_LOG(LE, (_T("[Failed to get current thread token]")));
    return false;
  }

  CDacl dacl;
  if (!current_token.GetDefaultDacl(&dacl)) {
    OPT_LOG(LE, (_T("[Failed to get default DACL]")));
    return false;
  }

  if (!is_machine) {
    sd->SetDacl(dacl);
    return true;
  }

  if (!dacl.AddAllowedAce(ATL::Sids::Users(), kPipeAccessMask)) {
    OPT_LOG(LE, (_T("[Failed to setup pipe security]")));
    return false;
  }

  if (!dacl.AddDeniedAce(ATL::Sids::Network(), FILE_ALL_ACCESS)) {
    OPT_LOG(LE, (_T("[Failed to setup pipe security]")));
    return false;
  }

  sd->SetDacl(dacl);
  return true;
}

bool Crash::BuildPipeSecurityAttributes(bool is_machine,
                                        CSecurityAttributes* sa) {
  ASSERT1(sa);

  CSecurityDesc sd;
  HRESULT hr = CreateLowIntegrityDesc(&sd);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to CreateLowIntegrityDesc][0x%x]"), hr));
    return false;
  }

  if (!AddPipeSecurityDaclToDesc(is_machine, &sd)) {
    return false;
  }

  sa->Set(sd);

#ifdef _DEBUG
  // Print SDDL for debugging.
  CString sddl;
  sd.ToString(&sddl, OWNER_SECURITY_INFORMATION |
                     GROUP_SECURITY_INFORMATION |
                     DACL_SECURITY_INFORMATION  |
                     SACL_SECURITY_INFORMATION  |
                     LABEL_SECURITY_INFORMATION);
  CORE_LOG(L1, (_T("[Pipe security SDDL][%s]"), sddl));
#endif

  return true;
}

bool Crash::BuildCrashDirSecurityAttributes(CSecurityAttributes* sa) {
  ASSERT1(sa);

  CDacl dacl;
  CAccessToken current_token;

  if (!current_token.GetEffectiveToken(TOKEN_QUERY)) {
    OPT_LOG(LE, (_T("[Failed to get current thread token]")));
    return false;
  }

  if (!current_token.GetDefaultDacl(&dacl)) {
    OPT_LOG(LE, (_T("[Failed to get default DACL]")));
    return false;
  }

  CSecurityDesc sd;
  sd.SetDacl(dacl);
  // Prevent the security settings on the parent folder of CrashReports folder
  // from being inherited by children of CrashReports folder.
  sd.SetControl(SE_DACL_PROTECTED, SE_DACL_PROTECTED);
  sa->Set(sd);

#ifdef _DEBUG
  // Print SDDL for debugging.
  CString sddl;
  sd.ToString(&sddl);
  CORE_LOG(L1, (_T("[Folder security SDDL][%s]"), sddl));
#endif

  return true;
}

HRESULT Crash::StartServer() {
  CORE_LOG(L1, (_T("[Crash::StartServer]")));
  ++metric_crash_start_server_total;

  std::wstring dump_path(crash_dir_);

  // Append the current user's sid to the pipe name so that machine and
  // user instances of the crash server open different pipes.
  CString user_sid;
  HRESULT hr = user_info::GetCurrentUser(NULL, NULL, &user_sid);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to get SID for current user][0x%08x]"), hr));
    return hr;
  }
  CString pipe_name(kPipeNamePrefix);
  pipe_name.AppendFormat(_T("\\%s"), user_sid);

  CSecurityAttributes pipe_sec_attrs;

  // * If running as machine, use custom security attributes on the pipe to
  // allow all users on the local machine to connect to it. Add the low
  // integrity SACL to account for browser plugins.
  // * If running as user, the default security descriptor for the
  // pipe grants full control to the system account, administrators,
  // and the creator owner. Add the low integrity SACL to account for browser
  // plugins.
  if (!BuildPipeSecurityAttributes(is_machine_, &pipe_sec_attrs)) {
    return GOOPDATE_E_CRASH_SECURITY_FAILED;
  }

  scoped_ptr<CrashGenerationServer> crash_server(
      new CrashGenerationServer(std::wstring(pipe_name),
                                &pipe_sec_attrs,
                                ClientConnectedCallback, NULL,
                                ClientCrashedCallback, NULL,
                                ClientExitedCallback, NULL,
                                true, &dump_path));

  if (!crash_server->Start()) {
    CORE_LOG(LE, (_T("[CrashServer::Start failed]")));
    return GOOPDATE_E_CRASH_START_SERVER_FAILED;
  }

  crash_server_ = crash_server.release();

  ++metric_crash_start_server_succeeded;
  return S_OK;
}

void Crash::StopServer() {
  CORE_LOG(L1, (_T("[Crash::StopServer]")));
  delete crash_server_;
  crash_server_ = NULL;
}


void _cdecl Crash::ClientConnectedCallback(void* context,
                                           const ClientInfo* client_info) {
  ASSERT1(!context);
  ASSERT1(client_info);
  UNREFERENCED_PARAMETER(context);
  OPT_LOG(L1, (_T("[Client connected][%d]"), client_info->pid()));
}

void _cdecl Crash::ClientCrashedCallback(void* context,
                                         const ClientInfo* client_info,
                                         const std::wstring* dump_path) {
  ASSERT1(!context);
  ASSERT1(client_info);
  UNREFERENCED_PARAMETER(context);
  OPT_LOG(L1, (_T("[Client crashed][%d]"), client_info->pid()));

  CString crash_filename(dump_path ? dump_path->c_str() : NULL);
  HRESULT hr = Crash::CrashHandler(is_machine_, *client_info, crash_filename);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[CrashHandler failed][0x%08x]"), hr));
  }
}

void _cdecl Crash::ClientExitedCallback(void* context,
                                        const ClientInfo* client_info) {
  ASSERT1(!context);
  ASSERT1(client_info);
  UNREFERENCED_PARAMETER(context);
  OPT_LOG(L1, (_T("[Client exited][%d]"), client_info->pid()));
}

}  // namespace omaha

