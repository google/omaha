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

#include "omaha/base/constants.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class SchedulerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SchedulerTest, ScheduledTaskReschedules) {
  int call_count = 0;

  {
    std::vector<HANDLE> event_handles(4);
    for (auto& handle : event_handles) {
      handle = ::CreateEvent(NULL, true, false, NULL);
    }

    Scheduler scheduler;
    // Increase call count after 200ms and every 300ms afterwards
    HRESULT hr =
        scheduler.Start(200, 300, [&call_count, &event_handles](auto*) {
          if (call_count < 4) {
            ::SetEvent(event_handles[call_count]);
          }
          call_count++;
        });

    ASSERT_EQ(WAIT_OBJECT_0,
              ::WaitForMultipleObjects(4, &event_handles[0], true, 1500));
  }
  // One after 200ms, then 2x 1000ms
}

TEST_F(SchedulerTest, DeleteWhenCallbackExpires) {
  int call_count = 0;
  std::vector<HANDLE> callbacks(2);
  for (auto& handle : callbacks) {
    handle = ::CreateEvent(NULL, true, false, NULL);
  }

  {
    Scheduler scheduler;
    // Timer that runs every 250 ms
    HRESULT hr = scheduler.Start(250, [&call_count, &callbacks](auto*) {
      ::SetEvent(callbacks[call_count]);
      call_count++;
    });
    // Wait for one callback, then scheduler should go out of scope
    ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(callbacks[0], 300));
  }
  // Second callback should never fire
  ASSERT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(callbacks[1], 1000));
  EXPECT_EQ(call_count, 1);
}

TEST_F(SchedulerTest, DeleteSoonBeforeCallbackExpires) {
  int call_count = 0;
  HANDLE callback_fired = ::CreateEvent(NULL, true, false, NULL);
  {
    Scheduler scheduler;
    // Task runs every 500ms
    HRESULT hr = scheduler.Start(500, [&call_count, callback_fired](auto*) {
      call_count++;
      ::SetEvent(callback_fired);
    });
    ASSERT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(callback_fired, 490));
  }
  EXPECT_EQ(call_count, 0);
}

TEST_F(SchedulerTest, DoesntUseDebugTimer) {
  int call_count = 0;
  const int kExpectedIntervalMs = 100;
  HANDLE callback_fired = ::CreateEvent(NULL, true, false, NULL);
  {
    Scheduler scheduler;
    // Task runs every 100ms
    HRESULT hr = scheduler.Start(
        kExpectedIntervalMs, [&call_count, callback_fired](auto* debug_timer) {
          ASSERT_EQ(nullptr, debug_timer);
          call_count++;
          ::SetEvent(callback_fired);
        });
    ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(callback_fired, 500));
  }
  ASSERT_EQ(call_count, 1);
}

TEST_F(SchedulerTest, UsesDebugTimer) {
  int call_count = 0;
  constexpr int kExpectedIntervalMs = 500;
  HANDLE signaled = ::CreateEvent(NULL, true, false, NULL);
  {
    Scheduler scheduler;
    // Task runs every 500ms
    HRESULT hr = scheduler.Start(
        kExpectedIntervalMs,
        [&call_count, kExpectedIntervalMs, signaled](auto* debug_timer) {
          EXPECT_GE(debug_timer->GetElapsedMs(), kExpectedIntervalMs);
          call_count++;
          ::SetEvent(signaled);
        },
        true);
    ASSERT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(signaled, kExpectedIntervalMs + 100));
  }
  ASSERT_EQ(call_count, 1);
}

TEST_F(SchedulerTest, LongCallbackBlocks) {
  auto scheduler = std::make_unique<Scheduler>();
  constexpr int kCallbackDelay = 500;
  HighresTimer timer;
  HANDLE callback_start = ::CreateEvent(NULL, true, false, NULL);
  HANDLE callback_end = ::CreateEvent(NULL, true, false, NULL);

  // Run after 50ms, callback will take 500ms to execute
  HRESULT hr = scheduler->Start(
      50, [kCallbackDelay, callback_start, callback_end](auto*) {
        ::SetEvent(callback_start);
        Sleep(kCallbackDelay);
        ::SetEvent(callback_end);
      });

  // Try deleting, record how much time it takes
  ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(callback_start, 100));
  timer.Start();
  scheduler.reset();
  ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(callback_end, 600));
  EXPECT_GE(timer.GetElapsedMs(), kCallbackDelay);
}

}  // namespace omaha