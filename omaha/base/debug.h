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

#ifndef OMAHA_BASE_DEBUG_H_
#define OMAHA_BASE_DEBUG_H_

// To create a release build with asserts turned on, uncomment the
// following line, and uncomment the linking with atls.lib in api.
//
// #define ASSERT_IN_RELEASE

#include "omaha/base/synchronized.h"
#include "omaha/base/time.h"

#define CONCATENATE_DIRECT(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)

namespace omaha {

// hash table for counts of the number of times REPORTs occur
// template<class K, class V>
// class HashTable;
// extern HashTable<uint32, uint16> *g_reports;

#define kMaxUniqueReports (20)
#define kMaxReportCountString (20)

class ReportIds;
extern ReportIds g_report_ids;
extern volatile LONG g_debugassertrecursioncheck;
extern bool g_always_assert;

const int kMaxStackTraceDialogLen = 512;  // too long and dialog box fails

enum ReportType {
  R_INFO = 1,   // Not an error, used for accumulating statistics.
  R_WARNING,    // May or may not be an error.
  R_ERROR,      // Definitely an error.
  R_FATAL       // halt program == ASSERT for release mode.
};

enum DebugReportKind {
  DEBUGREPORT_NONE   = 0,
  DEBUGREPORT_ASSERT = 1,
  DEBUGREPORT_REPORT = 2,
  DEBUGREPORT_ABORT  = 3
};

// TODO(omaha): consider merging this into DebugObserver.
//
// For automated testing, we don't (always) want asserts to fire. So
// we allow the unit test system to handle asserts instead.
//
// Give a function matching the prototype to REPLACE_ASSERT_FUNCTION to
// have your function called instead of the normal assert function.
typedef int DebugAssertFunctionType(const char *expression,
  const char *message, const char *file, int line);

class DebugObserver {
 public:
  virtual ~DebugObserver() {}
  virtual int SehSendMinidump(unsigned int code, struct _EXCEPTION_POINTERS *ep,
                              time64 time_between_minidumps) = 0;

#if defined(_DEBUG) || defined(ASSERT_IN_RELEASE)
  virtual void OnAssert(const char *expr, const TCHAR *msg,
                        const char *filename, int32 linenumber) = 0;
#endif

#if defined(_DEBUG)
  virtual void SendExceptionReport(const TCHAR *log_file, const TCHAR *filename,
                                   int line, const TCHAR *type, uint32 id,
                                   bool offline) = 0;
  virtual CString OnDebugReport(uint32 id, bool is_report, ReportType type,
                                const char *expr, const TCHAR *message,
                                const char *filename, int32 linenumber,
                                DebugReportKind debug_report_kind) = 0;
#endif
};

// replaces the debug observer, returns the previous value.
DebugObserver* SetDebugObserver(DebugObserver* observer);
DebugObserver* PeekDebugObserver();

#ifdef __cplusplus
extern "C" {
#endif

int SehSendMinidump(unsigned int code,
                    struct _EXCEPTION_POINTERS *ep,
                    time64 time_between_minidumps);

#define kMinReportInterval100ns (60 * kSecsTo100ns)
#define kMinStackReportInterval100ns (60 * kSecsTo100ns)
#define DEBUG_LOG_SEPARATOR_CHAR "-------------------------------------------\n"
#define DEBUG_LOG_SEPARATOR     L"-------------------------------------------\n"
#define kExceptionReportHeaders L"Content-Type: binary"

#undef ASSERT
#undef VERIFY
#undef TRACE

// Holds information about REPORTS and their frequency
struct ReportData {
  uint32 report_counts_num;
  uint32 report_ids[kMaxUniqueReports];
  uint16 report_counts[kMaxUniqueReports];
};

// Used to hold REPORT IDs and to back them to the registry, where they'll be
// read and sent in a ping.
class ReportIds : public GLock {
 public:
  ReportIds();
  ~ReportIds();

  // Call this after a successful ping to clear the report IDs from the registry
  // and from this component (TRS).
  void ResetReportsAfterPing();

  // Adds a report ID to our list, if there's enough space.
  bool ReleaseReport(uint32 id);

  // Creates a string with the report IDs and their frequency.
  // Caller deletes string.
  TCHAR *DebugReportString();

 private:
  ReportData data_;

  // Merges the report data from data2 with data1.
  void MergeReports(ReportData *data1, const ReportData *data2);

  // We have to use RegKey directly, because we can't depend on the global
  // Config object during destruction.
  bool LoadReportData(ReportData **data);
  void SaveReportData(ReportData *data);

  DISALLOW_COPY_AND_ASSIGN(ReportIds);
};

#if defined(_DEBUG) || defined(ASSERT_IN_RELEASE)
  // Replaces the debug assert function; returns the old value.
  DebugAssertFunctionType *ReplaceDebugAssertFunction(DebugAssertFunctionType
                                                      *replacement);
  #define REPLACE_ASSERT_FUNCTION(replacement) \
      ReplaceDebugAssertFunction(replacement)
#else
  #define REPLACE_ASSERT_FUNCTION(replacement) NULL
#endif

#ifdef _DEBUG
  void SendExceptionReport(const TCHAR *log_file,
                           const TCHAR *filename,
                           int line,
                           const TCHAR *type,
                           uint32 id,
                           bool offline);

  void SendStackTrace(const TCHAR *filename,
                      int line,
                      const TCHAR *type,
                      bool all_threads,
                      uint32 id);

  // DEBUG MODE

  extern bool g_LSPMode;

  bool DebugReport(unsigned int id,
                   ReportType type,
                   const char *expr,
                   const TCHAR *message,
                   const char *filename,
                   int linenumber,
                   DebugReportKind debug_report_kind);

  #define VERIFY(expr, msg) ASSERT(expr, msg)  // VERIFY is ASSERT

  #define ASSERT(expr, msg)                                                    \
      do {                                                                     \
        ((expr) ? 0 : omaha::DebugReport(0, omaha::R_FATAL, #expr,             \
          omaha::SPRINTF msg, __FILE__, __LINE__, omaha::DEBUGREPORT_ASSERT)); \
      } while (0)

  #define REPORT(expr, type, msg, id)                           \
    ((expr) ? 0 : omaha::DebugReport(id,                        \
                                     type,                      \
                                     #expr,                     \
                                     omaha::SPRINTF msg,        \
                                     __FILE__,                  \
                                     __LINE__,                  \
                                     omaha::DEBUGREPORT_REPORT))
  void DebugAbort(const TCHAR* msg,
                  const char* filename,
                  int32 linenumber,
                  bool do_abort);
  #define ABORT(msg)                                            \
      omaha::DebugAbort(omaha::SPRINTF msg, __FILE__, __LINE__, true)

  void TraceError(DWORD error);
  inline void TraceLastError() { TraceError(GetLastError()); }

  #define VERIFY_SUCCEEDED(hr)                        \
  do {                                                \
    const auto ANONYMOUS_VARIABLE(__hr) = (hr);                        \
    if (FAILED(ANONYMOUS_VARIABLE(__hr))) {                            \
      VERIFY(false, (L"FAILED(hr): %#08x", ANONYMOUS_VARIABLE(__hr))); \
    }                                                 \
  } while (0)

#else  // #ifdef _DEBUG

  #ifdef ASSERT_IN_RELEASE
    bool ReleaseAssert(const char *expr,
                       const TCHAR *msg,
                       const char *filename,
                       int32 linenumber);
    #define ASSERT(expr, msg) \
        ((expr) ? 0 : ReleaseAssert(#expr, SPRINTF msg, __FILE__, __LINE__))
  #else
    #define ASSERT(expr, msg) 0
  #endif

  // VERIFY executes but does not check expression
  #define VERIFY(expr, msg) \
    do {                   \
      (expr);              \
    } while (0)
  #define VERIFY_SUCCEEDED(hr) do { (hr); } while(0)

  #define REPORT(expr, type, msg, id) \
      ((expr) ? 0 : g_report_ids.ReleaseReport(id))
  void ReleaseAbort(const TCHAR* msg,
                    const char* filename,
                    int32 linenumber,
                    bool do_abort);
  #define ABORT(msg) ReleaseAbort(SPRINTF msg, __FILE__, __LINE__, true)

#endif  // #ifdef _DEBUG

#define ASSERT1(expr) ASSERT(expr, (_T("")))
#define VERIFY1(expr) VERIFY(expr, (_T("")))

#ifdef __cplusplus
}  // extern "C"{
#endif

#ifdef _DEBUG
void ShowAssertDialog(const TCHAR *message, const TCHAR *title);

// used to automatically dump the global summary of reports when the
// program exits
class ReportSummaryGenerator {
 public:
  ReportSummaryGenerator() {}
  // calls DumpReportSummary()
  ~ReportSummaryGenerator();
  // some programs exit without calling destructors, they can use this function
  // to dump the report summary
  void DumpReportSummary();
  // get text summary of reports
  // caller is responsible for deleting the string returned
  TCHAR *GetReportSummary();
  DISALLOW_COPY_AND_ASSIGN(ReportSummaryGenerator);
};

extern ReportSummaryGenerator g_report_summary_generator;
#endif

// return string from format+arglist; for debugging; not thread-safe
TCHAR * __cdecl SPRINTF(const TCHAR * format, ...);
bool DebugError(const char * expr,
                const TCHAR * message,
                const char * filename,
                INT linenumber,
                BOOL report_only);

// shows an error dialog in DEBUG and when g_release_debug is true
//
// example usage:
//
//  __try {
//    do something that sometimes causes a known exception (e.g.,
//    calling third party code)
//  } __except(SehNoMinidump(GetExceptionCode(), GetExceptionInformation(),
//      __FILE__, __LINE__)) {
//    REPORT(false, R_ERROR, (L"exception doing something"), 103178920);
//  }
// show_message - show an error dialog in DEBUG and when g_release_debug is true
int SehNoMinidump(unsigned int code, struct _EXCEPTION_POINTERS *ep,
  const char *filename, int32 linenumber, bool show_message);

/**
* @return Always returns an error value.  If GetLastError is not ERROR_SUCCESS
*   the function returns an HRESULT value derived from GetLastError()
*/
inline HRESULT GetCurError() {
  return ::GetLastError() == ERROR_SUCCESS ?
      E_FAIL : HRESULT_FROM_WIN32(::GetLastError());
}

// Returns the directory where the debugging module stores debug-related files.
CString GetDebugDirectory();

}  // namespace omaha

#endif  // OMAHA_BASE_DEBUG_H_
