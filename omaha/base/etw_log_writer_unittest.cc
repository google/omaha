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
// ETW log writer unittests.

#include <atlbase.h>
#include <atlstr.h>
#include <atlsync.h>
#include "omaha/base/etw_log_writer.h"
#include "omaha/base/event_trace_consumer.h"
#include "omaha/base/event_trace_controller.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"


namespace {

using omaha::EtwEventType;
using omaha::EtwLogWriter;
using omaha::EtwTraceConsumerBase;
using testing::StrEq;

class TestConsumer : public EtwTraceConsumerBase<TestConsumer> {
 public:
  TestConsumer() {
    EXPECT_EQ(NULL, s_current_);
    s_current_ = this;
  }

  ~TestConsumer() {
    EXPECT_TRUE(this == s_current_);
    s_current_ = NULL;
  }

  MOCK_METHOD1(OnLogMessage, void(const char* msg));

  void ProcessLogEvent(EtwEventType type, const void* data, size_t data_len) {
    data_len;  // Unused
    if (type == EtwLogWriter::kLogMessageType) {
      OnLogMessage(reinterpret_cast<const char*>(data));
    } else if (type == EtwLogWriter::kLogMessageWithStackTraceType) {
      const DWORD* depth = reinterpret_cast<const DWORD*>(data);
      void* const* stack_trace = reinterpret_cast<void* const*>(depth + 1);
      OnLogMessage(reinterpret_cast<const char*>(stack_trace + *depth));
    } else {
      FAIL() << "Unexpected message type " << type;
    }
  }

  static void ProcessEvent(EVENT_TRACE* event) {
    if (event->Header.Guid == EtwLogWriter::kLogEventId) {
      s_current_->ProcessLogEvent(event->Header.Class.Type,
                                  event->MofData,
                                  event->MofLength);
    }
  }

 private:
  static TestConsumer* s_current_;
};

TestConsumer* TestConsumer::s_current_ = NULL;

const wchar_t kTestSessionName[] = L"EtwLogWriterTest Session";
// {AD914B7A-0C5F-426e-895C-58B125408125}
const GUID kTestProviderGuid = { 0xad914b7a, 0xc5f, 0x426e,
    { 0x89, 0x5c, 0x58, 0xb1, 0x25, 0x40, 0x81, 0x25 } };

// Subclass to allow using a distinct provider GUID and overriding
// event handlers.
class TestingLogWriter: public EtwLogWriter {
 public:
  TestingLogWriter() : EtwLogWriter(kTestProviderGuid) {
    EXPECT_TRUE(events_enabled_.Create(NULL, TRUE, FALSE, NULL));
    EXPECT_TRUE(events_disabled_.Create(NULL, TRUE, FALSE, NULL));
  }

  void WaitUntilEnabled() {
    // Wait for a the callback to hit, then reset the event.
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(events_enabled_, INFINITE));
    EXPECT_TRUE(::ResetEvent(events_enabled_));
  }

  void WaitUntilDisabled() {
    // Wait for a the callback to hit, then reset the event.
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(events_disabled_, INFINITE));
    EXPECT_TRUE(::ResetEvent(events_disabled_));
  }

  ~TestingLogWriter() {
    // We explicitly call Cleanup() here so that the ETW provider will
    // be torn down before the CEvent destructors are called.  We had
    // issues with the test failing spuriously because the provider
    // teardown is done on a separate thread, and would attempt to call
    // OnEventsDisabled() after the event handles had been released.
    // (http://b/2873205)

    Cleanup();
  }

 protected:
  // Override from EtwTraceProvider.
  virtual void OnEventsEnabled() {
    EtwLogWriter::OnEventsEnabled();
    events_enabled_.Set();
  }

  virtual void OnEventsDisabled() {
    EtwLogWriter::OnEventsDisabled();
    events_disabled_.Set();
  }

  CEvent events_enabled_;
  CEvent events_disabled_;
};

}  // namespace

namespace omaha {

class EtwLogWriterTest: public testing::Test {
 public:
  EtwLogWriterTest() : counter_(0) {
  }

  virtual void SetUp() {
    // Kill any dangling trace session.
    EtwTraceProperties prop;
    EtwTraceController::Stop(kTestSessionName, &prop);

    // And create a session on a new temp file.
    temp_file_ = GetTempFilename(_T("tmp"));
    ASSERT_FALSE(temp_file_.IsEmpty());

    EXPECT_HRESULT_SUCCEEDED(
        controller_.StartFileSession(kTestSessionName, temp_file_, false));
  }

