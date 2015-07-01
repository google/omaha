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
// Unit tests for event trace controller.
#include "omaha/base/event_trace_controller.h"
#include <atlsync.h>
#include "omaha/base/app_util.h"
#include "omaha/base/event_trace_provider.h"
#include "omaha/testing/unit_test.h"
#include <initguid.h>  // NOLINT - must be last.

namespace {

using omaha::EtwTraceController;
using omaha::EtwTraceProvider;
using omaha::EtwTraceProperties;

const wchar_t kTestSessionName[] = L"TestLogSession";

// {0D236A42-CD18-4e3d-9975-DCEEA2106E05}
DEFINE_GUID(kTestProvider,
    0xd236a42, 0xcd18, 0x4e3d, 0x99, 0x75, 0xdc, 0xee, 0xa2, 0x10, 0x6e, 0x5);

DEFINE_GUID(kGuidNull,
    0x0000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0);

const ULONG kTestProviderFlags = 0xCAFEBABE;

class TestingProvider: public EtwTraceProvider {
 public:
  explicit TestingProvider(const GUID& provider_name)
      : EtwTraceProvider(provider_name) {
    callback_event_.Create(NULL, TRUE, FALSE, NULL);
  }

  void WaitForCallback() {
    ::WaitForSingleObject(callback_event_, INFINITE);
    callback_event_.Reset();
  }

 private:
  virtual void OnEventsEnabled() {
    callback_event_.Set();
  }
  virtual void OnEventsDisabled() {
    callback_event_.Set();
  }

  CEvent callback_event_;

  DISALLOW_COPY_AND_ASSIGN(TestingProvider);
};

// These fixtures make sure we clean up dangling trace sessions
// prior to all tests, to make the tests stable against crashes
// and failures.
class EtwTracePropertiesTest : public testing::Test {
 public:
  virtual void SetUp() {
    CloseTestTraceSession();
  }

  virtual void TearDown() {
    CloseTestTraceSession();
  }

 private:
  void CloseTestTraceSession() {
    // Clean up potential leftover sessions from previous unsuccessful runs.
    EtwTraceProperties prop;
    EtwTraceController::Stop(kTestSessionName, &prop);
  }
};

class EtwTraceControllerTest : public EtwTracePropertiesTest {
};

}  // namespace

