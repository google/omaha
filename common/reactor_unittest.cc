// Copyright 2008-2009 Google Inc.
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


#include <stdlib.h>
#include "base/scoped_ptr.h"
#include "omaha/common/event_handler.h"
#include "omaha/common/reactor.h"
#include "omaha/common/scoped_any.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// TODO(omaha): rename EventHandler to EventHandlerInterface.

// Creates and registers two waitable timers with the reactor. They go off
// randomly until the reactor stops handling events.
class ReactorTest
    : public testing::Test,
      public EventHandler {
 protected:
  ReactorTest() : cnt_(0) {}

  virtual void SetUp() {
    // Timer handles are with auto reset for simplicity.
    reset(timer1_, ::CreateWaitableTimer(NULL, false, NULL));
    reset(timer2_, ::CreateWaitableTimer(NULL, false, NULL));

    reset(event_done_, ::CreateEvent(NULL, true, false, NULL));

    ASSERT_TRUE(timer1_);
    ASSERT_TRUE(timer2_);
    ASSERT_TRUE(event_done_);

    // We only need the thread handle to queue an empty APC to it.
    reset(main_thread_, ::OpenThread(THREAD_ALL_ACCESS,
                                     false,
                                     ::GetCurrentThreadId()));
    ASSERT_TRUE(main_thread_);
  }

  virtual void TearDown() {
  }

  // EventHandler.
  virtual void HandleEvent(HANDLE h);

  // Empty APC to stop the reactor.
  static void _stdcall Stop(ULONG_PTR) {}

  // Returns an integer value in the [0, 10) range.
  static int GetSmallInt() {
    unsigned int val = 0;
    rand_s(&val);
    return val % 10;
  }

  Reactor reactor_;

  scoped_timer timer1_;
  scoped_timer timer2_;
  scoped_event event_done_;

  scoped_handle main_thread_;
  LONG cnt_;
  static const LONG ReactorTest::kMaxCount = 10;
};

const LONG ReactorTest::kMaxCount;

void ReactorTest::HandleEvent(HANDLE h) {
  EXPECT_TRUE(h);
  if (h == get(event_done_)) {
    ASSERT_TRUE(::QueueUserAPC(&ReactorTest::Stop,
                               get(main_thread_),
                               0));
  } else if (h == get(timer1_) || h == get(timer2_)) {
    // Check the handles auto reset correctly.
    EXPECT_EQ(::WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    if (::InterlockedIncrement(&cnt_) > kMaxCount) {
      ASSERT_TRUE(::SetEvent(get(event_done_)));
    } else {
      ASSERT_HRESULT_SUCCEEDED(reactor_.RegisterHandle(h));

      unsigned int val = 0;
      ASSERT_EQ(rand_s(&val), 0);
      val %= 10;

      // Set the timer to fire; negative values indicate relative time.
      LARGE_INTEGER due_time_100ns = {0};
      due_time_100ns.QuadPart = -(static_cast<int>(val) * 10 * 1000);
      ASSERT_TRUE(::SetWaitableTimer(h, &due_time_100ns, 0, NULL, NULL, false));
    }
  }
}

// Registers the handles, primes the timers, and handles events.
TEST_F(ReactorTest, HandleEvents) {
  ASSERT_HRESULT_SUCCEEDED(reactor_.RegisterHandle(get(event_done_), this, 0));
  ASSERT_HRESULT_SUCCEEDED(reactor_.RegisterHandle(get(timer1_), this, 0));
  ASSERT_HRESULT_SUCCEEDED(reactor_.RegisterHandle(get(timer2_), this, 0));

  LARGE_INTEGER due_time_100ns = {0};
  ASSERT_TRUE(SetWaitableTimer(get(timer1_),
                               &due_time_100ns,
                               0,
                               NULL,
                               NULL,
                               false));
  ASSERT_TRUE(SetWaitableTimer(get(timer2_),
                               &due_time_100ns,
                               0,
                               NULL,
                               NULL,
                               false));

  ASSERT_HRESULT_SUCCEEDED(reactor_.HandleEvents());

  ASSERT_HRESULT_SUCCEEDED(reactor_.UnregisterHandle(get(timer2_)));
  ASSERT_HRESULT_SUCCEEDED(reactor_.UnregisterHandle(get(timer1_)));
  ASSERT_HRESULT_SUCCEEDED(reactor_.UnregisterHandle(get(event_done_)));
}

}  // namespace omaha
