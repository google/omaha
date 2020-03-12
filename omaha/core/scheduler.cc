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
#include "omaha/base/logging.h"

namespace omaha {

Scheduler::SchedulerItem::SchedulerItem(HANDLE timer_queue,
                                        int start_delay_ms,
                                        int interval_ms,
                                        bool has_debug_timer,
                                        ScheduledWorkWithTimer work)
    : start_delay_ms_(start_delay_ms), interval_ms_(interval_ms), work_(work) {
  if (has_debug_timer) {
    debug_timer_.reset(new HighresTimer());
  }

  if (timer_queue) {
    timer_.reset(
        new QueueTimer(timer_queue, &SchedulerItem::TimerCallback, this));
    VERIFY_SUCCEEDED(
        ScheduleNext(timer_.get(), debug_timer_.get(), start_delay_ms));
  }
}

Scheduler::SchedulerItem::~SchedulerItem() {
  // QueueTimer dtor may block for pending callbacks.
  if (timer_) {
    timer_.reset();
  }

  if (debug_timer_) {
    debug_timer_.reset();
  }
}

// static
HRESULT Scheduler::SchedulerItem::ScheduleNext(QueueTimer* timer,
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
void Scheduler::SchedulerItem::TimerCallback(QueueTimer* timer) {
  ASSERT1(timer);
  if (!timer) {
    return;
  }

  SchedulerItem* item = reinterpret_cast<SchedulerItem*>(timer->ctx());
  ASSERT1(item);

  if (!item) {
    CORE_LOG(LE, (L"[Expected timer context to contain SchedulerItem]"));
    return;
  }

  // This may be long running, |item| may be deleted in the meantime,
  // however the dtor should block on deleting the |timer| and allow
  // pending callbacks to run.
  if (item && item->work_) {
    item->work_(item->debug_timer());
  }

  if (item) {
    const HRESULT hr = SchedulerItem::ScheduleNext(timer,
                                                   item->debug_timer(),
                                                   item->interval_ms());
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

  timers_.clear();

  if (timer_queue_) {
    // The destructor blocks on deleting the timer queue and it waits for
    // all timer callbacks to complete.
    ::DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE);
    timer_queue_ = NULL;
  }
}

HRESULT Scheduler::StartWithDebugTimer(int interval,
                                       ScheduledWorkWithTimer work) const {
  return DoStart(interval, interval, work, true /*has_debug_timer*/);
}

HRESULT Scheduler::StartWithDelay(int delay,
                                  int interval,
                                  ScheduledWork work) const {
  return DoStart(delay, interval, std::bind(work));
}

HRESULT Scheduler::Start(int interval, ScheduledWork work) const {
  return DoStart(interval, interval, std::bind(work));
}

HRESULT Scheduler::DoStart(int start_delay,
                           int interval,
                           ScheduledWorkWithTimer work_fn,
                           bool has_debug_timer) const {
  CORE_LOG(L1, (L"[Scheduler::Start]"));

  if (!timer_queue_) {
    return HRESULTFromLastError();
  }

  timers_.emplace_back(timer_queue_, start_delay, interval, has_debug_timer,
                       work_fn);
  return S_OK;
}

}  // namespace omaha