namespace omaha {

TEST_F(EtwTracePropertiesTest, Initialization) {
  EtwTraceProperties prop;

  EVENT_TRACE_PROPERTIES* p = prop.get();
  EXPECT_NE(0u, p->Wnode.BufferSize);
  EXPECT_EQ(0u, p->Wnode.ProviderId);
  EXPECT_EQ(0u, p->Wnode.HistoricalContext);

  EXPECT_TRUE(kGuidNull == p->Wnode.Guid);
  EXPECT_EQ(0, p->Wnode.ClientContext);
  EXPECT_EQ(WNODE_FLAG_TRACED_GUID, p->Wnode.Flags);

  EXPECT_EQ(0, p->BufferSize);
  EXPECT_EQ(0, p->MinimumBuffers);
  EXPECT_EQ(0, p->MaximumBuffers);
  EXPECT_EQ(0, p->MaximumFileSize);
  EXPECT_EQ(0, p->LogFileMode);
  EXPECT_EQ(0, p->FlushTimer);
  EXPECT_EQ(0, p->EnableFlags);
  EXPECT_EQ(0, p->AgeLimit);

  EXPECT_EQ(0, p->NumberOfBuffers);
  EXPECT_EQ(0, p->FreeBuffers);
  EXPECT_EQ(0, p->EventsLost);
  EXPECT_EQ(0, p->BuffersWritten);
  EXPECT_EQ(0, p->LogBuffersLost);
  EXPECT_EQ(0, p->RealTimeBuffersLost);
  EXPECT_EQ(0, p->LoggerThreadId);
  EXPECT_NE(0u, p->LogFileNameOffset);
  EXPECT_NE(0u, p->LoggerNameOffset);
}

TEST_F(EtwTracePropertiesTest, Strings) {
  EtwTraceProperties prop;

  EXPECT_STREQ(L"", prop.GetLoggerFileName());
  EXPECT_STREQ(L"", prop.GetLoggerName());

  std::wstring name(1023, L'A');
  EXPECT_HRESULT_SUCCEEDED(prop.SetLoggerFileName(name.c_str()));
  EXPECT_HRESULT_SUCCEEDED(prop.SetLoggerName(name.c_str()));
  EXPECT_STREQ(name.c_str(), prop.GetLoggerFileName());
  EXPECT_STREQ(name.c_str(), prop.GetLoggerName());

  std::wstring name2(1024, L'A');
  EXPECT_HRESULT_FAILED(prop.SetLoggerFileName(name2.c_str()));
  EXPECT_HRESULT_FAILED(prop.SetLoggerName(name2.c_str()));
}

TEST_F(EtwTraceControllerTest, Initialize) {
  EtwTraceController controller;

  EXPECT_EQ(NULL, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}

TEST_F(EtwTraceControllerTest, StartRealTimeSession) {
  EtwTraceController controller;

  HRESULT hr = controller.StartRealtimeSession(kTestSessionName, 100 * 1024);
  if (hr == E_ACCESSDENIED) {
    SUCCEED() << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_TRUE(NULL != controller.session());
  EXPECT_STREQ(kTestSessionName, controller.session_name());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_EQ(NULL, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}

TEST_F(EtwTraceControllerTest, StartFileSession) {
  CString temp;
  EXPECT_TRUE(::GetTempFileName(app_util::GetTempDir(), _T("tmp"), 0,
                                CStrBuf(temp, MAX_PATH)));

  EtwTraceController controller;
  HRESULT hr = controller.StartFileSession(kTestSessionName, temp);
  if (hr == E_ACCESSDENIED) {
    SUCCEED() << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_TRUE(NULL != controller.session());
  EXPECT_STREQ(kTestSessionName, controller.session_name());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_EQ(NULL, controller.session());
  EXPECT_STREQ(L"", controller.session_name());

  EXPECT_TRUE(::DeleteFile(temp));
}

TEST_F(EtwTraceControllerTest, EnableDisable) {
  TestingProvider provider(kTestProvider);

  EXPECT_EQ(ERROR_SUCCESS, provider.Register());
  EXPECT_EQ(NULL, provider.session_handle());

  EtwTraceController controller;
  HRESULT hr = controller.StartRealtimeSession(kTestSessionName, 100 * 1024);
  if (hr == E_ACCESSDENIED) {
    SUCCEED() << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(kTestProvider,
                           TRACE_LEVEL_VERBOSE, kTestProviderFlags));

  provider.WaitForCallback();

  EXPECT_EQ(TRACE_LEVEL_VERBOSE, provider.enable_level());
  EXPECT_EQ(kTestProviderFlags, provider.enable_flags());

  EXPECT_HRESULT_SUCCEEDED(controller.DisableProvider(kTestProvider));

  provider.WaitForCallback();

  EXPECT_EQ(0, provider.enable_level());
  EXPECT_EQ(0, provider.enable_flags());

  EXPECT_EQ(ERROR_SUCCESS, provider.Unregister());

  // Enable the provider again, before registering.
  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(kTestProvider,
                           TRACE_LEVEL_VERBOSE, kTestProviderFlags));

  // Register the provider again, the settings above
  // should take immediate effect.
  EXPECT_EQ(ERROR_SUCCESS, provider.Register());

  EXPECT_EQ(TRACE_LEVEL_VERBOSE, provider.enable_level());
  EXPECT_EQ(kTestProviderFlags, provider.enable_flags());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));

  provider.WaitForCallback();

  // Session should have wound down.
  EXPECT_EQ(0, provider.enable_level());
  EXPECT_EQ(0, provider.enable_flags());
}

}  // namespace omaha
