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
// ETW log writer implementation.

#include "omaha/base/etw_log_writer.h"

namespace omaha {

// {9B18BFF9-915E-4cc1-9C3E-F4AC112CB36C}
const GUID EtwLogWriter::kOmahaTraceGuid =
    { 0x9b18bff9, 0x915e, 0x4cc1,
        { 0x9c, 0x3e, 0xf4, 0xac, 0x11, 0x2c, 0xb3, 0x6c } };

const GUID EtwLogWriter::kLogEventId =
    { 0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

EtwLogWriter::EtwLogWriter(const GUID& provider_guid)
    : EtwTraceProvider(provider_guid),
      rtl_capture_stack_backtrace_(NULL) {
  HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
  if (kernel32 != NULL) {
    rtl_capture_stack_backtrace_ =
        reinterpret_cast<RtlCaptureStackBackTraceFunc>(
            ::GetProcAddress(kernel32, "RtlCaptureStackBackTrace"));
  }

  EtwTraceProvider::Register();
}

void EtwLogWriter::Cleanup() {
  EtwTraceProvider::Unregister();
}

EtwLogWriter* EtwLogWriter::Create() {
  return new EtwLogWriter(kOmahaTraceGuid);
}

bool EtwLogWriter::WantsToLogRegardless() const {
  return session_handle() != 0;
}

bool EtwLogWriter::IsCatLevelEnabled(LogCategory category,
                                     LogLevel level) const {
  if ((enable_flags() & CategoryToEnableFlag(category)) == 0 ||
      enable_level() < LogLevelToTraceLevel(level))
    return false;

  return true;
}

void EtwLogWriter::OutputMessage(const OutputInfo* output_info) {
  if (!IsCatLevelEnabled(output_info->category, output_info->level))
    return;

  CStringA msg1(output_info->msg1);
  CStringA msg2(output_info->msg2);
  EtwEventLevel level = LogLevelToTraceLevel(output_info->level);
  if (enable_flags() & kCaptureStackTraceMask) {
    void* back_trace[32] = {0};
    DWORD depth = 0;

    if (rtl_capture_stack_backtrace_) {
      depth = rtl_capture_stack_backtrace_(0,
                                           arraysize(back_trace),
                                           back_trace,
                                           NULL);
    }

    EtwMofEvent<4> mof_event(kLogEventId, kLogMessageWithStackTraceType, level);
    mof_event.SetField(0, sizeof(depth), &depth);
    mof_event.SetField(1, depth * sizeof(back_trace[0]), &back_trace);
    mof_event.SetField(2, msg1.GetLength(), msg1.GetString());
    mof_event.SetField(3, msg2.GetLength() + 1, msg2.GetString());
    Log(mof_event.get());
  } else {
    EtwMofEvent<2> mof_event(kLogEventId, kLogMessageType, level);
    mof_event.SetField(0, msg1.GetLength(), msg1.GetString());
    mof_event.SetField(1, msg2.GetLength() + 1, msg2.GetString());
    Log(mof_event.get());
  }
}

void EtwLogWriter::OnEventsEnabled() {
  CORE_LOG(L2, (_T("ETW logging enabled")));
}

void EtwLogWriter::OnEventsDisabled() {
  CORE_LOG(L2, (_T("ETW logging disabled")));
}

EtwEventFlags EtwLogWriter::CategoryToEnableFlag(LogCategory category) {
  // Bit zero is reserved for the capture stack trace enable flag.
  return 1 << (category + 1);
}

EtwEventLevel EtwLogWriter::LogLevelToTraceLevel(LogLevel level) {
  switch (level) {
    case LEVEL_FATALERROR:
      return TRACE_LEVEL_FATAL;
    case LEVEL_ERROR:
      return TRACE_LEVEL_ERROR;
    case LEVEL_WARNING:
      return TRACE_LEVEL_WARNING;
    case L1:
    case L2:
      return TRACE_LEVEL_INFORMATION;
    case L3:
    case L4:
    case L5:
    case L6:
      return TRACE_LEVEL_VERBOSE;

    case LEVEL_ALL:
    default:
      return TRACE_LEVEL_NONE;
  }

  // NOTREACHED
}

}  // namespace omaha
