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
#include <algorithm>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/highres_timer-win32.h"
#include "omaha/common/queue_timer.h"
#include "omaha/core/core.h"
#include "omaha/core/core_metrics.h"
#include "omaha/goopdate/config_manager.h"

namespace omaha {

Scheduler::Scheduler(const Core& core)
    : core_(core) {
  CORE_LOG(L1, (_T("[Scheduler::Scheduler]")));
}

Scheduler::~Scheduler() {
  CORE_LOG(L1, (_T("[Scheduler::~Scheduler]")));

  if (update_timer_.get()) {
    update_timer_.reset(NULL);
  }

  if (code_red_timer_.get()) {
    code_red_timer_.reset(NULL);
  }

  if (timer_queue_) {
    // The destructor blocks on deleting the timer queue and it waits for
    // all timer callbacks to complete.
    ::DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE);
  }
}

HRESULT Scheduler::Initialize() {
  CORE_LOG(L1, (_T("[Scheduler::Initialize]")));

  timer_queue_ = ::CreateTimerQueue();
  if (!timer_queue_) {
    return HRESULTFromLastError();
  }

  cr_debug_timer_.reset(new HighresTimer);

  update_timer_.reset(new QueueTimer(timer_queue_,
                                     &Scheduler::TimerCallback,
                                     this));
  code_red_timer_.reset(new QueueTimer(timer_queue_,
                                       &Scheduler::TimerCallback,
                                       this));

  ConfigManager* config_manager = ConfigManager::Instance();
  int cr_timer_interval_ms = config_manager->GetCodeRedTimerIntervalMs();
  VERIFY1(SUCCEEDED(ScheduleCodeRedTimer(cr_timer_interval_ms)));

  int au_timer_interval_ms = config_manager->GetUpdateWorkerStartUpDelayMs();
  VERIFY1(SUCCEEDED(ScheduleUpdateTimer(au_timer_interval_ms)));

  return S_OK;
}

void Scheduler::TimerCallback(QueueTimer* timer) {
  ASSERT1(timer);
  Scheduler* scheduler = static_cast<Scheduler*>(timer->ctx());
  ASSERT1(scheduler);
  scheduler->HandleCallback(timer);
}

// First, do the useful work and then reschedule the timer. Otherwise, it is
// possible that timer notifications overlap, and the timer can't be further
// rescheduled: http://b/1228095
void Scheduler::HandleCallback(QueueTimer* timer) {
  ConfigManager* config_manager = ConfigManager::Instance();
  if (update_timer_.get() == timer) {
    core_.StartUpdateWorker();
    int au_timer_interval_ms = config_manager->GetAutoUpdateTimerIntervalMs();
    VERIFY1(SUCCEEDED(ScheduleUpdateTimer(au_timer_interval_ms)));
  } else if (code_red_timer_.get() == timer) {
    core_.StartCodeRed();
    int actual_time_ms = static_cast<int>(cr_debug_timer_->GetElapsedMs());
    metric_core_cr_actual_timer_interval_ms = actual_time_ms;
    CORE_LOG(L3, (_T("[code red actual period][%d ms]"), actual_time_ms));
    int cr_timer_interval_ms = config_manager->GetCodeRedTimerIntervalMs();
    VERIFY1(SUCCEEDED(ScheduleCodeRedTimer(cr_timer_interval_ms)));
  } else {
    ASSERT1(false);
  }

  // Since core is a long lived process, aggregate its metrics once in a while.
  core_.AggregateMetrics();
}

HRESULT Scheduler::ScheduleUpdateTimer(int interval_ms) {
  HRESULT hr = update_timer_->Start(interval_ms, 0, WT_EXECUTEONLYONCE);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[can't start update queue timer][0x%08x]"), hr));
  }
  return hr;
}

HRESULT Scheduler::ScheduleCodeRedTimer(int interval_ms) {
  metric_core_cr_expected_timer_interval_ms = interval_ms;
  cr_debug_timer_->Start();
  HRESULT hr = code_red_timer_->Start(interval_ms, 0, WT_EXECUTEONLYONCE);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[can't start Code Red queue timer][0x%08x]"), hr));
  }
  return hr;
}

}  // namespace omaha