  virtual void TearDown() {
    if (controller_.session() != NULL) {
      EXPECT_HRESULT_SUCCEEDED(controller_.Stop(NULL));
      EXPECT_TRUE(::DeleteFile(temp_file_));
    }
  }

  // Asserts that enabling ETW logging at trace_level causes
  // logging at log_level to be enabled.
  void ExpectLogLevelEnabled(EtwEventLevel trace_level, LogLevel log_level) {
    TestingLogWriter writer;
    EXPECT_HRESULT_SUCCEEDED(
        controller_.EnableProvider(kTestProviderGuid,
                                   trace_level,
                                   0xFFFFFFFF)) <<
    _T("[trace level: ") << static_cast<uint16>(trace_level) <<
    _T("][log level: ") << log_level << _T("]");
    writer.WaitUntilEnabled();
    EXPECT_EQ(true, writer.IsCatLevelEnabled(LC_LOGGING, log_level));
  }

  // Asserts that enabling ETW logging at trace_level causes
  // logging at log_level to be disabled.
  void ExpectLogLevelDisabled(EtwEventLevel trace_level, LogLevel log_level) {
    TestingLogWriter writer;
    EXPECT_HRESULT_SUCCEEDED(
        controller_.EnableProvider(kTestProviderGuid,
                                   trace_level,
                                   0xFFFFFFFF)) <<
    _T("[trace level: ") << static_cast<uint16>(trace_level) <<
    _T("][log level: ") << log_level << _T("]");
    writer.WaitUntilEnabled();
    EXPECT_EQ(false, writer.IsCatLevelEnabled(LC_LOGGING, log_level));
  }

  void ExpectLogMessage(EtwEventLevel trace_level,
                        EtwEventFlags enable_bits,
                        LogLevel log_level,
                        LogCategory log_cat,
                        bool should_log) {
    TestingLogWriter writer;
    EXPECT_HRESULT_SUCCEEDED(
        controller_.EnableProvider(kTestProviderGuid,
                                   trace_level,
                                   enable_bits)) <<
    _T("[trace level: ") << static_cast<uint16>(trace_level) <<
    _T("][flags: 0x") << std::hex << enable_bits << std::dec <<
    _T("][log level: ") << log_level <<
    _T("][category: ") << log_cat <<
    _T("][should_log: ") << should_log << _T("]");
    writer.WaitUntilEnabled();

    CString message;
    message.Format(L"[%d][%d][0x%08X][%d][%d]",
                   ++counter_,  // Make each expectation call unique.
                   trace_level,
                   enable_bits,
                   log_level,
                   log_cat);
    OutputInfo info(log_cat, log_level, L"[Prefix]", message);
    writer.OutputMessage(&info);

    CStringA expected_msg("[Prefix]");
    expected_msg += message;
    // Set up an expectation for zero or one calls with this string,
    // depending whether should_log or not.
    EXPECT_CALL(consumer_, OnLogMessage(StrEq(expected_msg.GetString())))
        .Times(should_log ? 1 : 0);
  }

  void ConsumeAndRestartLog() {
    EXPECT_HRESULT_SUCCEEDED(controller_.Stop(NULL));

    EXPECT_HRESULT_SUCCEEDED(consumer_.OpenFileSession(temp_file_));
    EXPECT_HRESULT_SUCCEEDED(consumer_.Consume());
    EXPECT_HRESULT_SUCCEEDED(consumer_.Close());

    EXPECT_TRUE(::DeleteFile(temp_file_));

    // Restart the log collection.
    EXPECT_HRESULT_SUCCEEDED(
        controller_.StartFileSession(kTestSessionName, temp_file_, false));
  }

