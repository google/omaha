// Copyright 2004-2009 Google Inc.
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

#ifndef OMAHA_BASE_THREAD_POOL_H_
#define OMAHA_BASE_THREAD_POOL_H_

#include <windows.h>
#include <objbase.h>

#include <memory>

#include "base/basictypes.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class UserWorkItem {
 public:
  UserWorkItem() : shutdown_event_(NULL) {}
  virtual ~UserWorkItem() {}

  // Template method interface
  void Process() { DoProcess(); }

  HANDLE shutdown_event() const { return shutdown_event_; }
  void set_shutdown_event(HANDLE shutdown_event) {
    shutdown_event_ = shutdown_event;
  }

 private:
  // Executes the work item.
  virtual void DoProcess() = 0;

  // It is the job of implementers to watch for the signaling of this event
  // and shutdown correctly. This event is set when the thread pool is closing.
  // Do not close this event as is owned by the thread pool.
  HANDLE shutdown_event_;
  DISALLOW_COPY_AND_ASSIGN(UserWorkItem);
};

class ThreadPool {
 public:
  ThreadPool();

  // The destructor might block for 'shutdown_delay'.
  ~ThreadPool();

  HRESULT Initialize(int shutdown_delay);
  void Stop();

  // Adds a work item to the queue.
  HRESULT QueueUserWorkItem(std::unique_ptr<UserWorkItem> work_item,
                            DWORD coinit_flags,
                            uint32 flags);

  bool HasWorkItems() const {
    return work_item_count_ > 0;
  }

 private:
  class Context;

  // Calls UserWorkItem::Process() in the context of the worker thread.
  void ProcessWorkItemInContext(std::unique_ptr<Context> context);

  // This is the thread callback required by the underlying windows API.
  static DWORD WINAPI ThreadProc(void* context);

  bool is_stopped() const {
    return !!is_stopped_;
  }

  void set_is_stopped(bool is_stopped) {
    ::InterlockedExchange(&is_stopped_, is_stopped);
  }

  // True if the |Stop| function has been called and all work items
  // have been processed, or the |Stop| function has returned with
  // a timeout.
  volatile LONG is_stopped_;

  // Number of work items in the pool.
  volatile LONG work_item_count_;

  // This event signals when the thread pool destructor is in progress.
  scoped_event shutdown_event_;

  // How many milliseconds to wait for the work items to finish when
  // the thread pool is shutting down. The shutdown delay resolution is ~10ms.
  int shutdown_delay_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}  // namespace omaha

#endif  // OMAHA_BASE_THREAD_POOL_H_

