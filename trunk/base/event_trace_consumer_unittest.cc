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
// Unit tests for event trace consumer_ base class.
#include "omaha/base/event_trace_consumer.h"
#include <atlbase.h>
#include <atlsync.h>
#include <list>
#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/event_trace_controller.h"
#include "omaha/base/event_trace_provider.h"
// TODO(omaha): Remove when http://b/2767208 is fixed.
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"

#include <initguid.h>  // NOLINT - has to be last

namespace omaha {

namespace {

using omaha::EtwTraceConsumerBase;
using omaha::EtwTraceController;
using omaha::EtwTraceProperties;

typedef std::list<EVENT_TRACE> EventQueue;

class TestConsumer: public EtwTraceConsumerBase<TestConsumer> {
 public:
  TestConsumer() {
    sank_event_.Create(NULL, TRUE, FALSE, NULL);

    ClearQueue();
  }

  ~TestConsumer() {
    ClearQueue();
    sank_event_.Close();
  }

  void ClearQueue() {
    EventQueue::const_iterator it(events_.begin()), end(events_.end());

    for (; it != end; ++it) {
      delete [] it->MofData;
    }

    events_.clear();
  }

  static void EnqueueEvent(EVENT_TRACE* event) {
    events_.push_back(*event);
    EVENT_TRACE& back = events_.back();

    if (NULL != event->MofData && 0 != event->MofLength) {
      back.MofData = new char[event->MofLength];
      memcpy(back.MofData, event->MofData, event->MofLength);
    }
  }

  static void ProcessEvent(EVENT_TRACE* event) {
    EnqueueEvent(event);
    sank_event_.Set();
  }

  static CEvent sank_event_;
  static EventQueue events_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConsumer);
};

CEvent TestConsumer::sank_event_;
EventQueue TestConsumer::events_;

const wchar_t* const kTestSessionName = L"TestLogSession";

void StopTestTraceSession() {
  // Shut down any potentially dangling session.
  EtwTraceProperties prop;
  EtwTraceController::Stop(kTestSessionName, &prop);
}

class EtwTraceConsumerBaseTest: public testing::Test {
 public:
  virtual void SetUp() {
    StopTestTraceSession();
  }
};

}  // namespace

TEST_F(EtwTraceConsumerBaseTest, Initialize) {
  TestConsumer consumer_;
}

TEST_F(EtwTraceConsumerBaseTest, OpenRealtimeSucceedsWhenNoSession) {
  TestConsumer consumer_;

  EXPECT_HRESULT_SUCCEEDED(consumer_.OpenRealtimeSession(kTestSessionName));
}

TEST_F(EtwTraceConsumerBaseTest, ConsumerImmediateFailureWhenNoSession) {
  TestConsumer consumer_;

  EXPECT_HRESULT_SUCCEEDED(consumer_.OpenRealtimeSession(kTestSessionName));
  EXPECT_HRESULT_FAILED(consumer_.Consume());
}

class EtwTraceConsumerRealtimeTest: public testing::Test {
 public:
  virtual void SetUp() {
    StopTestTraceSession();

    EXPECT_HRESULT_SUCCEEDED(consumer_.OpenRealtimeSession(kTestSessionName));
  }

  virtual void TearDown() {
    consumer_.Close();
  }

  DWORD ConsumerThread() {
    ::SetEvent(consumer_ready_);

    HRESULT hr = consumer_.Consume();
    return hr;
  }

  static DWORD WINAPI ConsumerThreadMainProc(void* arg) {
    return reinterpret_cast<EtwTraceConsumerRealtimeTest*>(arg)->
        ConsumerThread();
  }