 protected:
  EtwTraceController controller_;
  TestConsumer consumer_;
  CString temp_file_;
  int counter_;
};

TEST_F(EtwLogWriterTest, ProviderNotEnabled) {
  // Verify that everything is turned off when the provider is not enabled.
  TestingLogWriter writer;

  EXPECT_FALSE(writer.WantsToLogRegardless());
  for (int cat = LC_LOGGING; cat < LC_MAX_CAT; ++cat) {
    for (int level = LEVEL_FATALERROR; level < LEVEL_ALL; ++level) {
      EXPECT_EQ(false, writer.IsCatLevelEnabled(static_cast<LogCategory>(cat),
                                                static_cast<LogLevel>(level)));
    }
  }
}

TEST_F(EtwLogWriterTest, ProviderEnableFlags) {
  // Test that various provider enable flags have the expected effect on
  // IsCatLevelEnabled.
  for (int cat = LC_LOGGING; cat < LC_MAX_CAT; ++cat) {
    EtwEventFlags flags = 1 << (cat + 1);

    TestingLogWriter writer;
    EXPECT_HRESULT_SUCCEEDED(
        controller_.EnableProvider(kTestProviderGuid,
                                   TRACE_LEVEL_INFORMATION,
                                   flags)) <<
    _T("cat[") << cat << _T("]: 0x") << std::hex << flags;
    writer.WaitUntilEnabled();

    for (int probe = LC_LOGGING; probe < LC_MAX_CAT; ++probe) {
      bool category_enabled = probe == cat;
      EXPECT_EQ(category_enabled,
                writer.IsCatLevelEnabled(static_cast<LogCategory>(probe),
                                         LEVEL_ERROR));
    }
  }
}

// On Windows XP, it appears that 32 enable/disable trace level operations
// saturates some sort of a buffer, which causes the subsequent enable/disable
// operations to fail. To work around this, the following tests are chopped up
// into unnaturally small pieces.
//
// TODO(omaha): This test is disabled because it fails on Windows 8.1. The test
// EtwLogWriterTest.ProviderLevel verifies that if log enabled at trace level X,
// all logs with level > X should be filter out and level <= X should be kept.
// In ETW trace controller, we call Win32 API EnableTrace(TRUE, ...). However,
// looks like this API behavior has changed on Win 8.1. In previous versions of
// Windows, when trace_level is TRACE_LEVEL_NONE aka 0, the level will be
// updated to 0 as ::GetTraceEnableLevel() returns 0 after that. On Win 8.1,
// EnableTrace() does not update the trace_level in this case,
// ::GetTraceEnableLevel() still returns the previous trace level.
//
// One way to fix this issue is to disable the trace when trace_level is 0 since
// this seems to be logically equivalent. We need to be careful in this case
// though: make sure ETW provider callback is called correctly.
TEST_F(EtwLogWriterTest, DISABLED_ProviderLevel) {
  // Test that various trace levels have the expected effect on
  // IsCatLevelEnabled.

  // TRACE_LEVEL_NONE
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, LEVEL_FATALERROR);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, LEVEL_ERROR);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, LE);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, LEVEL_WARNING);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, LW);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, L1);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, L2);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, L3);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, L4);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, L5);
  ExpectLogLevelDisabled(TRACE_LEVEL_NONE, L6);
  ConsumeAndRestartLog();

  if (!ShouldRunLargeTest()) {
    // This test takes about 6 seconds, so only run part of it by default.
    return;
  }

  // TRACE_LEVEL_FATAL
  ExpectLogLevelEnabled(TRACE_LEVEL_FATAL, LEVEL_FATALERROR);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, LEVEL_ERROR);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, LE);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, LEVEL_WARNING);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, LW);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, L1);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, L2);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, L3);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, L4);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, L5);
  ExpectLogLevelDisabled(TRACE_LEVEL_FATAL, L6);
  ConsumeAndRestartLog();

  // TRACE_LEVEL_ERROR
  ExpectLogLevelEnabled(TRACE_LEVEL_ERROR, LEVEL_FATALERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_ERROR, LEVEL_ERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_ERROR, LE);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, LEVEL_WARNING);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, LW);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, L1);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, L2);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, L3);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, L4);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, L5);
  ExpectLogLevelDisabled(TRACE_LEVEL_ERROR, L6);
  ConsumeAndRestartLog();

  // TRACE_LEVEL_WARNING
  ExpectLogLevelEnabled(TRACE_LEVEL_WARNING, LEVEL_FATALERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_WARNING, LEVEL_ERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_WARNING, LE);
  ExpectLogLevelEnabled(TRACE_LEVEL_WARNING, LEVEL_WARNING);
  ExpectLogLevelEnabled(TRACE_LEVEL_WARNING, LW);
  ExpectLogLevelDisabled(TRACE_LEVEL_WARNING, L1);
  ExpectLogLevelDisabled(TRACE_LEVEL_WARNING, L2);
  ExpectLogLevelDisabled(TRACE_LEVEL_WARNING, L3);
  ExpectLogLevelDisabled(TRACE_LEVEL_WARNING, L4);
  ExpectLogLevelDisabled(TRACE_LEVEL_WARNING, L5);
  ExpectLogLevelDisabled(TRACE_LEVEL_WARNING, L6);
  ConsumeAndRestartLog();

  // TRACE_LEVEL_INFORMATION
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, LEVEL_FATALERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, LEVEL_ERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, LE);
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, LEVEL_WARNING);
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, LW);
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, L1);
  ExpectLogLevelEnabled(TRACE_LEVEL_INFORMATION, L2);
  ExpectLogLevelDisabled(TRACE_LEVEL_INFORMATION, L3);
  ExpectLogLevelDisabled(TRACE_LEVEL_INFORMATION, L4);
  ExpectLogLevelDisabled(TRACE_LEVEL_INFORMATION, L5);
  ExpectLogLevelDisabled(TRACE_LEVEL_INFORMATION, L6);
  ConsumeAndRestartLog();

  // TRACE_LEVEL_VERBOSE
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, LEVEL_FATALERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, LEVEL_ERROR);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, LE);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, LEVEL_WARNING);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, LW);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, L1);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, L2);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, L3);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, L4);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, L5);
  ExpectLogLevelEnabled(TRACE_LEVEL_VERBOSE, L6);
}

