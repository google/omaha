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
// A log writer that is controlled, and outputs to Event Tracing for Windows.

#ifndef OMAHA_BASE_ETW_LOG_WRITER_H_
#define OMAHA_BASE_ETW_LOG_WRITER_H_

#include "omaha/base/event_trace_provider.h"
#include "omaha/base/logging.h"

namespace omaha {

class EtwLogWriter : public LogWriter, public EtwTraceProvider {
 public:
  // The Omaha trace provider's GUID.
  static const GUID kOmahaTraceGuid;

  // The event ID for the event types below.
  static const GUID kLogEventId;

  // The event type for a simple UTF-8 zero-terminated message event.
  static const EtwEventType kLogMessageType = 10;

  // The event type for a log message with a stack trace, followed by the
  // zero-terminated UTF-8 message text.
  static const EtwEventType kLogMessageWithStackTraceType = 11;

  // The lowest-order enable flags bit turn on stack trace capture.
  // The remaining enable bits correspond to log categories, starting
  // from LC_LOGGING through LC_MAX_CAT - 1.
  static const EtwEventFlags kCaptureStackTraceMask = 0x0001;

  // LogWriter overrides.
  virtual bool WantsToLogRegardless() const;
  virtual bool IsCatLevelEnabled(LogCategory category, LogLevel level) const;
  virtual void OutputMessage(const OutputInfo* output_info);

  // Factory for new instances.
  static EtwLogWriter* Create();

  // Convert a log category to the corresponding ETW enable flag.
  static EtwEventFlags CategoryToEnableFlag(LogCategory category);

  // Convert from a log level to the corresponding ETW trace level.
  static EtwEventLevel LogLevelToTraceLevel(LogLevel level);

 protected:
  explicit EtwLogWriter(const GUID& provider_guid);
  virtual void Cleanup();

  // Override from EtwTraceProvider.
  virtual void OnEventsEnabled();
  virtual void OnEventsDisabled();

 private:
  // The CaptureStackBackTrace function is only available as of Windows XP,
  // and is only declared in SDK headers as of the Vista SDK. To uncomplicate
  // things we get at the function through GetProcAddress.
  typedef WORD (NTAPI* RtlCaptureStackBackTraceFunc)(DWORD frames_to_skip,
                                                     DWORD frames_to_capture,
                                                     PVOID* backtrace,
                                                     PDWORD backtrace_hash);

  RtlCaptureStackBackTraceFunc rtl_capture_stack_backtrace_;

  DISALLOW_COPY_AND_ASSIGN(EtwLogWriter);
};

}  // namespace omaha

#endif  // OMAHA_BASE_ETW_LOG_WRITER_H_
