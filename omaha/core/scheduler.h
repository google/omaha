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

#include <atlstr.h>
#include <windows.h>
#include <functional>
#include <list>
#include <memory>

#include "base/basictypes.h"

namespace omaha {

class Core;
class HighresTimer;
class QueueTimer;

using ScheduledWork = std::function<void(HighresTimer*)>;

struct SchedulerItem {
  SchedulerItem(HANDLE timer_queue,
                int start_delay,
                int interval,
                bool has_debug_timer,
                ScheduledWork work_fn);

  ~SchedulerItem();

  int start_delay_ms;
  int interval_ms;
  std::unique_ptr<QueueTimer> timer;

  // Measures the actual time interval between events for debugging
  // purposes. The timer is started when a red alarm is set and then,
  // the value of the timer is read when the alarm goes off.
  std::unique_ptr<HighresTimer> debug_timer;

  // Work function to be run on the timer
  ScheduledWork work;

  static HRESULT ScheduleNext(QueueTimer* timer,
                              HighresTimer* debug_timer,
                              int interval_ms);
  static void TimerCallback(QueueTimer* timer);

  DISALLOW_COPY_AND_ASSIGN(SchedulerItem);
};

class Scheduler {
 public:
  explicit Scheduler();
  ~Scheduler();

  // Starts the scheduler with a delay.
  // Default debug timer to false
  HRESULT Start(int start_delay,
                int interval,
                ScheduledWork work,
                bool has_debug_timer = false);

  // Starts the scheduler with regular interval
  HRESULT Start(int interval, ScheduledWork work, bool has_debug_timer = false);

 private:
  // Timer queue handle for all timers
  HANDLE timer_queue_;
  std::list<SchedulerItem> timers_;
  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace omaha

#endif  // OMAHA_CORE_SCHEDULER_H__
