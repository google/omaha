// Copyright 2005-2009 Google Inc.
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
// Take over ATLASSERT
//

#ifndef OMAHA_COMMON_ATLASSERT_H__
#define OMAHA_COMMON_ATLASSERT_H__

#include <tchar.h>

#ifdef _DEBUG
#ifndef DEBUG
#error DEBUG and _DEBUG must be in sync
#endif
#endif

namespace omaha {

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

#ifdef DEBUG
extern "C" bool DebugReport(unsigned int id,
                            omaha::ReportType type,
                            const char* expr,
                            const TCHAR* message,
                            const char* filename,
                            int linenumber,
                            omaha::DebugReportKind debug_report_kind);
  #ifndef ATLASSERT
  #define ATLASSERT(expr)                         \
    do {                                          \
      if (!(expr)) {                              \
        DebugReport(0,                            \
                    omaha::R_FATAL,               \
                    #expr,                        \
                    _T("ATL assertion"),          \
                    __FILE__,                     \
                    __LINE__,                     \
                    omaha::DEBUGREPORT_ASSERT);   \
      }                                           \
    } while (0)
  #endif
#else
  #ifndef ATLASSERT
  #define ATLASSERT(expr) ((void)0)
  #endif
#endif

}  // namespace omaha

#endif  // OMAHA_COMMON_ATLASSERT_H__
