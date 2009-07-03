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
//
// The implementation is straightforward except the destruction of the
// QueueTimer which needs some clarification.
// If there is no callback running, then the destructor gets the critical
// section and then it blocks on the DeleteTimerQueueTimer call, waiting for
// the kernel to clean up the timer handle. The callback never fires in this
// case.
// If a callback is running, then there are two possibilities:
// 1. The callback gets the critical section. The callback runs as usual and
// then the destructor gets the critical section. This is also easy.
// 2. The destructor gets the critical section. In this case, the callback
// tries the critical section then it returns right away.
//
// Alarm timers are started and restarted every time they fire. The usage
// patterns for alarms is usually Start, Callback, Start, Callback, etc...
// The cleanup of an alarm timer handle usually happens in the callback, unless
// the destructor of the QueueTimer is called, in which case the logic
// above applies.
//
// Periodic timers are only started once: Start, Callback, Callback, etc...
// In this case, the destructor does all the necessary cleanup.
// Periodic timers must fire at intervals that are reasonable long so that
// the callbacks do not queue up.

#include "omaha/common/queue_timer.h"

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"

namespace omaha {

QueueTimer::QueueTimer(HANDLE timer_queue, Callback callback, void* ctx)
    : callback_tid_(0),
      ctx_(ctx),
      due_time_(0),
      period_(0),
      flags_(0),
      timer_handle_(NULL),
      timer_queue_(timer_queue),
      callback_(callback) {
  UTIL_LOG(L3, (_T("[QueueTimer::QueueTimer][0x%08x]"), this));
  ASSERT1(timer_queue);
  ASSERT1(callback);
  ::InitializeCriticalSection(&dtor_cs_);
  ::InitializeCriticalSection(&cs_);
}

// The destructor blocks on waiting for the timer kernel object to be deleted.
// We can't call the destructor of QueueTimer while we are handling a callback.
// This will result is a deadlock.
QueueTimer::~QueueTimer() {
  UTIL_LOG(L3, (_T("[QueueTimer::~QueueTimer][0x%08x]"), this));

  ::EnterCriticalSection(&dtor_cs_);
  if (timer_handle_) {
    ASSERT1(callback_tid_ != ::GetCurrentThreadId());

    // This is a blocking call waiting for all callbacks to clear up.
    bool res = !!::DeleteTimerQueueTimer(timer_queue_,
                                         timer_handle_,
                                         INVALID_HANDLE_VALUE);
    ASSERT1(res);
    timer_handle_ = NULL;
  }
  callback_ = NULL;
  timer_queue_ = NULL;
  flags_ = 0;
  period_ = 0;
  due_time_ = 0;
  ctx_ = 0;
  callback_tid_ = 0;
  ::LeaveCriticalSection(&dtor_cs_);

  ::DeleteCriticalSection(&cs_);
  ::DeleteCriticalSection(&dtor_cs_);
}

// Thread safe.
HRESULT QueueTimer::Start(int due_time, int period, uint32 flags) {
  // Since Start creates the timer there could be a race condition where
  // the timer could fire while we are still executing Start. We protect
  // the start with a critical section so the Start completes before the
  // timer can be entered by the callback.

  ::EnterCriticalSection(&cs_);
  HRESULT hr = DoStart(due_time, period, flags);
  ::LeaveCriticalSection(&cs_);
  return hr;
}

// Thread-safe.
void QueueTimer::TimerCallback(void* param, BOOLEAN timer_or_wait) {
  ASSERT1(param);
  VERIFY1(timer_or_wait);

  QueueTimer* timer = static_cast<QueueTimer*>(param);

  if (!::TryEnterCriticalSection(&timer->dtor_cs_)) {
    return;
  }

  ::EnterCriticalSection(&timer->cs_);
  timer->DoCallback();
  ::LeaveCriticalSection(&timer->cs_);

  ::LeaveCriticalSection(&timer->dtor_cs_);
}


HRESULT QueueTimer::DoStart(int due_time, int period, uint32 flags) {
  due_time_ = due_time;
  period_ = period;
  flags_ = flags;

  // Application Verifier says period must be 0 for WT_EXECUTEONLYONCE timers.
  if ((flags & WT_EXECUTEONLYONCE) && period != 0) {
    return E_INVALIDARG;
  }

  // Periodic timers can't be started more than one time.
  if (timer_handle_) {
    return E_UNEXPECTED;
  }

  bool res = !!::CreateTimerQueueTimer(&timer_handle_,
                                       timer_queue_,
                                       &QueueTimer::TimerCallback,
                                       this,
                                       due_time,
                                       period,
                                       flags_);
  if (!res) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[QueueTimer::Start failed][0x%08x][0x%08x]"), hr, this));
    return hr;
  }

  ASSERT1(timer_handle_);
  UTIL_LOG(L3, (_T("[QueueTimer::Start timer created][0x%08x]"), this));
  return S_OK;
}

void QueueTimer::DoCallback() {
  UTIL_LOG(L2, (_T("[QueueTimer::OnCallback][0x%08x]"), this));

  ASSERT1(timer_queue_);
  ASSERT1(timer_handle_);
  ASSERT1(callback_);

  if (!period_) {
    // Non-periodic aka alarm timers fire only once. We delete the timer
    // handle so that the timer object can be restarted later on.
    // The call below is non-blocking. The deletion of the kernel object can
    // succeed right away, for example if the timer runs in the timer thread
    // itself. Otherwise, if the last error is ERROR_IO_PENDING the kernel
    // cleans up the object once the callback returns.
    bool res = !!::DeleteTimerQueueTimer(timer_queue_, timer_handle_, NULL);
    ASSERT1(res || (!res && ::GetLastError() == ERROR_IO_PENDING));
    timer_handle_ = NULL;
  }

  callback_tid_ = ::GetCurrentThreadId();
  callback_(this);
  callback_tid_ = 0;
}

}  // namespace omaha

