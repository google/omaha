// Copyright 2007-2009 Google Inc.
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
// Event Logger provides a simple mechanism to log events to Windows
// Event Log. A few overloads are defined to simplify logging by reducing
// the number of parameters that must be provided. The overloads are
// implemented in terms of the EventLogger class.
//
// The event logging works in both debug and optimized builds. This is not
// a substitute for the debug log. Instead it is a way to provide some level
// of transparency into what Google Update is doing at runtime and to help
// diagnosing end user issues.
//
// Familiarity with Windows Event Log is helpful in understanding how
// these wrappers are to be used. Windows Event Log uses localized strings
// in a message file and it substitutes string insterts that correspond to
// formatting characters in the message string. In addtion, the log is able
// to record raw data, herein provided by a context string, which may be
// useful to provide some context around the formatted message.

// TODO(omaha): Provide some control for the verbosity level in the log.
// TODO(omaha): Perhaps there is a better way to define the overloaded
// wrappers below. I chose a compromise between the easy of use while not
// mixing up different string parameters that have different meanings.

#ifndef OMAHA_GOOPDATE_EVENT_LOGGER_H__
#define OMAHA_GOOPDATE_EVENT_LOGGER_H__

#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

void LogEventHelper(WORD type, DWORD id, size_t count, const TCHAR** strings,
                    const TCHAR* ctx);

// Logs an event to the Application log
inline void LogEvent(WORD type, DWORD id) {
  LogEventHelper(type, id, 0, NULL, NULL);
}

inline void LogEvent(WORD type, DWORD id, const TCHAR* s) {
  const TCHAR* strings[] = {s};
  LogEventHelper(type, id, arraysize(strings), strings, NULL);
}

inline void LogEvent(WORD type, DWORD id, const TCHAR* s1, const TCHAR* s2) {
  const TCHAR* strings[] = {s1, s2};
  LogEventHelper(type, id, arraysize(strings), strings, NULL);
}

inline void LogEvent(WORD type, DWORD id, const TCHAR* s1, const TCHAR* s2,
                     const TCHAR* s3) {
  const TCHAR* strings[] = {s1, s2, s3};
  LogEventHelper(type, id, arraysize(strings), strings, NULL);
}

// Logs an event to the Application log with a context string.
inline void LogEventContext(WORD type, DWORD id, const TCHAR* ctx) {
  LogEventHelper(type, id, 0, NULL, ctx);
}

inline void LogEventContext(WORD type, DWORD id, const TCHAR* s,
                            const TCHAR* ctx) {
  const TCHAR* strings[] = {s};
  LogEventHelper(type, id, arraysize(strings), strings, ctx);
}

inline void LogEventContext(WORD type, DWORD id, const TCHAR* s1,
                            const TCHAR* s2, const TCHAR* ctx) {
  const TCHAR* strings[] = {s1, s2};
  LogEventHelper(type, id, arraysize(strings), strings, ctx);
}

inline void LogEventContext(WORD type, DWORD id, const TCHAR* s1,
                            const TCHAR* s2, const TCHAR* s3,
                            const TCHAR* ctx) {
  const TCHAR* strings[] = {s1, s2, s3};
  LogEventHelper(type, id, arraysize(strings), strings, ctx);
}

class EventLogger {
 public:
  // Creates an event source for the "Application" log so that EventViewer can
  // map event identifier codes to message strings.
  static HRESULT AddEventSource(
      const TCHAR* src_name,       // Event source name.
      const TCHAR* msg_dll_path);  // Path for message DLL.

  static HRESULT RemoveEventSource(
      const TCHAR* src_name);      // Event source name.

  // Writes an entry at the end of event log that contains the source name.
  static HRESULT ReportEvent(
      const TCHAR* src_name,       // Event source name.
      WORD type,                   // Type of the event to be logged.
      WORD category,               // Event category.
      DWORD id,                    // Event identifier.
      WORD count,                  // Count of insert strings.
      const TCHAR** strings,       // Insert strings.
      size_t buf_size,             // Size of binary data to append.
      void* buffer);               // Buffer containing the binary data.

  // Default name for the event source.
  static const TCHAR* const kSourceName;

  // Default event category.
  static const WORD kDefaultCategory = 0;
};

class GoogleUpdateLogEvent {
 public:
  GoogleUpdateLogEvent(int type, int id, bool is_machine)
      : type_(type),
        id_(id),
        is_machine_(is_machine) {}
  GoogleUpdateLogEvent() : type_(0), id_(0), is_machine_(false) {}
  ~GoogleUpdateLogEvent() {}
  void WriteEvent();
  void set_event_desc(const CString& desc) { event_desc_ = desc; }
  void set_event_text(const CString& text) { event_text_ = text; }

 private:
  CString event_desc_;
  CString event_text_;
  int type_;
  int id_;
  bool is_machine_;

  DISALLOW_EVIL_CONSTRUCTORS(GoogleUpdateLogEvent);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_EVENT_LOGGER_H__

