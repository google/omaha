// Copyright 2007-2019 Google Inc.
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

#include "omaha/core/scheduler.h"

#include <functional>

#include "omaha/base/constants.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

inline void ASSERT_SIGNALLED_BEFORE(HANDLE handle, DWORD timeout_ms) {
  ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(handle, timeout_ms));
}

inline void ASSERT_ALL_SIGNALLED_BEFORE(std::vector<scoped_handle>& handles,
                                        DWORD timeout_ms) {
  // Wait for all handles
  constexpr bool kWaitAll = true;
  std::vector<HANDLE> raw_handles;
  for (auto& handle : handles) {
    raw_handles.emplace_back(get(handle));
  }
  const DWORD res = ::WaitForMultipleObjects(
      raw_handles.size(), &raw_handles[0], kWaitAll, timeout_ms);
  ASSERT_EQ(WAIT_OBJECT_0, res);
}

inline void ASSERT_TIMEOUT_AFTER(HANDLE handle, DWORD timeout_ms) {
  ASSERT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(handle, timeout_ms));
}

}  // namespace

class SchedulerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SchedulerTest, ScheduledTaskReschedules) {
  int call_count = 0;
  std::vector<scoped_handle> event_handles(4);

  for (auto& handle : event_handles) {
    reset(handle, ::CreateEvent(NULL, true, false, NULL));
  }

  Scheduler scheduler;
  // Increase call count after 200ms and every 300ms afterwards
  HRESULT hr = scheduler.Start(200, 300, [&call_count, &event_handles](auto*) {
    if (call_count < 4) {
      ::SetEvent(get(event_handles[call_count]));
    }
    call_count++;
  });
  ASSERT_SUCCEEDED(hr);
  ASSERT_ALL_SIGNALLED_BEFORE(event_handles, 1500);
  EXPECT_EQ(4, call_count);
}

TEST_F(SchedulerTest, DeleteWhenCallbackExpires) {
  int call_count = 0;
  std::vector<scoped_handle> callbacks(2);
  for (auto& handle : callbacks) {
    reset(handle, ::CreateEvent(NULL, true, false, NULL));
  }

  // Create the scheduler in a new scope
  {
    Scheduler scheduler;
    // Timer that runs every 250 ms
    HRESULT hr = scheduler.Start(250, [&call_count, &callbacks](auto*) {
      ::SetEvent(get(callbacks[call_count]));
      call_count++;
    });
    // Wait for one callback, then scheduler should go out of scope
    ASSERT_SIGNALLED_BEFORE(get(callbacks[0]), 300);
  }
  // Second callback should never fire
  ASSERT_TIMEOUT_AFTER(get(callbacks[1]), 1000);
  EXPECT_EQ(call_count, 1);
}

TEST_F(SchedulerTest, DeleteSoonBeforeCallbackExpires) {
  int call_count = 0;
  constexpr int kInterval = 500;
  constexpr int kTimeout = kInterval - 10;
  scoped_handle callback_fired(::CreateEvent(NULL, true, false, NULL));
  {
    Scheduler scheduler;
    // Task runs every 500ms
    HRESULT hr =
        scheduler.Start(kInterval, [&call_count, &callback_fired](auto*) {
          call_count++;
          ::SetEvent(get(callback_fired));
        });
    ASSERT_SUCCEEDED(hr);
    ASSERT_TIMEOUT_AFTER(get(callback_fired), kTimeout);
  }
  EXPECT_EQ(call_count, 0);
}

TEST_F(SchedulerTest, DoesntUseDebugTimer) {
  int call_count = 0;
  constexpr int kExpectedIntervalMs = 100;
  constexpr int kTimeout = 500;
  scoped_handle callback_fired(::CreateEvent(NULL, true, false, NULL));
  {
    Scheduler scheduler;
    // Task runs every 100ms
    HRESULT hr = scheduler.Start(
        kExpectedIntervalMs, [&call_count, &callback_fired](auto* debug_timer) {
          ASSERT_EQ(nullptr, debug_timer);
          call_count++;
          ::SetEvent(get(callback_fired));
        });
    ASSERT_SUCCEEDED(hr);
    ASSERT_SIGNALLED_BEFORE(get(callback_fired), kTimeout);
  }
  ASSERT_EQ(call_count, 1);
}

TEST_F(SchedulerTest, UsesDebugTimer) {
  int call_count = 0;
  constexpr int kExpectedIntervalMs = 500;
  scoped_handle callback_handle(::CreateEvent(NULL, true, false, NULL));
  {
    Scheduler scheduler;
    // Task runs every 500ms
    HRESULT hr = scheduler.Start(
        kExpectedIntervalMs,
        [&call_count, kExpectedIntervalMs, &callback_handle](auto* debug_timer) {
          ASSERT_TRUE(debug_timer != nullptr);
          EXPECT_GE(debug_timer->GetElapsedMs(), kExpectedIntervalMs);
          call_count++;
          ::SetEvent(get(callback_handle));
        },
        true /* has_debug_timer */);
    ASSERT_SUCCEEDED(hr);
    ASSERT_SIGNALLED_BEFORE(get(callback_handle), kExpectedIntervalMs + 100);
  }
  ASSERT_EQ(call_count, 1);
}

TEST_F(SchedulerTest, LongCallbackBlocks) {
  auto scheduler = std::make_unique<Scheduler>();
  constexpr int kInterval = 50;
  constexpr int kCallbackDelay = 500;
  HighresTimer timer;
  scoped_handle callback_start(::CreateEvent(NULL, true, false, NULL));
  scoped_handle callback_end(::CreateEvent(NULL, true, false, NULL));

  // Run after 50ms, callback will take 500ms to execute
  HRESULT hr = scheduler->Start(
      kInterval, [kCallbackDelay, &callback_start, &callback_end](auto*) {
        ::SetEvent(get(callback_start));
        Sleep(kCallbackDelay);
        ::SetEvent(get(callback_end));
      });
  ASSERT_SUCCEEDED(hr);
  // Try deleting, record how much time it takes
  ASSERT_SIGNALLED_BEFORE(get(callback_start), 100);
  timer.Start();
  scheduler.reset();
  ASSERT_SIGNALLED_BEFORE(get(callback_end), 600);
  EXPECT_GE(timer.GetElapsedMs(), kCallbackDelay);
}

}  // namespace omaha