// Copyright 2010 Google Inc.
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
// Declaration of a Windows event trace consumer base class.
#ifndef BASE_EVENT_TRACE_CONSUMER_H_
#define BASE_EVENT_TRACE_CONSUMER_H_

#include <windows.h>
#include <wmistr.h>
#include <evntrace.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

// This class is a base class that makes it easier to consume events
// from realtime or file sessions. Concrete consumers need to sublass
// a specialization of this class and override the ProcessEvent and/or
// the ProcessBuffer methods to implement the event consumption logic.
// Usage might look like:
// class MyConsumer: public EtwTraceConsumerBase<MyConsumer, 1> {
//  protected:
//    static VOID WINAPI ProcessEvent(PEVENT_TRACE event);
// };
//
// MyConsumer consumer;
// consumer.OpenFileSession(file_path);
// consumer.Consume();
template <class ImplClass>
class EtwTraceConsumerBase {
 public:
  // Constructs a closed consumer.
  EtwTraceConsumerBase() {
  }

  ~EtwTraceConsumerBase() {
    Close();
  }

  // Opens the named realtime session, which must be existent.
  // Note: You can use OpenRealtimeSession or OpenFileSession
  //    to open as many as MAXIMUM_WAIT_OBJECTS (63) sessions at
  //    any one time, though only one of them may be a realtime
  //    session.
  HRESULT OpenRealtimeSession(const wchar_t* session_name);

  // Opens the event trace log in "file_name", which must be a full or
  // relative path to an existing event trace log file.
  // Note: You can use OpenRealtimeSession or OpenFileSession
  //    to open as many as kNumSessions at any one time.
  HRESULT OpenFileSession(const wchar_t* file_name);

  // Consume all open sessions from beginning to end.
  HRESULT Consume();

  // Close all open sessions.
  HRESULT Close();

 protected:
  // Override in subclasses to handle events.
  static void ProcessEvent(EVENT_TRACE* event) {
  }
  // Override in subclasses to handle buffers.
  static bool ProcessBuffer(EVENT_TRACE_LOGFILE* buffer) {
    buffer;
    return true;  // keep going
  }

 protected:
  // Currently open sessions.
  std::vector<TRACEHANDLE> trace_handles_;

 private:
  // These delegate to ImplClass callbacks with saner signatures.
  static void WINAPI ProcessEventCallback(EVENT_TRACE* event) {
    ImplClass::ProcessEvent(event);
  }
  static ULONG WINAPI ProcessBufferCallback(PEVENT_TRACE_LOGFILE buffer) {
    return ImplClass::ProcessBuffer(buffer);
  }

  DISALLOW_COPY_AND_ASSIGN(EtwTraceConsumerBase);
};

template <class ImplClass> inline
HRESULT EtwTraceConsumerBase<ImplClass>::OpenRealtimeSession(
    const wchar_t* session_name) {
  EVENT_TRACE_LOGFILE logfile = {};
  logfile.LoggerName = const_cast<wchar_t*>(session_name);
  logfile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  logfile.BufferCallback = &ProcessBufferCallback;
  logfile.EventCallback = &ProcessEventCallback;
  logfile.Context = this;
  TRACEHANDLE trace_handle = ::OpenTrace(&logfile);
  if (reinterpret_cast<TRACEHANDLE>(INVALID_HANDLE_VALUE) == trace_handle)
    return HRESULT_FROM_WIN32(::GetLastError());

  trace_handles_.push_back(trace_handle);
  return S_OK;
}

template <class ImplClass> inline
HRESULT EtwTraceConsumerBase<ImplClass>::OpenFileSession(
    const wchar_t* file_name) {
  EVENT_TRACE_LOGFILE logfile = {};
  logfile.LogFileName = const_cast<wchar_t*>(file_name);
  logfile.BufferCallback = &ProcessBufferCallback;
  logfile.EventCallback = &ProcessEventCallback;
  logfile.Context = this;
  TRACEHANDLE trace_handle = ::OpenTrace(&logfile);
  if (reinterpret_cast<TRACEHANDLE>(INVALID_HANDLE_VALUE) == trace_handle)
    return HRESULT_FROM_WIN32(::GetLastError());

  trace_handles_.push_back(trace_handle);
  return S_OK;
}

template <class ImplClass> inline
HRESULT EtwTraceConsumerBase<ImplClass>::Consume() {
  ULONG err = ::ProcessTrace(&trace_handles_[0],
                             trace_handles_.size(),
                             NULL,
                             NULL);
  return HRESULT_FROM_WIN32(err);
}

template <class ImplClass> inline
HRESULT EtwTraceConsumerBase<ImplClass>::Close() {
  HRESULT hr = S_OK;
  for (size_t i = 0; i < trace_handles_.size(); ++i) {
    if (NULL != trace_handles_[i]) {
      ULONG ret = ::CloseTrace(trace_handles_[i]);
      trace_handles_[i] = NULL;

      if (FAILED(HRESULT_FROM_WIN32(ret)))
        hr = HRESULT_FROM_WIN32(ret);
    }

    trace_handles_.clear();
  }

  return hr;
}

}  // namespace omaha

#endif  // BASE_EVENT_TRACE_CONSUMER_H_
