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
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace omaha {

class Core;
class HighresTimer;
class QueueTimer;

class Scheduler {
 public:
  explicit Scheduler(const Core& core);
  ~Scheduler();

  // Starts the scheduler.
  HRESULT Initialize();

 private:
  static void TimerCallback(QueueTimer* timer);
  void HandleCallback(QueueTimer* timer);
  HRESULT ScheduleUpdateTimer(int interval_ms);
  HRESULT ScheduleCodeRedTimer(int interval_ms);

  const Core& core_;
  HANDLE timer_queue_;
  scoped_ptr<QueueTimer> update_timer_;
  scoped_ptr<QueueTimer> code_red_timer_;

  // Measures the actual time interval between code red events for debugging
  // purposes. The timer is started when a code red alarm is set and then,
  // the value of the timer is read when the alarm goes off.
  scoped_ptr<HighresTimer> cr_debug_timer_;

  DISALLOW_EVIL_CONSTRUCTORS(Scheduler);
};

}  // namespace omaha

#endif  // OMAHA_CORE_SCHEDULER_H__

