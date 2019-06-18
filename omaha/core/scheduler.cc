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

#include "omaha/core/scheduler.h"

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/base/logging.h"
#include "omaha/base/queue_timer.h"
#include "omaha/common/config_manager.h"
#include "omaha/core/core.h"
#include "omaha/core/core_metrics.h"

namespace omaha {

SchedulerItem::SchedulerItem(HANDLE timer_queue,
                             int start_delay_ms,
                             int interval_ms,
                             bool has_debug_timer,
                             ScheduledWork work_fn)
    : start_delay_ms(start_delay_ms), interval_ms(interval_ms), work(work_fn) {
  if (has_debug_timer) {
    debug_timer.reset(new HighresTimer());
  }

  if (timer_queue) {
    timer.reset(
        new QueueTimer(timer_queue, &SchedulerItem::TimerCallback, this));
    VERIFY1(SUCCEEDED(
        ScheduleNext(timer.get(), debug_timer.get(), start_delay_ms)));
  }
}

SchedulerItem::~SchedulerItem() {
  // QueueTimer dtor may block for pending callbacks
  timer.reset(nullptr);
}

// static
HRESULT SchedulerItem::ScheduleNext(QueueTimer* timer,
                                    HighresTimer* debug_timer,
                                    int start_after_ms) {
  if (!timer) {
    return E_FAIL;
  }

  if (debug_timer) {
    debug_timer->Start();
  }

  const HRESULT hr = timer->Start(start_after_ms, 0, WT_EXECUTEONLYONCE);

  if (FAILED(hr)) {
    CORE_LOG(LE, (L"[can't start queue timer][0x%08x]", hr));
  }

  return hr;
}

// static
void SchedulerItem::TimerCallback(QueueTimer* timer) {
  ASSERT1(timer);
  if (!timer) {
    return;
  }

  SchedulerItem* item = reinterpret_cast<SchedulerItem*>(timer->ctx());
  ASSERT1(item);

  if (!item) {
    return;
  }

  // This may be long running, item may be deleted in the meantime
  if (item && item->work) {
    item->work(item->debug_timer.get());
  }

  if (item) {
    const HRESULT hr = SchedulerItem::ScheduleNext(
        timer, item->debug_timer.get(), item->interval_ms);
    if (FAILED(hr)) {
      CORE_LOG(L1, (L"[Scheduling next timer callback failed][0x%08x]", hr));
    }
  }
}

Scheduler::Scheduler() {
  CORE_LOG(L1, (L"[Scheduler::Scheduler]"));
  timer_queue_ = ::CreateTimerQueue();
  if (!timer_queue_) {
    CORE_LOG(LE, (L"[Failed to create Timer Queue][%d]", ::GetLastError()));
  }
}

Scheduler::~Scheduler() {
  CORE_LOG(L1, (L"[Scheduler::~Scheduler]"));

  // Reset all the timers
  timers_.clear();

  if (timer_queue_) {
    // The destructor blocks on deleting the timer queue and it waits for
    // all timer callbacks to complete.
    ::DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE);
    timer_queue_ = nullptr;
  }
}

HRESULT Scheduler::Start(int interval,
                         ScheduledWork work,
                         bool has_debug_timer) {
  return Start(interval, interval, work, has_debug_timer);
}

HRESULT Scheduler::Start(int start_delay,
                         int interval,
                         ScheduledWork work_fn,
                         bool has_debug_timer) {
  CORE_LOG(L1, (L"[Scheduler::Start]"));

  if (!timer_queue_) {
    return HRESULTFromLastError();
  }

  timers_.emplace_back(timer_queue_, start_delay, interval, has_debug_timer,
                       work_fn);
  return S_OK;
}

}  // namespace omaha
