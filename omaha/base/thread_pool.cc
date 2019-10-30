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

#include "omaha/base/thread_pool.h"

#include <utility>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"

namespace omaha {

// Context keeps track the information necessary to execute a work item
// inside a thread pool thread.
class ThreadPool::Context {
 public:
  Context(ThreadPool* pool, std::unique_ptr<UserWorkItem> work_item, DWORD coinit_flags)
      : pool_(pool),
        work_item_(std::move(work_item)),
        coinit_flags_(coinit_flags) {
    ASSERT1(pool_);
    ASSERT1(work_item_);
  }

  ThreadPool*   pool() const { return pool_; }
  UserWorkItem* work_item() const { return work_item_.get(); }
  DWORD coinit_flags() const { return coinit_flags_; }

 private:
  ThreadPool*   pool_;
  std::unique_ptr<UserWorkItem> work_item_;
  const DWORD   coinit_flags_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPool::Context);
};


DWORD WINAPI ThreadPool::ThreadProc(void* param) {
  UTIL_LOG(L4, (_T("[ThreadPool::ThreadProc]")));
  ASSERT1(param);
  std::unique_ptr<Context> context(static_cast<Context*>(param));
  auto thread_pool = context->pool();
  thread_pool->ProcessWorkItemInContext(std::move(context));
  return 0;
}

ThreadPool::ThreadPool()
    : work_item_count_(0),
      is_stopped_(true),
      shutdown_delay_(0) {
  UTIL_LOG(L2, (_T("[ThreadPool::ThreadPool]")));
}

ThreadPool::~ThreadPool() {
  UTIL_LOG(L2, (_T("[ThreadPool::~ThreadPool]")));
  ASSERT1(is_stopped());

  if (HasWorkItems()) {
    // Destroying a thread pool that has active work items can result in other
    // race condition and non-deterministic behavior during shutdown.
    ::RaiseException(EXCEPTION_ACCESS_VIOLATION,
                     EXCEPTION_NONCONTINUABLE,
                     0,
                     NULL);
  }
}

HRESULT ThreadPool::Initialize(int shutdown_delay) {
  set_is_stopped(false);
  shutdown_delay_ = shutdown_delay;
  reset(shutdown_event_, ::CreateEvent(NULL, true, false, NULL));
  return shutdown_event_ ? S_OK : HRESULTFromLastError();
}

void ThreadPool::Stop() {
  UTIL_LOG(L2, (_T("[ThreadPool::Stop]")));

  if (is_stopped()) {
     return;
  }

  DWORD baseline_tick_count = ::GetTickCount();
  if (::SetEvent(get(shutdown_event_))) {
    while (HasWorkItems()) {
      ::Sleep(1);
      if (TimeHasElapsed(baseline_tick_count, shutdown_delay_)) {
        UTIL_LOG(LE, (_T("[ThreadPool::Stop][timeout elapsed]")));
        break;
      }
    }
  }

  set_is_stopped(true);
}

void ThreadPool::ProcessWorkItemInContext(std::unique_ptr<Context> context) {
  ASSERT1(context);

  {
    scoped_co_init init_com_apt(context->coinit_flags());
    ASSERT1(SUCCEEDED(init_com_apt.hresult()));

    context->work_item()->Process();
    context.reset();
  }

  ::InterlockedDecrement(&work_item_count_);
}

HRESULT ThreadPool::QueueUserWorkItem(std::unique_ptr<UserWorkItem> work_item,
                                      DWORD coinit_flags,
                                      uint32 flags) {
  UTIL_LOG(L4, (_T("[ThreadPool::QueueUserWorkItem]")));
  ASSERT1(work_item);

  if (is_stopped()) {
     return E_FAIL;
  }

  work_item->set_shutdown_event(get(shutdown_event_));
  auto context = std::make_unique<Context>(this,
                                           std::move(work_item),
                                           coinit_flags);
  ::InterlockedIncrement(&work_item_count_);
  if (!::QueueUserWorkItem(&ThreadPool::ThreadProc, context.get(), flags)) {
    ::InterlockedDecrement(&work_item_count_);
    return HRESULTFromLastError();
  }

  context.release();
  return S_OK;
}

}   // namespace omaha

