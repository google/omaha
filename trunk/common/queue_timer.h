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
// QueueTimer is a wrapper for the kernel queue timer.
//
// There are two ways to use the QueueTimer:
// - alarm, where the timer goes off only once.
// - periodic timer.
// When working as an alarm, the timer must be restarted after it fired. There
// is no need to destroy the whole object.
// When working with a periodic timer, the timer can only be started once.
// Alarm timers fire only once, so they will have to be restarted every time.
// It is easy to deadlock when working with timers. As a general rule, never
// destroy a QueueTimer from its callback.

#ifndef OMAHA_COMMON_QUEUE_TIMER_H__
#define OMAHA_COMMON_QUEUE_TIMER_H__

#include <windows.h>

#include "base/basictypes.h"

namespace omaha {

class QueueTimer {
  public:
    typedef void (*Callback)(QueueTimer* timer);

    QueueTimer(HANDLE timer_queue,  // Caller provided timer queue.
               Callback callback,   // Callback to call when the timer fires.
               void* ctx);          // Caller provided context.

    // The destructor waits for the pending callbacks to finish since we do not
    // want the callback to fire after the C++ object was destroyed.
    ~QueueTimer();

    // Starts a timer. The time is in milliseconds.
    HRESULT Start(int due_time, int period, uint32 flags);

    void* ctx() const { return ctx_; }

    int due_time() const { return due_time_; }

    int period() const { return period_; }

    uint32 flags() const { return flags_; }

  private:
    static void _stdcall TimerCallback(void* param, BOOLEAN timer_or_wait);

    HRESULT DoStart(int due_time, int period, uint32 flags);
    void DoCallback();

    CRITICAL_SECTION cs_;         // Serializes access to shared state.
    CRITICAL_SECTION dtor_cs_;    // Serializes the destruction of the object.
    DWORD callback_tid_;          // The thread id of the callback, if any.
    void* ctx_;
    int due_time_;
    int period_;
    uint32 flags_;
    HANDLE timer_handle_;
    HANDLE timer_queue_;
    Callback callback_;

    DISALLOW_EVIL_CONSTRUCTORS(QueueTimer);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_QUEUE_TIMER_H__

