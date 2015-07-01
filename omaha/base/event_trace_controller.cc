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
// Implementation of a Windows event trace controller class.
#include "omaha/base/event_trace_controller.h"
#include "omaha/base/debug.h"

namespace omaha {

EtwTraceController::EtwTraceController() : session_(NULL) {
}

EtwTraceController::~EtwTraceController() {
  Stop(NULL);
}

HRESULT EtwTraceController::Start(const wchar_t* session_name,
    EtwTraceProperties* prop) {
  ASSERT1(NULL == session_ && session_name_.empty());
  EtwTraceProperties ignore;
  if (prop == NULL)
    prop = &ignore;

  HRESULT hr = Start(session_name, prop, &session_);
  if (SUCCEEDED(hr))
    session_name_ = session_name;

  return hr;
}

HRESULT EtwTraceController::StartFileSession(const wchar_t* session_name,
      const wchar_t* logfile_path, bool realtime) {
  ASSERT1(NULL == session_ && session_name_.empty());

  EtwTraceProperties prop;
  prop.SetLoggerFileName(logfile_path);
  EVENT_TRACE_PROPERTIES& p = *prop.get();
  p.Wnode.ClientContext = 1;  // QPC timer accuracy.
  p.LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;  // Sequential log.
  if (realtime)
    p.LogFileMode |= EVENT_TRACE_REAL_TIME_MODE;

  p.MaximumFileSize = 100;  // 100M file size.
  p.FlushTimer = 30;  // 30 seconds flush lag.
  return Start(session_name, &prop);
}

HRESULT EtwTraceController::StartRealtimeSession(const wchar_t* session_name,
    size_t buffer_size) {
  ASSERT1(NULL == session_ && session_name_.empty());
  if (buffer_size > ULONG_MAX) {
    return E_INVALIDARG;
  }
  EtwTraceProperties prop;
  EVENT_TRACE_PROPERTIES& p = *prop.get();
  p.LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_USE_PAGED_MEMORY;
  p.FlushTimer = 1;  // flush every second.
  p.BufferSize = static_cast<ULONG>(buffer_size);  // buffer_size in kilobytes.
  p.LogFileNameOffset = 0;
  return Start(session_name, &prop);
}

HRESULT EtwTraceController::EnableProvider(REFGUID provider, UCHAR level,
    ULONG flags) {
  ULONG error = ::EnableTrace(TRUE, flags, level, &provider, session_);
  return HRESULT_FROM_WIN32(error);
}

HRESULT EtwTraceController::DisableProvider(REFGUID provider) {
  ULONG error = ::EnableTrace(FALSE, 0, 0, &provider, session_);
  return HRESULT_FROM_WIN32(error);
}

HRESULT EtwTraceController::Stop(EtwTraceProperties* properties) {
  EtwTraceProperties ignore;
  if (properties == NULL)
    properties = &ignore;

  ULONG error = ::ControlTrace(session_, NULL, properties->get(),
    EVENT_TRACE_CONTROL_STOP);
  if (ERROR_SUCCESS != error)
    return HRESULT_FROM_WIN32(error);

  session_ = NULL;
  session_name_.clear();
  return S_OK;
}

HRESULT EtwTraceController::Flush(EtwTraceProperties* properties) {
  EtwTraceProperties ignore;
  if (properties == NULL)
    properties = &ignore;

  ULONG error = ::ControlTrace(session_, NULL, properties->get(),
                               EVENT_TRACE_CONTROL_FLUSH);
  if (ERROR_SUCCESS != error)
    return HRESULT_FROM_WIN32(error);

  return S_OK;
}

HRESULT EtwTraceController::Start(const wchar_t* session_name,
    EtwTraceProperties* properties, TRACEHANDLE* session_handle) {
  ASSERT1(properties != NULL);
  ULONG err = ::StartTrace(session_handle, session_name, properties->get());
  return HRESULT_FROM_WIN32(err);
}

HRESULT EtwTraceController::Query(const wchar_t* session_name,
    EtwTraceProperties* properties) {
  ASSERT1(properties != NULL);
  ULONG err = ::ControlTrace(NULL, session_name, properties->get(),
                             EVENT_TRACE_CONTROL_QUERY);
  return HRESULT_FROM_WIN32(err);
};

HRESULT EtwTraceController::Update(const wchar_t* session_name,
    EtwTraceProperties* properties) {
  ASSERT1(properties != NULL);
  ULONG err = ::ControlTrace(NULL, session_name, properties->get(),
                             EVENT_TRACE_CONTROL_UPDATE);
  return HRESULT_FROM_WIN32(err);
}

HRESULT EtwTraceController::Stop(const wchar_t* session_name,
    EtwTraceProperties* properties) {
  ASSERT1(properties != NULL);
  ULONG err = ::ControlTrace(NULL, session_name, properties->get(),
                             EVENT_TRACE_CONTROL_STOP);
  return HRESULT_FROM_WIN32(err);
}

HRESULT EtwTraceController::Flush(const wchar_t* session_name,
    EtwTraceProperties* properties) {
  ASSERT1(properties != NULL);
  ULONG err = ::ControlTrace(NULL, session_name, properties->get(),
                             EVENT_TRACE_CONTROL_FLUSH);
  return HRESULT_FROM_WIN32(err);
}

}  // namespace omaha
