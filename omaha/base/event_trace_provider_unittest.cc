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
// Unit tests for event trace provider.
#include "omaha/base/event_trace_provider.h"
#include <new>
#include "omaha/testing/unit_test.h"
#include <initguid.h>  // NOLINT - has to be last

namespace omaha {

// {7F0FD37F-FA3C-4cd6-9242-DF60967A2CB2}
DEFINE_GUID(kTestProvider,
  0x7f0fd37f, 0xfa3c, 0x4cd6, 0x92, 0x42, 0xdf, 0x60, 0x96, 0x7a, 0x2c, 0xb2);

// {7F0FD37F-FA3C-4cd6-9242-DF60967A2CB2}
DEFINE_GUID(kTestEventClass,
  0x7f0fd37f, 0xfa3c, 0x4cd6, 0x92, 0x42, 0xdf, 0x60, 0x96, 0x7a, 0x2c, 0xb2);

TEST(EtwTraceProviderTest, ToleratesPreCreateInvocations) {
  // Because the trace provider is used in logging, it's important that
  // it be possible to use static provider instances without regard to
  // whether they've been constructed or destructed.
  // The interface of the class is designed to tolerate this usage.
  char buf[sizeof(EtwTraceProvider)] = {0};
  EtwTraceProvider& provider = reinterpret_cast<EtwTraceProvider&>(buf);

  EXPECT_EQ(NULL, provider.registration_handle());
  EXPECT_EQ(NULL, provider.session_handle());
  EXPECT_EQ(0, provider.enable_flags());
  EXPECT_EQ(0, provider.enable_level());

  EXPECT_FALSE(provider.ShouldLog(TRACE_LEVEL_FATAL, 0xfffffff));

  // We expect these not to crash.
  provider.Log(kTestEventClass, 0, TRACE_LEVEL_FATAL, "foo");
  provider.Log(kTestEventClass, 0, TRACE_LEVEL_FATAL, L"foo");

  EtwMofEvent<1> etw_mof_event(kTestEventClass, 0, TRACE_LEVEL_FATAL);
  DWORD data = 0;
  etw_mof_event.SetField(0, sizeof(data), &data);
  provider.Log(etw_mof_event.get());

  // Placement-new the provider into our buffer.
  new (buf) EtwTraceProvider(kTestProvider);  // NOLINT

  // Registration is now safe.
  EXPECT_EQ(ERROR_SUCCESS, provider.Register());

  // Destruct the instance, this should unregister it.
  provider.EtwTraceProvider::~EtwTraceProvider();

  // And post-destruction, all of the above should still be safe.
  EXPECT_EQ(NULL, provider.registration_handle());
  EXPECT_EQ(NULL, provider.session_handle());
  EXPECT_EQ(0, provider.enable_flags());
  EXPECT_EQ(0, provider.enable_level());

  EXPECT_FALSE(provider.ShouldLog(TRACE_LEVEL_FATAL, 0xfffffff));

  // We expect these not to crash.
  provider.Log(kTestEventClass, 0, TRACE_LEVEL_FATAL, "foo");
  provider.Log(kTestEventClass, 0, TRACE_LEVEL_FATAL, L"foo");
  provider.Log(etw_mof_event.get());
}

TEST(EtwTraceProviderTest, Initialize) {
  EtwTraceProvider provider(kTestProvider);

  EXPECT_EQ(NULL, provider.registration_handle());
  EXPECT_EQ(NULL, provider.session_handle());
  EXPECT_EQ(0, provider.enable_flags());
  EXPECT_EQ(0, provider.enable_level());
}

TEST(EtwTraceProviderTest, Register) {
  EtwTraceProvider provider(kTestProvider);

  EXPECT_EQ(ERROR_SUCCESS, provider.Register());
  EXPECT_NE(NULL, provider.registration_handle());
  EXPECT_EQ(ERROR_SUCCESS, provider.Unregister());
  EXPECT_EQ(NULL, provider.registration_handle());
}

TEST(EtwTraceProviderTest, RegisterWithNoNameFails) {
  EtwTraceProvider provider;

  EXPECT_TRUE(provider.Register() != ERROR_SUCCESS);
}

TEST(EtwTraceProviderTest, Enable) {
  EtwTraceProvider provider(kTestProvider);

  EXPECT_EQ(ERROR_SUCCESS, provider.Register());
  EXPECT_NE(NULL, provider.registration_handle());

  // No session so far.
  EXPECT_EQ(NULL, provider.session_handle());
  EXPECT_EQ(0, provider.enable_flags());
  EXPECT_EQ(0, provider.enable_level());

  EXPECT_EQ(ERROR_SUCCESS, provider.Unregister());
  EXPECT_EQ(NULL, provider.registration_handle());
}

}  // namespace omaha
