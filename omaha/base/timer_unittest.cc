// Copyright 2003-2009 Google Inc.
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
// Timer unittest

#include <cmath>
#include "omaha/base/time.h"
#include "omaha/base/timer.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// The accuracy of the unit test measurements is expected to be within 50 ms.
// The error varies depending on how the unit test process gets scheduled.
// The timer test is prone to failing when run by Pulse. Consider diabling the
// test completely.
const int kErrorMs = 50;

// The tests that use the Timer class are flaky (see warning in timer.h),
// and Timer isn't used in production Omaha code, so we leave out everything
// but the LowResTimer test.
// TODO(omaha): Is there a better way to do this? Maybe not run on build system?
#if 0

class TimerTest : public testing::Test {
 protected:
  // Set up a test so that we can measure the same time interval using the
  // low and high resolution timers. If the difference between them is too
  // big then we consider that the high resolution time is busted and we
  // stop running the unit tests.
  // The high resolution timer may have undefined behavior, see the header
  // file for more comments.
  static void SetUpTestCase() {
    const int kSleepMs = 100;
    const int kDiffMs = 1;
    LowResTimer t(false);
    Timer u(false);
    t.Start();
    u.Start();
    ::Sleep(kSleepMs);
    busted_ = abs(t.GetMilliseconds()- u.GetMilliseconds()) >= kErrorMs;
  }

  void PrintError() {
    // This is going to print "Test Foo is busted but passed."
    printf("is busted but ");
  }

  static bool busted_;
};

bool TimerTest::busted_ = false;

#endif  // #if 0

TEST(TimerTest, LowResTimer) {
  // This test was flaky on the build machine.
  // TODO(omaha): Is this still the case? Can we improve the test?
  if (omaha::IsBuildSystem()) {
    return;
  }

  LowResTimer t(false);

  const int kSleep1 = 100;
  t.Start();
  ::Sleep(kSleep1);
  uint32 elapsedMs = t.Stop();

  // For the first run of the timer the elapsed value must be equal to
  // the timer interval.
  EXPECT_EQ(elapsedMs, t.GetMilliseconds());

  // About 100 ms now.
  EXPECT_NEAR(kSleep1, elapsedMs, kErrorMs);

  // Test the accessors of different time units.
  EXPECT_DOUBLE_EQ(t.GetSeconds() * 1000, t.GetMilliseconds());

  const int kSleep2 = 10;
  t.Start();
  ::Sleep(kSleep2);
  elapsedMs = t.Stop();
  EXPECT_NEAR(kSleep2, elapsedMs, kErrorMs);

  // About 110 ms now.
  EXPECT_NEAR(kSleep1 + kSleep2, t.GetMilliseconds(), 2 * kErrorMs);

  const int kSleep3 = 50;
  t.Start();
  ::Sleep(kSleep3);
  elapsedMs = t.Stop();
  EXPECT_NEAR(kSleep3, elapsedMs, kErrorMs);

  // About 160 ms now.
  EXPECT_NEAR(kSleep1 + kSleep2 + kSleep3, t.GetMilliseconds(), 3 * kErrorMs);

  t.Reset();
  EXPECT_EQ(0, t.GetMilliseconds());
}

// Tests disabled, see comment at top of file.
#if 0
// Test that values from RTDSC change quickly.
TEST_F(TimerTest, RTDSC) {
  uint32 last = 0;
  for (int i = 0; i < 10; ++i) {
    uint64 counter = Timer::GetRdtscCounter();
    uint32 a = *(reinterpret_cast<uint32 *>(&counter));
    ASSERT_NE(a, last);
    last = a;
  }
}

// Compare everything as ms units for uniformity.

TEST_F(TimerTest, Timer) {
  if (busted_) {
    PrintError();
    return;
  }

  Timer t(false);

  const int kSleep1 = 100;
  t.Start();
  ::Sleep(kSleep1);
  time64 elapsed = t.Stop();

  // For the first run of the timer the elapsed value must be equal to
  // the timer interval.
  EXPECT_DOUBLE_EQ(t.PerfCountToNanoSeconds(elapsed) / 1000000,
                   t.GetNanoseconds() / 1000000);

  // About 100 ms now.
  EXPECT_NEAR(kSleep1, t.PerfCountToNanoSeconds(elapsed) / 1000000, kErrorMs);

  // Test the accessors of different time units.
  EXPECT_DOUBLE_EQ(t.GetSeconds() * 1000, t.GetMilliseconds());
  EXPECT_DOUBLE_EQ(t.GetMilliseconds() * 1000, t.GetMicroseconds());
  EXPECT_DOUBLE_EQ(t.GetMicroseconds() * 1000, t.GetNanoseconds());

  EXPECT_NEAR(t.Get100Nanoseconds() * 100.0, t.GetNanoseconds(), 100);

  const int kSleep2 = 10;
  t.Start();
  ::Sleep(kSleep2);
  elapsed = t.Stop();
  EXPECT_NEAR(kSleep2, t.PerfCountToNanoSeconds(elapsed) / 1000000, kErrorMs);

  // About 110 ms now.
  EXPECT_NEAR(kSleep1 + kSleep2, t.GetMilliseconds(), 2 * kErrorMs);

  const int kSleep3 = 50;
  t.Start();
  ::Sleep(kSleep3);
  elapsed = t.Stop();
  EXPECT_NEAR(kSleep3, t.PerfCountToNanoSeconds(elapsed) / 1000000, kErrorMs);

  // About 160 ms now.
  EXPECT_NEAR(kSleep1 + kSleep2 + kSleep3, t.GetMilliseconds(), 3 * kErrorMs);

  t.Reset();
  EXPECT_DOUBLE_EQ(0, t.GetMilliseconds());
}

TEST_F(TimerTest, TimerSplit) {
  if (busted_) {
    PrintError();
    return;
  }

  const int kSleep1 = 50;
  const int kSleep2 = 125;
  const int kSleep3 = 25;

  double split1(0), split2(0), split3(0);
  double elapsed1(0), elapsed2(0);

  Timer t(false);
  t.Start();
  ::Sleep(kSleep1);
  t.Split(&split1, &elapsed1);
  EXPECT_NEAR(split1, kSleep1, kErrorMs);
  EXPECT_NEAR(elapsed1, kSleep1, kErrorMs);
  EXPECT_DOUBLE_EQ(split1, elapsed1);

  ::Sleep(kSleep2);
  t.Split(&split2, &elapsed2);
  EXPECT_NEAR(split2, kSleep2, kErrorMs);
  EXPECT_DOUBLE_EQ(split1 + split2, elapsed2);

  ::Sleep(kSleep3);
  t.Split(&split3, NULL);
  t.Stop();
  EXPECT_NEAR(split3, kSleep3, kErrorMs);
  EXPECT_NEAR(split1 + split2 + split3, t.GetMilliseconds(), kErrorMs);
}

// Time QueryPerformanceCounter
TEST_F(TimerTest, QueryPerformanceCounter) {
  if (busted_) {
    PrintError();
    return;
  }

  Timer t(false);
  t.Start();

  const int kIterations = 100;
  LARGE_INTEGER count = {0};
  for (int i = 0; i < kIterations; i++) {
    ASSERT_TRUE(::QueryPerformanceCounter(&count));
  }

  t.Stop();

  // Expect the call to take anywhere up to 1000 nano seconds.
  EXPECT_NEAR(t.GetNanoseconds() / kIterations, 500, 500);
}
#endif  // #if 0

}  // namespace omaha
