// Copyright 2006-2009 Google Inc.
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
#include "base/basictypes.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Timing tests are extremely sensitive to external interference from other
// work currently being done on the machine.  scoped_priority_boost temporarily
// raises the priority of the caller's thread to reduce the chance of an
// intervening context switch.

class scoped_priority_boost {
 public:
  scoped_priority_boost() {
    orig_process_priority_ = ::GetPriorityClass(::GetCurrentProcess());
    if (0 != orig_process_priority_) {
      if (0 == ::SetPriorityClass(::GetCurrentProcess(),
                                  HIGH_PRIORITY_CLASS)) {
        orig_process_priority_ = 0;
      }
    }

    orig_thread_priority_ = ::GetThreadPriority(::GetCurrentThread());
    if (THREAD_PRIORITY_ERROR_RETURN != orig_thread_priority_) {
      if (0 == ::SetThreadPriority(::GetCurrentThread(),
                                   THREAD_PRIORITY_HIGHEST)) {
        orig_thread_priority_ = THREAD_PRIORITY_ERROR_RETURN;
      }
    }
  }

  ~scoped_priority_boost() {
    if (0 != orig_process_priority_) {
      ::SetPriorityClass(::GetCurrentProcess(), orig_process_priority_);
    }
    if (THREAD_PRIORITY_ERROR_RETURN != orig_thread_priority_) {
      ::SetPriorityClass(::GetCurrentProcess(), orig_process_priority_);
    }
  }

  bool succeeded() const {
    return (0 != orig_process_priority_) &&
            (THREAD_PRIORITY_ERROR_RETURN != orig_thread_priority_);
  }

 private:
  DWORD orig_process_priority_;
  int orig_thread_priority_;

  DISALLOW_COPY_AND_ASSIGN(scoped_priority_boost);
};

TEST(HighresTimer, MillisecondClock) {
  scoped_priority_boost spb;
  EXPECT_TRUE(spb.succeeded());

  HighresTimer timer;

  // note: this could fail if we context switch between initializing the timer
  // and here. Very unlikely however.
  EXPECT_EQ(0, timer.GetElapsedMs());
  timer.Start();
  uint64 half_ms = (HighresTimer::GetTimerFrequency() / 2000) + 1;
  // busy wait for a fraction more than half a millisecond.
  while (timer.start_ticks() + half_ms > HighresTimer::GetCurrentTicks()) {
    // Nothing
  }
  EXPECT_EQ(1, timer.GetElapsedMs());
}

TEST(HighresTimer, SecondClock) {
  scoped_priority_boost spb;
  EXPECT_TRUE(spb.succeeded());

  HighresTimer timer;

  EXPECT_EQ(0, timer.GetElapsedSec());
#ifdef OS_WINDOWS
  ::Sleep(250);
#else
  struct timespec ts1 = {0, 250000000};
  nanosleep(&ts1, 0);
#endif
  EXPECT_EQ(0, timer.GetElapsedSec());
  EXPECT_LE(230, timer.GetElapsedMs());
  EXPECT_GE(270, timer.GetElapsedMs());
#ifdef OS_WINDOWS
  ::Sleep(251);
#else
  struct timespec ts2 = {0, 251000000};
  nanosleep(&ts2, 0);
#endif
  EXPECT_EQ(1, timer.GetElapsedSec());
}

}  // namespace omaha

