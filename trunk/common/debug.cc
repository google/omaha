// Copyright 2003-2009 Google Inc.
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
// Debug functions

#include "omaha/common/debug.h"

#include <dbghelp.h>
#include <wtsapi32.h>
#include <atlstr.h>
#ifdef _DEBUG
#include <atlcom.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#endif
#include <stdlib.h>
#include <signal.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/app_util.h"
#include "omaha/common/clipboard.h"
#include "omaha/common/constants.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/const_config.h"
#include "omaha/common/const_debug.h"
#include "omaha/common/const_timeouts.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/string.h"
#include "omaha/common/system.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/common/vista_utils.h"

namespace omaha {

#ifdef _DEBUG
#define kSprintfBuffers (100)  // number of buffers for SPRINTF
#else
#define kSprintfBuffers (3)  // number of buffers for SPRINTF
#endif

// pad SPRINTF buffer to check for overruns
#if SHIPPING
#define kSprintfBufferOverrunPadding 0
#else  // !SHIPPING
#ifdef DEBUG
#define kSprintfBufferOverrunPadding 20000
#else
#define kSprintfBufferOverrunPadding 1024
#endif  // DEBUG
#endif  // SHIPPING

#define kErrorRequestToSend                                                    \
    _T("*** Please hit Ignore to continue and send error information to the ") \
    kAppName                                                                   \
    _T(" team ***\n*** These details have been pasted to the clipboard ***")

#define kMaxReportSummaryLen (1024*100)  // max length of report summary string

#define kReportIdsLock kLockPrefix                                             \
    _T("Report_Ids_Lock_57146B01-6A07-4b8d-A1D8-0C3AFC3B2F9B")

SELECTANY bool g_always_assert = false;
SELECTANY TCHAR *g_additional_status_ping_info = NULL;

#define kSprintfMaxLen (1024 + 2)  // max length that wvsprintf writes is 1024
static bool g_initialized_sprintf = false;
static volatile LONG g_sprintf_interlock = 0;
static int g_current_sprintf_buffer = 0;
static TCHAR *g_sprintf_buffer = NULL;
static TCHAR *g_sprintf_buffers[kSprintfBuffers];
SELECTANY volatile LONG g_debugassertrecursioncheck = 0;
static int g_total_reports = 0;

SELECTANY ReportIds g_report_ids;

// Builds a full path name out of the given filename. If the filename is
// a relative path, it is appended to the debug directory. Otherwise, if the
// filename is a full path, it returns it as the full debug filename.
static CString MakeFullDebugFilename(const TCHAR *filename) {
  CString full_name;
  if (lstrlen(filename) <= 2 || filename[1] != _T(':')) {
    full_name = GetDebugDirectory();
    full_name += L"\\";
  }
  full_name += filename;
  return full_name;
}


// Displays the assert box. Due to session isolation, MB_SERVICE_NOTIFICATION
// flag does not work for Vista services. In this case, use WTS to display
// a message box in the active console session.
void ShowAssertDialog(const TCHAR *message, const TCHAR *title) {
  int ret = 0;
  OSVERSIONINFOEX osviex = {sizeof(OSVERSIONINFOEX), 0};
  const bool is_vista_or_greater =
     ::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osviex)) &&
     osviex.dwMajorVersion >= 6;
  bool is_system_process = false;
  if (is_vista_or_greater &&
      SUCCEEDED(IsSystemProcess(&is_system_process)) &&
      is_system_process) {
    DWORD session_id = System::WTSGetActiveConsoleSessionId();
    if (session_id == kInvalidSessionId) {
      session_id = WTS_CURRENT_SESSION;
    }
    DWORD response = 0;
    ::WTSSendMessage(WTS_CURRENT_SERVER_HANDLE,
                     session_id,
                     const_cast<TCHAR*>(title),
                     _tcslen(title) * sizeof(TCHAR),
                     const_cast<TCHAR*>(message),
                     _tcslen(message) * sizeof(TCHAR),
                     MB_ABORTRETRYIGNORE | MB_ICONERROR,
                     0,
                     &response,
                     true);
    ret = response;
  } else {
    ret = ::MessageBoxW(NULL,
                        message,
                        title,
                        MB_ABORTRETRYIGNORE |
                        MB_ICONERROR        |
                        MB_SERVICE_NOTIFICATION);
  }

  switch (ret) {
    case IDABORT:
      // Terminate the process if the user chose 'Abort'. Calling ExitProcess
      // here results in calling the destructors for static objects which can
      // result in deadlocks.
      raise(SIGABRT);
      break;

    case IDRETRY:
      // Break if the user chose "Retry".
      __debugbreak();
      break;
    default:
      // By default we ignore the message.
      break;
  }
}

DebugObserver* g_debug_observer = NULL;

