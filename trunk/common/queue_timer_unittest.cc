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


#include <iostream>
#include "base/scoped_ptr.h"
#include "omaha/common/queue_timer.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/timer.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class QueueTimerTest : public testing::Test {
 protected:
  QueueTimerTest()
      : timer_queue_(NULL),
        cnt_(0),
        max_cnt_(0) {}

  virtual void SetUp() {
    cnt_ = 0;
    timer_queue_ = ::CreateTimerQueue();
    ASSERT_TRUE(timer_queue_);
    reset(ev_, ::CreateEvent(NULL, true, false, NULL));
  }

  virtual void TearDown() {
    // First destroy the timer, otherwise the timer could fire and
    // access invalid test case state.
    queue_timer_.reset();
    reset(ev_);
    ASSERT_TRUE(::DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE));
    cnt_ = 0;
    max_cnt_ = 0;
  }

  // Handles the alarm mode of the timer queue, where the timer fires once.
  static void AlarmCallback(QueueTimer* queue_timer);

  // Handles the periodic timer.
  static void TimerCallback(QueueTimer* queue_timer);

  HANDLE timer_queue_;
  scoped_ptr<QueueTimer> queue_timer_;
  scoped_event ev_;
  volatile int cnt_;
  volatile int max_cnt_;
};

void QueueTimerTest::AlarmCallback(QueueTimer* queue_timer) {
  ASSERT_TRUE(queue_timer);
  void* ctx = queue_timer->ctx();
  QueueTimerTest* test = static_cast<QueueTimerTest*>(ctx);
  test->cnt_ = 1;
  ASSERT_TRUE(::SetEvent(get(test->ev_)));
}

void QueueTimerTest::TimerCallback(QueueTimer* queue_timer) {
  ASSERT_TRUE(queue_timer);
  void* ctx = queue_timer->ctx();
  QueueTimerTest* test = static_cast<QueueTimerTest*>(ctx);

  // Wait max_cnt_ ticks before signaling.
  ++test->cnt_;
  if (test->cnt_ == test->max_cnt_) {
    ::SetEvent(get(test->ev_));
  }
}

TEST_F(QueueTimerTest, QuickAlarm) {
  queue_timer_.reset(new QueueTimer(timer_queue_,
                                    &QueueTimerTest::AlarmCallback,
                                    this));
  const int kWaitTimeMaxMs = 1000;

  // Set the timer to fire once right away.
  LowResTimer timer(true);
  ASSERT_HRESULT_SUCCEEDED(
      queue_timer_->Start(0, 0, WT_EXECUTEINTIMERTHREAD | WT_EXECUTEONLYONCE));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(ev_), kWaitTimeMaxMs));
  EXPECT_EQ(1, cnt_);

  // Expect the alarm to fire quickly.
  EXPECT_GE(50u, timer.GetMilliseconds());
}

// The test takes about 50 seconds to run.
TEST_F(QueueTimerTest, Alarm) {
  if (!IsBuildSystem()) {
    std::wcout << _T("\tThe long timing test below only runs on build system")
               << std::endl;
    return;
  }

  queue_timer_.reset(new QueueTimer(timer_queue_,
                                    &QueueTimerTest::AlarmCallback,
                                    this));
  const int kWaitTimeMaxMs = 60 * 1000;     // 60 seconds.

  // Set the timer to fire once after 5 sec, 10 sec, 15 sec, 20 sec and wait.
  LowResTimer timer(false);
  for (int i = 1; i <= 4; ++i) {
    const int time_interval_ms = 5 * 1000 * i;
    SCOPED_TRACE(testing::Message() << "time_interval_ms=" << time_interval_ms);

    timer.Start();
    ASSERT_HRESULT_SUCCEEDED(
        queue_timer_->Start(time_interval_ms, 0, WT_EXECUTEONLYONCE));
    EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(ev_), kWaitTimeMaxMs));

    int actual_time_ms = timer.GetMilliseconds();
    timer.Reset();

    // Expect the alarm to fire anytime between a narrow interval.
    EXPECT_EQ(1, cnt_);
    EXPECT_LE(time_interval_ms - 50, actual_time_ms);
    EXPECT_GE(time_interval_ms + 150, actual_time_ms);

    cnt_ = 0;
    ::ResetEvent(get(ev_));
  }

  // Set the timer to fire once after 2000 ms but do not wait for it to fire.
  ASSERT_HRESULT_SUCCEEDED(
      queue_timer_->Start(2000, 0, WT_EXECUTEONLYONCE));
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(ev_), 100));
  EXPECT_EQ(0, cnt_);
}

// The test takes about 35 seconds to run.
TEST_F(QueueTimerTest, Timer) {
  if (!IsBuildSystem()) {
    std::wcout << _T("\tThe long timing test below only runs on build system")
               << std::endl;
    return;
  }

  queue_timer_.reset(new QueueTimer(timer_queue_,
                                    &QueueTimerTest::TimerCallback,
                                    this));
  const int kWaitTimeMaxMs = 60 * 1000;       // 60 seconds.

  max_cnt_ = 4;

  // Set the timer to fire at 10 seconds intervals with an initial delay of
  // 5 seconds.
  LowResTimer timer(true);
  ASSERT_HRESULT_SUCCEEDED(queue_timer_->Start(5000, 10000, WT_EXECUTEDEFAULT));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(ev_), kWaitTimeMaxMs));

  int actual_time_ms = timer.GetMilliseconds();

  EXPECT_EQ(4, cnt_);
  EXPECT_LE(35 * 1000 - 50, actual_time_ms);
  EXPECT_GE(35 * 1000 + 350, actual_time_ms);

  // Tests it can't start periodic timers more than one time.
  ASSERT_EQ(E_UNEXPECTED, queue_timer_->Start(25, 50, WT_EXECUTEDEFAULT));
}

}  // namespace omaha