  HRESULT StartConsumerThread() {
    consumer_ready_.Attach(::CreateEvent(NULL, TRUE, FALSE, NULL));
    EXPECT_TRUE(consumer_ready_ != NULL);
    consumer_thread_.Attach(::CreateThread(NULL, 0, ConsumerThreadMainProc,
        this, 0, NULL));
    if (NULL == consumer_thread_)
      return HRESULT_FROM_WIN32(::GetLastError());

    HRESULT hr = S_OK;
    HANDLE events[] = { consumer_ready_, consumer_thread_ };
    DWORD result = ::WaitForMultipleObjects(arraysize(events), events,
                                            FALSE, INFINITE);
    switch (result) {
      case WAIT_OBJECT_0:
        // The event was set, the consumer_ is ready.
        return S_OK;
      case WAIT_OBJECT_0 + 1: {
          // The thread finished. This may race with the event, so check
          // explicitly for the event here, before concluding there's trouble.
          if (WAIT_OBJECT_0 == ::WaitForSingleObject(consumer_ready_, 0))
            return S_OK;
          DWORD exit_code = 0;
          if (::GetExitCodeThread(consumer_thread_, &exit_code))
            return exit_code;
          else
            return HRESULT_FROM_WIN32(::GetLastError());
          break;
        }
      default:
        return E_UNEXPECTED;
        break;
    }

    // NOTREACHED
  }

  // Waits for consumer_ thread to exit, and returns its exit code.
  HRESULT JoinConsumerThread() {
    if (WAIT_OBJECT_0 != ::WaitForSingleObject(consumer_thread_, INFINITE))
      return HRESULT_FROM_WIN32(::GetLastError());

    DWORD exit_code = 0;
    if (::GetExitCodeThread(consumer_thread_, &exit_code))
      return exit_code;

    return HRESULT_FROM_WIN32(::GetLastError());
  }

  TestConsumer consumer_;
  CHandle consumer_ready_;
  CHandle consumer_thread_;
};

TEST_F(EtwTraceConsumerRealtimeTest, ConsumerReturnsWhenSessionClosed) {
  if (!IsBuildSystem() && !vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tTest not run due to http://b/2767208.") << std::endl;
    return;
  }

  EtwTraceController controller;

  HRESULT hr = controller.StartRealtimeSession(kTestSessionName, 100 * 1024);
  if (hr == E_ACCESSDENIED) {
    SUCCEED() << "You must be an administrator to run this test on Vista";
    return;
  }

  // Start the consumer_.
  EXPECT_HRESULT_SUCCEEDED(StartConsumerThread());

  // Wait around for the consumer_ thread a bit.
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(consumer_thread_, 50));

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));

  // The consumer_ returns success on session stop.
  EXPECT_HRESULT_SUCCEEDED(JoinConsumerThread());
}

namespace {

// {036B8F65-8DF3-46e4-ABFC-6985C43D59BA}
DEFINE_GUID(kTestProvider,
  0x36b8f65, 0x8df3, 0x46e4, 0xab, 0xfc, 0x69, 0x85, 0xc4, 0x3d, 0x59, 0xba);

// {57E47923-A549-476f-86CA-503D57F59E62}
DEFINE_GUID(kTestEventType,
  0x57e47923, 0xa549, 0x476f, 0x86, 0xca, 0x50, 0x3d, 0x57, 0xf5, 0x9e, 0x62);

}  // namespace

TEST_F(EtwTraceConsumerRealtimeTest, ConsumeEvent) {
  if (!IsBuildSystem() && !vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tTest not run due to http://b/2767208.") << std::endl;
    return;
  }

  EtwTraceController controller;
  HRESULT hr = controller.StartRealtimeSession(kTestSessionName, 100 * 1024);
  if (hr == E_ACCESSDENIED) {
    SUCCEED() << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(kTestProvider,
      TRACE_LEVEL_VERBOSE, 0xFFFFFFFF));

  EtwTraceProvider provider(kTestProvider);
  EXPECT_EQ(ERROR_SUCCESS, provider.Register());

  // Start the consumer_.
  EXPECT_HRESULT_SUCCEEDED(StartConsumerThread());

  EXPECT_EQ(0, TestConsumer::events_.size());

  EtwMofEvent<1> event(kTestEventType, 1, TRACE_LEVEL_ERROR);
  EXPECT_EQ(ERROR_SUCCESS, provider.Log(&event.header));

  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(TestConsumer::sank_event_,
                                                 INFINITE));
  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_HRESULT_SUCCEEDED(JoinConsumerThread());
  EXPECT_NE(0, TestConsumer::events_.size());
}