TEST_F(EtwLogWriterTest, UpdateProviderLevel) {
  // Test that changing ETW trace levels and flags causes a pre-existing
  // ETWLogProvider to update its log levels per IsCatLevelEnabled.
  TestingLogWriter writer;
  for (int cat = LC_LOGGING; cat < LC_MAX_CAT; ++cat) {
    EXPECT_FALSE(writer.IsCatLevelEnabled(static_cast<LogCategory>(cat),
                                          LEVEL_WARNING));
  }

  // Turn logging on.
  EXPECT_HRESULT_SUCCEEDED(
      controller_.EnableProvider(kTestProviderGuid,
                                 TRACE_LEVEL_INFORMATION,
                                 0xFFFFFFFF));

  // Wait for the callback to hit, then assert that logging is now enabled.
  writer.WaitUntilEnabled();
  for (int cat = LC_LOGGING; cat < LC_MAX_CAT; ++cat) {
    EXPECT_TRUE(writer.IsCatLevelEnabled(static_cast<LogCategory>(cat),
                                         LEVEL_WARNING));
  }

  // Turn down the enable mask, aka categories.
  EXPECT_HRESULT_SUCCEEDED(
      controller_.EnableProvider(kTestProviderGuid,
                                 TRACE_LEVEL_INFORMATION,
                                 0x0003));  // 3 is LC_LOGGING + stack traces.

  // Wait for the callback to hit, then assert that logging is still enabled
  // but only for the LC_LOGGING category.
  writer.WaitUntilEnabled();
  for (int cat = LC_LOGGING; cat < LC_MAX_CAT; ++cat) {
    EXPECT_EQ(cat == LC_LOGGING,
              writer.IsCatLevelEnabled(static_cast<LogCategory>(cat),
                                       LEVEL_WARNING));
  }

  EXPECT_HRESULT_SUCCEEDED(controller_.DisableProvider(kTestProviderGuid));

  // Wait for the callback to hit, then assert that logging is now disabled.
  writer.WaitUntilDisabled();
  for (int cat = LC_LOGGING; cat < LC_MAX_CAT; ++cat) {
    EXPECT_FALSE(writer.IsCatLevelEnabled(static_cast<LogCategory>(cat),
                                          LEVEL_WARNING));
  }
}

TEST_F(EtwLogWriterTest, OutputMessageIsRobust) {
  // Test that OutputMessage doesn't fall over on unexpected inputs.

  // Turn logging on.
  TestingLogWriter writer;
  EXPECT_HRESULT_SUCCEEDED(
      controller_.EnableProvider(kTestProviderGuid,
                                 TRACE_LEVEL_INFORMATION,
                                 0xFFFFFFFF));
  writer.WaitUntilEnabled();

  OutputInfo msg(LC_LOGGING, LW, L"TEST", NULL);
  writer.OutputMessage(&msg);

  msg.category = static_cast<LogCategory>(0xFF);
  msg.level = static_cast<LogLevel>(0xFF);
  msg.msg1 = NULL;
  msg.msg2 = L"TEST";
  writer.OutputMessage(&msg);
}