// replaces the debug observer, returns the previous value.
DebugObserver* SetDebugObserver(DebugObserver* observer) {
  DebugObserver* old_value = g_debug_observer;
  g_debug_observer = observer;
  return old_value;
}

DebugObserver* PeekDebugObserver() {
  return g_debug_observer;
}

int SehSendMinidump(unsigned int code,
                    struct _EXCEPTION_POINTERS *ep,
                    time64 time_between_minidumps) {
    if (code == EXCEPTION_BREAKPOINT)
    return EXCEPTION_CONTINUE_SEARCH;

  if (::IsDebuggerPresent())
    return EXCEPTION_CONTINUE_SEARCH;

  OutputDebugString(L"**SehSendMinidump**\r\n");

  if (g_debug_observer) {
    return g_debug_observer->SehSendMinidump(code, ep, time_between_minidumps);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

#if defined(_DEBUG) || defined(ASSERT_IN_RELEASE)
CallInterceptor<DebugAssertFunctionType> debug_assert_interceptor;

// Replaces the debug assert function; returns the old value.
DebugAssertFunctionType* ReplaceDebugAssertFunction(
    DebugAssertFunctionType* replacement) {
  return debug_assert_interceptor.ReplaceFunction(replacement);
}

void OnAssert(const char *expr, const TCHAR *msg,
              const char *filename, int32 linenumber) {
  if (g_debug_observer) {
    g_debug_observer->OnAssert(expr, msg, filename, linenumber);
  }
}
#endif

#if defined(_DEBUG)
CString OnDebugReport(uint32 id,
                      bool is_report,
                      ReportType type,
                      const char *expr,
                      const TCHAR *message,
                      const char *filename,
                      int32 linenumber,
                      DebugReportKind debug_report_kind) {
  CString trace;
  if (g_debug_observer) {
    trace = g_debug_observer->OnDebugReport(id, is_report,
                                            type, expr, message, filename,
                                            linenumber, debug_report_kind);
  }
  return trace;
}

void SendExceptionReport(const TCHAR *log_file, const TCHAR *filename, int line,
                         const TCHAR *type, uint32 id, bool offline) {
  if (g_debug_observer) {
    g_debug_observer->SendExceptionReport(log_file, filename, line,
                                          type, id, offline);
  }
}
#endif


#ifdef _DEBUG  // won't compile since _CrtDbgReport isn't defined.

#include <crtdbg.h>    // NOLINT
static CString g_report_summary;

// dump summary of reports on exit
SELECTANY ReportSummaryGenerator g_report_summary_generator;

ReportSummaryGenerator::~ReportSummaryGenerator() {
  DumpReportSummary();
}

void ReportSummaryGenerator::DumpReportSummary() {
  if (g_total_reports) {
    ::OutputDebugString(L"REPORT SUMMARY:\r\n");
    ::OutputDebugString(SPRINTF(L"%d total reports\r\n", g_total_reports));
    ::OutputDebugString(g_report_summary);
  } else {
    ::OutputDebugString(L"NO REPORTS!!\r\n");
  }
}

TCHAR *ReportSummaryGenerator::GetReportSummary() {
  TCHAR *s = new TCHAR[kMaxReportSummaryLen];
  if (s) {
    s[0] = 0;
    if (g_total_reports) {
      SafeStrCat(s, L"REPORT SUMMARY:\r\n\r\n", kMaxReportSummaryLen);
      SafeStrCat(s,
                 SPRINTF(L"%d total reports\r\n\r\n", g_total_reports),
                 kMaxReportSummaryLen);
      SafeStrCat(s,
                 g_report_summary.
                     Left(kMaxReportSummaryLen - lstrlen(s) - 1).GetString(),
                 kMaxReportSummaryLen);
      CString report_string = g_report_ids.DebugReportString();
      ReplaceCString(report_string, L"&", L"\r\n");
      SafeStrCat(s,
                 report_string.
                     Left(kMaxReportSummaryLen - lstrlen(s) - 1).GetString(),
                 kMaxReportSummaryLen);
    } else {
      SafeStrCat(s, L"NO REPORTS!!\r\n", kMaxReportSummaryLen);
    }
  }

  return s;
}

static CAtlMap<CString, uint32> g_reports_done;

#endif  // _DEBUG

#ifdef _DEBUG

void TraceError(DWORD error) {
  HLOCAL mem = NULL;
  ::FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    static_cast<LPVOID>(_AtlBaseModule.GetResourceInstance()),
    error,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // default language
    reinterpret_cast<TCHAR*>(&mem),
    0,
    NULL);

  TCHAR* str = reinterpret_cast<TCHAR*>(::LocalLock(mem));
  ::OutputDebugString(str);
  REPORT(false, R_ERROR, (str), 3968294226);
  ::LocalFree(mem);
}

// ban ASSERT/VERIFY/REPORT to prevent recursion
#undef ASSERT
#undef VERIFY
#undef REPORT

// TODO(omaha): fix static initialization order below.
// The initialization order of static variables is not deterministic per C++
// standard and it depends completely on the compiler implementation.
// For VC++ compiler we are using, it seems to work as expected.
// One real fix is to put all definitons of static variables inside a class and
// define a boolean variable in this class to indicate that all necessary
// static initializations have been done.
//
// The follow definition is used to detect whether we get the exception
// during initializing static variables. If this is the case, DebugReport()
// will not function and will throw an exception because some of its refering
// static variables are not initialized yet (i.e. g_reports_done).
const int kTestInitStaticVariablesDoneValue = 1234;
struct TestInitStaticVariablesDone {
  int value;
  TestInitStaticVariablesDone() : value(kTestInitStaticVariablesDoneValue) {}
};
static TestInitStaticVariablesDone test_var;

bool DebugReport(unsigned int id,
                 ReportType type,
                 const char *expr,
                 const TCHAR *message,
                 const char *filename,
                 int linenumber,
                 DebugReportKind debug_report_kind) {
  int recursion_count = ::InterlockedIncrement(&g_debugassertrecursioncheck);
  ON_SCOPE_EXIT(::InterlockedDecrement, &g_debugassertrecursioncheck);
  if (recursion_count > 1) {
    ::OutputDebugString(_T("recursive debugreport skipped\n"));
    return 1;
  }

  if (debug_assert_interceptor.interceptor()) {
    // call replacement function (typically used for unit tests)
    // Note that I'm doing this inside the in_assert block for paranoia;
    // it's not really necessary and perhaps the wrong choice.
    debug_assert_interceptor.interceptor()(expr, CT2A(message), filename,
                                            linenumber);
    return true;
  }


  // Check whether we have already finished initializing all static variables
  // needed for executing DebugReport(). If not, bail out.
  if (test_var.value != kTestInitStaticVariablesDoneValue) {
    CString debug_msg;
    debug_msg.Format(_T("%hs:%d - %s - %S"),
                     filename, linenumber, message, expr);
    debug_msg.Append(_T("\n\nException occurs while initializing ")
                     _T("static variables needed for DebugReport"));
    ShowAssertDialog(debug_msg, _T("DebugReport"));
    return true;
  }

  bool is_assert = debug_report_kind == DEBUGREPORT_ASSERT;
  bool is_report = debug_report_kind == DEBUGREPORT_REPORT;
  bool is_abort  = debug_report_kind == DEBUGREPORT_ABORT;

  if (is_report)
    g_total_reports++;

  g_report_ids.ReleaseReport(id);

  if (type == R_FATAL) {
    if (is_report) {
      // Treat as ASSERT
      is_report = false;
      is_assert = true;
    }
  }

  bool always_assert = g_always_assert;

  if (always_assert) {
    is_report = false;
    is_assert = true;
  }

  if (!message) {
    message = _T("");
  }

  // log to debugger
  TCHAR *debug_string;
  // ::OutputDebugString(DEBUG_LOG_SEPARATOR);
  ::OutputDebugString(is_report ? _T("REPORT: ") :
                                  (is_assert ? _T("ASSERT: ") : _T("ABORT: ")));

  CFixedStringT<CString, 1024> proc_name = app_util::GetAppName();

  // last %s now %s skip %d
  const TCHAR* format = message && *message ?
                        _T("[%hs:%d][%hs][%s]") : _T("[%hs:%d][%hs]");
  debug_string = SPRINTF(format, filename, linenumber, expr, message);

  // String_Int64ToString(g_last_report_time, 10),
  // String_Int64ToString(time, 10), skip_report));

  // ::OutputDebugString(DEBUG_LOG_SEPARATOR);
  // ::OutputDebugString(_T("\n"));

#ifdef LOGGING
  // Log the reports via the logging system to all loggers.
  CString what = is_report ? _T("REPORT") :
                             is_assert ? _T("ASSERT") : _T("ABORT");
  LC_LOG(LC_LOGGING, LEVEL_ERROR, (_T("[%s]%s"), what, debug_string));
#else
  ::OutputDebugString(debug_string);
  ::OutputDebugString(_T("\n"));
#endif

  // skip sending strack trace for duplicate reports
  CString report_id;
  report_id.Format(_T("%hs:%d"), filename, linenumber);

  uint32 prev_reports = 0;
  if (g_reports_done.Lookup(report_id, prev_reports) && is_report) {
    prev_reports++;
    g_reports_done.SetAt(report_id, prev_reports);
    ::OutputDebugString(SPRINTF(_T("skipping duplicate report %s %d\n"),
                                report_id.GetString(),
                                prev_reports));
    return 1;
  }

  prev_reports++;
  g_reports_done.SetAt(report_id, prev_reports);

  g_report_summary.Append(debug_string);
  g_report_summary.Append(L" (");
  g_report_summary.Append(itostr(id));
  g_report_summary.Append(L")");
  g_report_summary.Append(L"\r\n");

  // ::OutputDebugString(_T("log to file\n"));

  // log to file
  CString path_name(MakeFullDebugFilename(kCiDebugLogFile));
  HANDLE h = CreateFile(path_name,
                        GENERIC_WRITE | GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

  HANDLE assert_file = INVALID_HANDLE_VALUE;
  if (is_assert) {
    path_name = MakeFullDebugFilename(kCiAssertOccurredFile);
    assert_file = CreateFile(path_name,
                             GENERIC_WRITE,
                             0,
                             0,
                             OPEN_ALWAYS,
                             FILE_FLAG_WRITE_THROUGH,
                             NULL);
  }

  HANDLE abort_file = INVALID_HANDLE_VALUE;
  if (is_abort) {
    path_name = MakeFullDebugFilename(kCiAbortOccurredFile);
    abort_file = CreateFile(path_name,
                            GENERIC_WRITE,
                            0,
                            0,
                            OPEN_ALWAYS,
                            FILE_FLAG_WRITE_THROUGH,
                            NULL);
  }

  if (h != INVALID_HANDLE_VALUE ||
      assert_file != INVALID_HANDLE_VALUE ||
      abort_file != INVALID_HANDLE_VALUE) {
    // more convenient for now to have this in UTF8
    char *utf8_buffer = new char[(lstrlen(debug_string)*2) + 1];
    if (utf8_buffer) {
        int conv_bytes = WideCharToMultiByte(CP_UTF8,
                                             0,
                                             debug_string,
                                             lstrlen(debug_string),
                                             utf8_buffer,
                                             (lstrlen(debug_string) * 2) + 1,
                                             NULL,
                                             NULL);

        if (conv_bytes) {
            DWORD bytes_written;
            BOOL result;

            if (h != INVALID_HANDLE_VALUE) {
              SetFilePointer(h, 0, NULL, FILE_END);
              result = ::WriteFile(h,
                                   (LPCVOID)utf8_buffer,
                                   conv_bytes,
                                   &bytes_written,
                                   NULL);
              result = ::WriteFile(h,
                                   (LPCVOID)DEBUG_LOG_SEPARATOR_CHAR,
                                   strlen(DEBUG_LOG_SEPARATOR_CHAR),
                                   &bytes_written,
                                   NULL);
            }
            if (assert_file != INVALID_HANDLE_VALUE) {
              result = ::WriteFile(assert_file,
                                   (LPCVOID)utf8_buffer,
                                   conv_bytes,
                                   &bytes_written,
                                   NULL);
            }
            if (abort_file != INVALID_HANDLE_VALUE) {
              result = ::WriteFile(abort_file,
                                   (LPCVOID)utf8_buffer,
                                   conv_bytes,
                                   &bytes_written,
                                   NULL);
            }
        }

        delete [] utf8_buffer;
    }
  }

  if (h != INVALID_HANDLE_VALUE) {
    ::CloseHandle(h);
  }
  if (assert_file != INVALID_HANDLE_VALUE) {
    ::CloseHandle(assert_file);
  }
  if (abort_file != INVALID_HANDLE_VALUE) {
    ::CloseHandle(abort_file);
  }

  CString stack_trace = OnDebugReport(id, is_report, type, expr, message,
                                      filename, linenumber, debug_report_kind);

  if (is_report) {
    return 1;
  }

  ::OutputDebugString(L"show assert dialog\r\n");
  ::OutputDebugString(stack_trace.GetString());

  CString process_path;
  GetModuleFileName(NULL, &process_path);

  static TCHAR clipboard_string[4096] = {0};
  lstrcpyn(clipboard_string,
           SPRINTF(L"%ls (pid=%i)\r\n%hs:%d\r\n\r\n%hs\r\n%s\r\n\r\n",
                   process_path,
                   ::GetCurrentProcessId(),
                   filename,
                   linenumber,
                   expr,
                   message),
           arraysize(clipboard_string));
  stack_trace = stack_trace.Left(
      arraysize(clipboard_string) - lstrlen(clipboard_string) - 1);
  SafeStrCat(clipboard_string, stack_trace, arraysize(clipboard_string));
  SetClipboard(clipboard_string);

  stack_trace = stack_trace.Left(kMaxStackTraceDialogLen);

  CString assert_text;
  assert_text.Format(_T("Assertion (%ls) failed!\r\n\r\nProcess: %d ")
    _T("(0x%08X)\r\nThread %d (0x%08X)\r\nProgram: %ls\r\n")
    _T("Version: %s\r\nFile: %hs\r\nLine: %d\r\n\r\n"),
    debug_report_kind == DEBUGREPORT_ASSERT ?
      L"Assert" : (debug_report_kind == DEBUGREPORT_ABORT ?
      L"Abort" : L"Report"),
    ::GetCurrentProcessId(), ::GetCurrentProcessId(), ::GetCurrentThreadId(),
    ::GetCurrentThreadId(), process_path, omaha::GetVersionString(), filename,
    linenumber);

  if (lstrlen(message) > 0) {
    assert_text.AppendFormat(
        _T("Expression: %hs\r\nMessage: %s\r\n\r\n%s\r\n\r\n%hs"),
        expr,
        message,
        stack_trace,
        kErrorRequestToSend);
  } else {
    assert_text.AppendFormat(_T("Expression: %hs\r\n\r\n%s\r\n\r\n%hs"),
                             expr, stack_trace, kErrorRequestToSend);
  }

  ShowAssertDialog(assert_text, CString(filename));
  return 1;
}

#endif  // #ifdef _DEBUG


ReportIds::ReportIds() {
  data_.report_counts_num = 0;
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kReportIdsLock,
                           vista_util::IsUserAdmin(),
                           &lock_attr);
  InitializeWithSecAttr(lock_attr.name, &lock_attr.sa);
}

ReportIds::~ReportIds() {
  // don't attempt to write out reports from low integrity mode.
  //
  // TODO(omaha): save reports from a low integrity process (specifically IE
  // which does some extra special magic to thwart this) --
  // possible by launch a process or a broker, etc.
  if (vista::IsProcessProtected()) {
    return;
  }

  if (data_.report_counts_num != 0) {
    // Back the report IDs to the registry
    __mutexBlock(this) {
      ReportData *reports_in_config = NULL;
      if (LoadReportData(&reports_in_config)) {
        MergeReports(reports_in_config, &data_);
        SaveReportData(reports_in_config);

        byte *data = reinterpret_cast<byte*>(reports_in_config);
        delete [] data;
      } else {
        // There's no data in the registry, so just fill it up with this
        // component's data.
        SaveReportData(&data_);
      }
    }
  }
}

const TCHAR* const GetRegKeyShared() {
  return vista_util::IsUserAdmin() ? _T("HKLM\\") kCiRegKeyShared :
                                     _T("HKCU\\") kCiRegKeyShared;
}

void ReportIds::ResetReportsAfterPing() {
  // We will lose reports from TRS between the time DebugReportString was called
  // and now. We'll also lose reports from non TRS components if they exit in
  // between this time.  Not important.
  data_.report_counts_num = 0;
  __mutexBlock(this) {
    RegKey::DeleteValue(GetRegKeyShared(), kRegValueReportIds);
  }
}

void ReportIds::MergeReports(ReportData *data1, const ReportData *data2) {
  // Loop through each report ID from data2.  If we find it already, increment
  // the report's count in data1. Otherwise, if there's enough space, add the
  // report ID to data1.
  uint32 i, j;
  for (i = 0; i < data2->report_counts_num; ++i) {
    bool duplicate_report = false;
    for (j = 0; j < data1->report_counts_num; ++j) {
      if (data1->report_ids[j] == data2->report_ids[i]) {
        // uint16 is promoted to int.
        data1->report_counts[j] = static_cast<uint16>(
            data1->report_counts[j] + data2->report_counts[i]);
        duplicate_report = true;
      }
    }

    if (!duplicate_report && j < kMaxUniqueReports) {
      data1->report_ids[j] = data2->report_ids[i];
      data1->report_counts[j] = data2->report_counts[i];
      data1->report_counts_num++;
    }
  }
}

bool ReportIds::LoadReportData(ReportData **data) {
  DWORD byte_count = 0;
  *data = NULL;
  HRESULT hr = RegKey::GetValue(GetRegKeyShared(),
                                kRegValueReportIds,
                                reinterpret_cast<byte**>(data),
                                &byte_count);
  if (SUCCEEDED(hr)) {
    if (byte_count == sizeof(ReportData)) {
      return true;
    } else {
      delete[] data;
      data = NULL;
      return false;
    }
  } else {
    return false;
  }
}

void ReportIds::SaveReportData(ReportData *data) {
  if (data->report_counts_num) {
    RegKey::SetValue(GetRegKeyShared(),
                     kRegValueReportIds,
                     reinterpret_cast<byte*>(data),
                     sizeof(ReportData));
  }
}

bool ReportIds::ReleaseReport(uint32 id) {
  uint32 max = data_.report_counts_num;

  // If two threads call simultaneously, might miss one of an existing report
  // here; not important.

  uint32 i = 0;
  for (i = 0; i < max; ++i) {
    if (data_.report_ids[i] == id) {
      data_.report_counts[i]++;
      return true;
      }
  }

  // If two threads call simultaneously, might overwrite first of another
  // report; not important.

  if (i < kMaxUniqueReports) {
    data_.report_ids[i] = id;
    data_.report_counts[i] = 1;
    data_.report_counts_num = i + 1;  // Set only after setting ids and count;
                                      // don't use ++
  }

#ifdef _DEBUG
  OutputDebugString(SPRINTF(_T("release report %u\n"), id));
#endif
  // must return true (return value of REPORT)
  return true;
}

// caller deletes the string
TCHAR *ReportIds::DebugReportString() {
  TCHAR *s = new TCHAR[(kMaxUniqueReports * kMaxReportCountString) + 1];
  if (!s) { return NULL; }
  s[0] = '\0';

#if 0
  // this version if we use a hash table for the report counts:
  uint32 id;
  uint16 count;
  hr = g_reports->First(&found, &id, &count);
  while (SUCCEEDED(hr) && found) {
    if (count) {
        status_info.AppendFormat(_T("%d=%d"), id, static_cast<uint32>(count));
    }

    hr = g_reports->Next(&found, &id, &count);
  }
#endif

  // The registry will contain the REPORTs from other components, so get that
  // list and merge it with TRS'.
  __mutexBlock(this) {
    ReportData *reports_in_config = NULL;
    if (LoadReportData(&reports_in_config)) {
      MergeReports(&data_, reports_in_config);

      byte *data = reinterpret_cast<byte*>(reports_in_config);
      delete[] data;
    }
  }

  TCHAR *current_pos = s;
  for (uint32 i = 0; i < data_.report_counts_num; i++) {
    if (data_.report_counts[i]) {
      // should be no chance of overflow, ok to use wsprintf
      int n = wsprintf(current_pos,
                       _T("%u:%u,"),
                       data_.report_ids[i],
                       static_cast<uint32>(data_.report_counts[i]));
      current_pos += n;
    }
  }

  return s;
}

// A simple helper function whose sole purpose is
// to isolate the dtor from SPRINTF which uses try/except.
// app_util::GetAppName returns a CString and dtor's
// aren't allowed in functions with try/except when
// the /EHsc flag is set.
void FillInSprintfErrorString(const TCHAR * format, TCHAR * error_string) {
  wsprintf(error_string, L"SPRINTF buffer overrun %ls %hs %ls",
    app_util::GetAppName(), omaha::GetVersionString(), format);
}

// following is currently included in release build
// return string from format+arglist; for debugging
TCHAR * __cdecl SPRINTF(const TCHAR * format, ...) {
  while (::InterlockedCompareExchange(&g_sprintf_interlock, 1, 0) == 1) {
  // while (::InterlockedIncrement(&g_sprintf_interlock)>1) {
    // ::InterlockedDecrement(&g_sprintf_interlock);
    // Don't process APCs here.
    // Can lead to infinite recursion, for example: filecap->logging->filecap...
    Sleep(0);
  }

  g_current_sprintf_buffer++;
  if (g_current_sprintf_buffer >= kSprintfBuffers) {
    g_current_sprintf_buffer = 0;
  }

  TCHAR *sprintf_buf = NULL;

  if (!g_initialized_sprintf) {  // initialize buffers
    g_sprintf_buffer = new TCHAR[
        ((kSprintfMaxLen + 1) * kSprintfBuffers) +
        kSprintfBufferOverrunPadding];
    TCHAR* buffer = g_sprintf_buffer;
    if (!buffer) { goto cleanup; }
    for (int i = 0; i < kSprintfBuffers; ++i) {
      g_sprintf_buffers[i] = buffer;
      buffer += kSprintfMaxLen + 1;
    }

#if !SHIPPING
    for (int i = ((kSprintfMaxLen+1) * kSprintfBuffers);
         i < ((kSprintfMaxLen + 1) * kSprintfBuffers) +
            kSprintfBufferOverrunPadding;
         ++i) {
      g_sprintf_buffer[i] = 1;
    }
#endif

    // InitializeCriticalSection(&g_sprintf_critical_section);
    g_initialized_sprintf = true;
  }

  sprintf_buf = g_sprintf_buffers[g_current_sprintf_buffer];

  // EnterCriticalSection(&g_sprintf_critical_section);

  __try {
    // create the formatted CString
    va_list vl;
    va_start(vl, format);
#ifdef DEBUG
    StringCbVPrintfW(sprintf_buf, kSprintfMaxLen, format, vl);
#else
    wvsprintfW(sprintf_buf, format, vl);
#endif
    va_end(vl);

#if !SHIPPING
    for (int i = ((kSprintfMaxLen+1) * kSprintfBuffers);
         i < ((kSprintfMaxLen+1) * kSprintfBuffers) +
            kSprintfBufferOverrunPadding;
         ++i) {
      if (g_sprintf_buffer[i] != 1) {
        TCHAR error_string[1024];
        FillInSprintfErrorString(format, error_string);
        MessageBox(NULL,
                   error_string,
                   error_string,
                   MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
        break;
      }
    }
#endif
  }
  __except(EXCEPTION_EXECUTE_HANDLER) {
    lstrcpyn(sprintf_buf, _T("sprintf failure"), kSprintfMaxLen);
  }

  // LeaveCriticalSection(&g_sprintf_critical_section);

  cleanup:

  ::InterlockedDecrement(&g_sprintf_interlock);
  return sprintf_buf;
}

#if 0
  TCHAR * __cdecl SPRINTF(const TCHAR * format, ...) {
  ASSERT(format, (L""));

  g_current_sprintf_buffer++;
  if (g_current_sprintf_buffer >= kSprintfBuffers) {
    g_current_sprintf_buffer = 0;
    }

  TCHAR *sprintf_buf = sprintf_buffers[g_current_sprintf_buffer];
  CFixedStringT<CString, kSprintfMaxLen> out;

  va_list argptr;
  va_start(argptr, format);
  out.FormatV(format, argptr);
  va_end(argptr);

  // copy to fixed return buffers
  SafeStrCat(sprintf_buf,
             out.GetBufferSetLength(kSprintfMaxLen),
             g_current_sprintf_buffer);
  sprintf_buf[kSprintfMaxLen] = '\0';

  return sprintf_buf;
}
#endif

// Cleanup allocated memory
class SprintfCleaner {
 public:
  SprintfCleaner() {}

  ~SprintfCleaner() {
    while (::InterlockedCompareExchange(&g_sprintf_interlock, 1, 0) == 1) {
      Sleep(0);
    }

    if (g_initialized_sprintf) {
      delete[] g_sprintf_buffer;
      for (int i = 0; i < kSprintfBuffers; ++i) {
        g_sprintf_buffers[i] = NULL;
      }
      g_initialized_sprintf = false;
    }

    ::InterlockedDecrement(&g_sprintf_interlock);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SprintfCleaner);
};

static SprintfCleaner cleaner;

// This is for our testers to find asserts in release mode.
#if !defined(_DEBUG) && defined(ASSERT_IN_RELEASE)
bool ReleaseAssert(const char *expr,
                   const TCHAR *msg,
                   const char *filename,
                   int32 linenumber) {
  ASSERT(filename, (L""));
  ASSERT(msg, (L""));
  ASSERT(expr, (L""));

  if (debug_assert_interceptor.interceptor()) {
    // call replacement function (typically used for unit tests)
    // Note that I'm doing this inside the in_assert block for paranoia;
    // it's not really necessary and perhaps the wrong choice.
    debug_assert_interceptor.interceptor()(expr,
                                           CT2CA(msg),
                                           filename,
                                           linenumber);
    return true;
  }

  OnAssert(expr, msg, filename, linenumber);

  // Also put up a message box.
  TCHAR error_string[1024] = {0};
  wsprintf(error_string,
           L"App: %ls\r\n"
           L"Expr: %hs\r\n"
           L"File: %hs\r\n"
           L"Line: %d\r\n"
           L"Version: %hs\r\n"
           L"Message: ",
           app_util::GetAppName(),
           expr,
           filename,
           linenumber,
           VER_TIMESTAMP_STR_FILE);
  SafeStrCat(error_string, msg, arraysize(error_string));
  SafeStrCat(error_string,
             L"\r\n\r\n*** This message has been copied to the clipboard. ***",
             arraysize(error_string));
  SetClipboard(error_string);

  TCHAR title_string[1024];
  wsprintf(title_string, kAppName L" ASSERT %ls %hs",
    app_util::GetAppName(), VER_TIMESTAMP_STR_FILE);
  MessageBox(NULL,
             error_string,
             title_string,
             MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
  return true;
}
#endif

#if defined(_DEBUG)
void DebugAbort(const TCHAR *msg,
                const char* filename,
                int32 linenumber,
                bool do_abort) {
  DebugReport(0, R_FATAL, "", msg, filename, linenumber, DEBUGREPORT_ABORT);
  if (do_abort) {
    abort();
  }
}
#else
void ReleaseAbort(const TCHAR *msg,
                  const char* filename,
                  int32 linenumber,
                  bool do_abort) {
  // Send info to the server.
#if defined(ASSERT_IN_RELEASE)
  OnAssert("", msg, filename, linenumber);
#endif

  // Also put up a message box.
  TCHAR error_string[1024] = {0};
  wsprintf(error_string,
           L"App: %ls\r\n"
           L"File: %hs\r\n"
           L"Line: %d\r\n"
           L"Version: %hs\r\n"
           L"Message: ",
           app_util::GetAppName(),
           filename,
           linenumber,
           omaha::GetVersionString());
  SafeStrCat(error_string, msg, arraysize(error_string));
  SafeStrCat(error_string,
             L"\r\n\r\n*** This message has been copied to the clipboard. ***",
             arraysize(error_string));
  SetClipboard(error_string);

  TCHAR title_string[1024];
  wsprintf(title_string,
           kAppName L" ABORT %ls %hs",
           app_util::GetAppName(),
           omaha::GetVersionString());
  MessageBox(NULL,
             error_string,
             title_string,
             MB_OK | MB_SETFOREGROUND | MB_TOPMOST);

  if (do_abort) {
    abort();
  }
}
#endif


#ifdef _DEBUG

void DumpInterface(IUnknown* unknown) {
  if (!unknown)
    return;

  OutputDebugString(_T("------------------------------------------------\r\n"));

  // Open the HKCR\Interfaces key where the IIDs of marshalable interfaces
  // are stored.
  RegKey key;
  if (SUCCEEDED(key.Open(HKEY_CLASSES_ROOT, _T("Interface"), KEY_READ))) {
    TCHAR name[_MAX_PATH + 1] = {0};
    DWORD name_size = _MAX_PATH;
    DWORD index = 0;
    FILETIME last_written;

    //
    // Enumerate through the IIDs and see if the object supports it
    // by calling QueryInterface.
    //
    while (::RegEnumKeyEx(key.Key(),
                          index++,
                          name,
                          &name_size,
                          NULL,
                          NULL,
                          NULL,
                          &last_written) == ERROR_SUCCESS) {
      // Convert the string to an IID
      IID iid;
      HRESULT hr = ::CLSIDFromString(name, &iid);

      CComPtr<IUnknown> test;
      if (unknown->QueryInterface(iid,
                                  reinterpret_cast<void**>(&test)) == S_OK) {
        //
        // The object supports this interface.
        // See if we can get a human readable name for the interface
        // If not, the name buffer already contains the string
        // representation of the IID, which we'll use as a fallback.
        //
        RegKey sub_key;
        if (sub_key.Open(key.Key(), name, KEY_READ) == S_OK) {
          scoped_array<TCHAR> display;
          // If this fails, we should still have the IID
          if (sub_key.GetValue(NULL, address(display)) == S_OK)
            lstrcpyn(name, display.get(), _MAX_PATH);
        }

        CString fmt;
        fmt.Format(_T("  %s\r\n"), name);
        OutputDebugString(fmt);
      }

      ZeroMemory(name, arraysize(name));
      name_size = _MAX_PATH;
    }
  }

  OutputDebugString(_T("------------------------------------------------\r\n"));
}
#endif

// TODO(omaha): the implementation below is using CStrings so it is not very
// conservative in terms of memory allocations.
int SehNoMinidump(unsigned int code, struct _EXCEPTION_POINTERS *,
                  const char *filename, int32 linenumber, bool show_message) {
  if (code == EXCEPTION_BREAKPOINT)
    return EXCEPTION_CONTINUE_SEARCH;

  uint32 latest_cl = 0;
#ifdef VERSION_LATEST_CL
  latest_cl = VERSION_LATEST_CL;
#endif

  if (show_message) {
    TCHAR message[1025] = {0};
    wsprintf(message,
             _T("Exception %x in %s %s %u\r\n\r\n%hs:%d\r\n"),
             code,
             app_util::GetAppName(),
             omaha::GetVersionString(),
             latest_cl,
             filename,
             linenumber);

    SetClipboard(message);
    uint32 type = MB_ABORTRETRYIGNORE |
                  MB_ICONERROR |
                  MB_SERVICE_NOTIFICATION |
                  MB_SETFOREGROUND |
                  MB_TOPMOST;
    int ret = ::MessageBox(NULL, message, _T("Exception"), type);
    switch (ret) {
      case IDABORT:
        // Kamikaze if the user chose 'abort'
        ::ExitProcess(static_cast<UINT>(-1));
        break;

      case IDRETRY:
        // Break if the user chose "retry"
        __debugbreak();
        break;

      default:
        // By default we ignore the message
      break;
    }
  }
  return EXCEPTION_EXECUTE_HANDLER;
}

CString GetDebugDirectory() {
  CString debug_dir;
  CString system_drive = GetEnvironmentVariableAsString(_T("SystemDrive"));
  if (!system_drive.IsEmpty()) {
    debug_dir += system_drive;
    debug_dir += L"\\";
  }
  debug_dir += kCiDebugDirectory;
  return debug_dir;
}

}  // namespace omaha