namespace {

// We run events through a file session to assert that
// the content comes through.
class EtwTraceConsumerDataTest: public testing::Test {
 public:
  EtwTraceConsumerDataTest() {
  }

  virtual void SetUp() {
    StopTestTraceSession();

    // Construct a temp file name.
    CString temp_dir = omaha::app_util::GetTempDir();
    EXPECT_TRUE(::GetTempFileName(temp_dir, _T("tmp"), 0,
                                  CStrBuf(temp_file_, MAX_PATH)));
  }

  virtual void TearDown() {
    EXPECT_TRUE(::DeleteFile(temp_file_));

    // Shut down any potentially dangling session.
    EtwTraceProperties prop;
    EtwTraceController::Stop(kTestSessionName, &prop);
  }

  HRESULT LogEventToTempSession(PEVENT_TRACE_HEADER header) {
    EtwTraceController controller;

    // Set up a file session.
    HRESULT hr = controller.StartFileSession(kTestSessionName, temp_file_);
    if (FAILED(hr))
      return hr;

    // Enable our provider.
    EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(kTestProvider,
        TRACE_LEVEL_VERBOSE, 0xFFFFFFFF));

    EtwTraceProvider provider(kTestProvider);
    // Then register our provider, means we get a session handle immediately.
    EXPECT_EQ(ERROR_SUCCESS, provider.Register());
    // Trace the event, it goes to the temp file.
    EXPECT_EQ(ERROR_SUCCESS, provider.Log(header));
    EXPECT_HRESULT_SUCCEEDED(controller.DisableProvider(kTestProvider));
    EXPECT_HRESULT_SUCCEEDED(provider.Unregister());
    EXPECT_HRESULT_SUCCEEDED(controller.Flush(NULL));
    EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));

    return S_OK;
  }

  HRESULT ConsumeEventFromTempSession() {
    // Now consume the event(s).
    TestConsumer consumer_;
    HRESULT hr = consumer_.OpenFileSession(temp_file_);
    if (SUCCEEDED(hr))
      hr = consumer_.Consume();
    consumer_.Close();
    // And nab the result.
    events_.swap(TestConsumer::events_);
    return hr;
  }

  HRESULT RoundTripEvent(PEVENT_TRACE_HEADER header, PEVENT_TRACE* trace) {
    ::DeleteFile(temp_file_);

    HRESULT hr = LogEventToTempSession(header);
    if (SUCCEEDED(hr))
      hr = ConsumeEventFromTempSession();

    if (FAILED(hr))
      return hr;

    // We should now have the event in the queue.
    if (events_.empty())
      return E_FAIL;

    *trace = &events_.back();
    return S_OK;
  }

  EventQueue events_;
  CString temp_file_;
};

}  // namespace


TEST_F(EtwTraceConsumerDataTest, RoundTrip) {
  EtwMofEvent<1> event(kTestEventType, 1, TRACE_LEVEL_ERROR);

  static const char kData[] = "This is but test data";
  event.fields[0].DataPtr = reinterpret_cast<ULONG_PTR>(kData);
  event.fields[0].Length = sizeof(kData);

  PEVENT_TRACE trace = NULL;
  HRESULT hr = RoundTripEvent(&event.header, &trace);
  if (hr == E_ACCESSDENIED) {
    SUCCEED() << "You must be an administrator to run this test on Vista";
    return;
  }
  ASSERT_TRUE(NULL != trace);
  EXPECT_EQ(sizeof(kData), trace->MofLength);
  EXPECT_STREQ(kData, reinterpret_cast<const char*>(trace->MofData));
}

}  // namespace omaha