// TODO(omaha): This test is disabled because it fails on Windows 8.1. In
// previous versions of Windows, when trace_level is TRACE_LEVEL_NONE aka 0, the
// level will be updated to 0 as ::GetTraceEnableLevel() returns 0 after that.
// On Win 8.1, EnableTrace() does not update the trace_level in this case,
// ::GetTraceEnableLevel() still returns the previous trace level.
TEST_F(EtwLogWriterTest, DISABLED_OutputMessageOnlyLevelsEnabled) {
#define EXPECT_LOG(trace_level, enable_bits, log_level, log_cat) \
  ExpectLogMessage(trace_level, enable_bits, log_level, log_cat, true);
#define EXPECT_NO_LOG(trace_level, enable_bits, log_level, log_cat) \
  ExpectLogMessage(trace_level, enable_bits, log_level, log_cat, false);

  // Try different levels at LC_LOGGING category.
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, LEVEL_FATALERROR, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, LEVEL_ERROR, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, LE, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, LEVEL_WARNING, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, LW, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, L1, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, L2, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, L3, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, L4, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, L5, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_NONE, 0x3, L6, LC_LOGGING);
  ConsumeAndRestartLog();

  if (!ShouldRunLargeTest()) {
    // This test takes about 7 seconds, so only run part of it by default.
    return;
  }

  EXPECT_LOG(TRACE_LEVEL_FATAL, 0x3, LEVEL_FATALERROR, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, LEVEL_ERROR, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, LE, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, LEVEL_WARNING, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, LW, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, L1, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, L2, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, L3, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, L4, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, L5, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_FATAL, 0x3, L6, LC_LOGGING);
  ConsumeAndRestartLog();

  EXPECT_LOG(TRACE_LEVEL_ERROR, 0x3, LEVEL_FATALERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_ERROR, 0x3, LEVEL_ERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_ERROR, 0x3, LE, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, LEVEL_WARNING, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, LW, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, L1, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, L2, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, L3, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, L4, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, L5, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_ERROR, 0x3, L6, LC_LOGGING);
  ConsumeAndRestartLog();

  EXPECT_LOG(TRACE_LEVEL_WARNING, 0x3, LEVEL_FATALERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_WARNING, 0x3, LEVEL_ERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_WARNING, 0x3, LE, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_WARNING, 0x3, LEVEL_WARNING, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_WARNING, 0x3, LW, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_WARNING, 0x3, L1, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_WARNING, 0x3, L2, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_WARNING, 0x3, L3, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_WARNING, 0x3, L4, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_WARNING, 0x3, L5, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_WARNING, 0x3, L6, LC_LOGGING);
  ConsumeAndRestartLog();

  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, LEVEL_FATALERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, LEVEL_ERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, LE, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, LEVEL_WARNING, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, LW, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, L1, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_INFORMATION, 0x3, L2, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_INFORMATION, 0x3, L3, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_INFORMATION, 0x3, L4, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_INFORMATION, 0x3, L5, LC_LOGGING);
  EXPECT_NO_LOG(TRACE_LEVEL_INFORMATION, 0x3, L6, LC_LOGGING);
  ConsumeAndRestartLog();

  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, LEVEL_FATALERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, LEVEL_ERROR, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, LE, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, LEVEL_WARNING, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, LW, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, L1, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, L2, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, L3, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, L4, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, L5, LC_LOGGING);
  EXPECT_LOG(TRACE_LEVEL_VERBOSE, 0x3, L6, LC_LOGGING);
  ConsumeAndRestartLog();
}

TEST_F(EtwLogWriterTest, OutputMessageOnlyCategoriesEnabled) {
  int max_cat = LC_MAX_CAT;
  if (!ShouldRunLargeTest()) {
    // This test takes about 12 seconds, so only run part of it by default.
    max_cat = LC_LOGGING + 2;  // Test combinations of the first two categories.
  }

  // Loop through categories.
  for (int log_cat = LC_LOGGING; log_cat < max_cat; ++log_cat) {
    for (int enable_cat = LC_LOGGING; enable_cat < max_cat; ++enable_cat) {
      EtwEventFlags enable_bits =
          EtwLogWriter::CategoryToEnableFlag(
              static_cast<LogCategory>(enable_cat));

      ExpectLogMessage(TRACE_LEVEL_VERBOSE,
                       enable_bits,
                       LEVEL_WARNING,
                       static_cast<LogCategory>(log_cat),
                       log_cat == enable_cat);
    }

    // Play the log for every log category, due to the ETW enable/disable
    // restriction on Windows XP that's mentioned above.
    ConsumeAndRestartLog();
  }
}

}  // namespace omaha
