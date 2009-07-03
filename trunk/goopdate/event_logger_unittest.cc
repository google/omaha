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


#include <stdio.h>
#include <stdarg.h>

#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/error.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/event_logger.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class EventLoggerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
    OverrideRegistryHives(kRegistryHiveOverrideRoot);

    // Enable logging of events.
    DWORD log_events = LOG_EVENT_LEVEL_ALL;
    EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueEventLogLevel, log_events));
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
  }

  // Reads the topmost event log record.
  HRESULT ReadLastEventLogRecord(const TCHAR* src_name,
                                 EVENTLOGRECORD* rec) {
    if (!(rec && src_name)) return E_INVALIDARG;
    HANDLE hlog = ::OpenEventLog(NULL, src_name);
    if (!hlog) {
      return HRESULTFromLastError();
    }
    HRESULT hr = E_FAIL;
    DWORD bytes_read(0), bytes_needed(0);
    DWORD read_flags = EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ;
    if (::ReadEventLog(hlog,              // Event log handle.
                       read_flags,        // Reverse chronological order.
                       0,                 // Not used.
                       rec,               // Read buffer.
                       rec->Length,       // Size of read buffer.
                       &bytes_read,       // Number of bytes read.
                       &bytes_needed)) {  // Number of bytes required.
      hr = S_OK;
    } else {
      hr = HRESULTFromLastError();
    }
    ::CloseEventLog(hlog);
    return hr;
  }
};

TEST_F(EventLoggerTest, AddEventSource) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  // Registers the "Google Update" event source for the "Application" log
  EXPECT_SUCCEEDED(EventLogger::AddEventSource(EventLogger::kSourceName,
                                               _T("path")));

  const TCHAR key_name[] = _T("HKLM\\SYSTEM\\CurrentControlSet\\Services\\")
                           _T("EventLog\\Application\\Google Update");
  EXPECT_TRUE(RegKey::HasKey(key_name));

  CString s;
  EXPECT_SUCCEEDED(RegKey::GetValue(key_name, _T("EventMessageFile"), &s));
  EXPECT_STREQ(s.GetString(), _T("path"));

  DWORD types(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(key_name, _T("TypesSupported"), &types));
  EXPECT_EQ(types,
      EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE);

  // Removes the "OmahaUnitTest" event source.
  EXPECT_SUCCEEDED(EventLogger::RemoveEventSource(EventLogger::kSourceName));
  EXPECT_FALSE(RegKey::HasKey(key_name));
  EXPECT_TRUE(RegKey::HasKey(
      _T("HKLM\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application")));
}

TEST_F(EventLoggerTest, ReportEvent) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  EXPECT_SUCCEEDED(EventLogger::AddEventSource(EventLogger::kSourceName,
                                               _T("path")));

  const TCHAR* strings[] = {_T("foo"), _T("bar")};
  byte buf[] = {0xaa, 0x55, 0};

  const int kEventId = 100;
  EXPECT_SUCCEEDED(EventLogger::ReportEvent(EventLogger::kSourceName,
                                            EVENTLOG_WARNING_TYPE,
                                            0,
                                            kEventId,
                                            arraysize(strings),
                                            strings,
                                            arraysize(buf),
                                            buf));
  // Read the record at the top to do a brief sanity check.
  const size_t kBufferSize = 1024 * 64;
  byte buffer[kBufferSize] = {0};
  EVENTLOGRECORD* rec = reinterpret_cast<EVENTLOGRECORD*>(buffer);
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(rec->EventID, kEventId);
  EXPECT_EQ(rec->EventType, EVENTLOG_WARNING_TYPE);
  EXPECT_EQ(rec->EventCategory, 0);
  EXPECT_EQ(rec->NumStrings, 2);
  const TCHAR* src = reinterpret_cast<const TCHAR*>(
      reinterpret_cast<byte*>(rec) + sizeof EVENTLOGRECORD);
  EXPECT_STREQ(src, EventLogger::kSourceName);
  const TCHAR* s2 = (LPTSTR) ((LPBYTE) rec + rec->StringOffset);
  EXPECT_SUCCEEDED(EventLogger::RemoveEventSource(EventLogger::kSourceName));
}

TEST_F(EventLoggerTest, LogEvent_LoggingDisabled) {
  // Disable logging.
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueEventLogLevel,
                                    static_cast<DWORD>(0)));

  const size_t kBufferSize = 1024 * 64;
  byte buffer[kBufferSize] = {0};
  EVENTLOGRECORD* rec = reinterpret_cast<EVENTLOGRECORD*>(buffer);
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  int record_number = rec->RecordNumber;

  // Logging is disabled, expect no event is logged.
  LogEvent(EVENTLOG_INFORMATION_TYPE, 10);
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(record_number, rec->RecordNumber);
}

TEST_F(EventLoggerTest, LogEvent) {
  const size_t kBufferSize = 1024 * 64;
  byte buffer[kBufferSize] = {0};
  EVENTLOGRECORD* rec = reinterpret_cast<EVENTLOGRECORD*>(buffer);

  LogEvent(EVENTLOG_INFORMATION_TYPE, 10);
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(10, rec->EventID);

  LogEvent(EVENTLOG_INFORMATION_TYPE, 11, _T("s1"));
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(11, rec->EventID);

  LogEvent(EVENTLOG_INFORMATION_TYPE, 12, _T("s1"), _T("s2"));
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(12, rec->EventID);
}

TEST_F(EventLoggerTest, LogEventContext) {
  const size_t kBufferSize = 1024 * 64;
  byte buffer[kBufferSize] = {0};
  EVENTLOGRECORD* rec = reinterpret_cast<EVENTLOGRECORD*>(buffer);

  LogEventContext(EVENTLOG_INFORMATION_TYPE, 20, _T("foo"));
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(20, rec->EventID);

  LogEventContext(EVENTLOG_INFORMATION_TYPE, 21, _T("s1"), _T("bar"));
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(21, rec->EventID);

  LogEventContext(EVENTLOG_INFORMATION_TYPE, 22,
                  _T("s1"), _T("s2"), _T("foobar"));
  rec->Length = kBufferSize;
  EXPECT_SUCCEEDED(ReadLastEventLogRecord(EventLogger::kSourceName, rec));
  EXPECT_EQ(22, rec->EventID);
}

}  // namespace omaha
