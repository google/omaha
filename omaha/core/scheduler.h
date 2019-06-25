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

// TODO(omaha): consider using a waitable timer registered with the reactor. It
// seems a lot less code than using the QueueTimer class.

#ifndef OMAHA_CORE_SCHEDULER_H__
#define OMAHA_CORE_SCHEDULER_H__

#include <windows.h>
#include <functional>
#include <list>
#include <memory>

#include "base/basictypes.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/base/queue_timer.h"

namespace omaha {

using ScheduledWork = std::function<void()>;
using ScheduledWorkWithTimer = std::function<void(HighresTimer*)>;

class Scheduler {
 public:
  explicit Scheduler();
  ~Scheduler();

  // Starts the scheduler that executes |work| with regular |interval| (ms).
  HRESULT Start(int interval, ScheduledWork work) const;

  // Starts the scheduler that executes |work| with regular |interval| (ms)
  // after an initial |delay| (ms).
  HRESULT StartWithDelay(int delay, int interval, ScheduledWork work) const;

  // Start the scheduler on a regular |interval| (ms). The callback is provided
  // a timer which starts after the previous item finishes execution.
  HRESULT StartWithDebugTimer(int interval, ScheduledWorkWithTimer work) const;

 private:
  class SchedulerItem {
   public:
    SchedulerItem(HANDLE timer_queue,
                  int start_delay,
                  int interval,
                  bool has_debug_timer,
                  ScheduledWorkWithTimer work_fn);

    ~SchedulerItem();

    HighresTimer* debug_timer() const {
      return debug_timer_ ? debug_timer_.get() : nullptr;
    }

    int interval_ms() const { return interval_ms_; }

   private:
    int start_delay_ms_;
    int interval_ms_;

    std::unique_ptr<QueueTimer> timer_;

    // Measures the actual time interval between events for debugging
    // purposes. The timer is started when an alarm is set and then,
    // the value of the timer is read when the alarm goes off.
    std::unique_ptr<HighresTimer> debug_timer_;

    ScheduledWorkWithTimer work_;

    static HRESULT ScheduleNext(QueueTimer* timer,
                                HighresTimer* debug_timer,
                                int interval_ms);
    static void TimerCallback(QueueTimer* timer);

    DISALLOW_COPY_AND_ASSIGN(SchedulerItem);
  };

  HRESULT DoStart(int start_delay,
                  int interval,
                  ScheduledWorkWithTimer work,
                  bool has_debug_timer = false) const;

  // Timer queue handle for all QueueTimer objects.
  HANDLE timer_queue_;

  mutable std::list<SchedulerItem> timers_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace omaha

#endif  // OMAHA_CORE_SCHEDULER_H__
