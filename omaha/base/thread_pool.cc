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

#include "base/scoped_ptr.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"

namespace omaha {

namespace {

// Context keeps track the information necessary to execute a work item
// inside a thread pool thread.
class Context {
 public:
  Context(ThreadPool* pool, UserWorkItem* work_item, DWORD coinit_flags)
      : pool_(pool),
        work_item_(work_item),
        coinit_flags_(coinit_flags) {
    ASSERT1(pool);
    ASSERT1(work_item);
  }

  ThreadPool*   pool() const { return pool_; }
  UserWorkItem* work_item() const { return work_item_; }
  DWORD coinit_flags() const { return coinit_flags_; }

 private:
  ThreadPool*   pool_;
  UserWorkItem* work_item_;
  const DWORD   coinit_flags_;

  DISALLOW_EVIL_CONSTRUCTORS(Context);
};

}   // namespace


DWORD WINAPI ThreadPool::ThreadProc(void* param) {
  UTIL_LOG(L4, (_T("[ThreadPool::ThreadProc]")));
  ASSERT1(param);

  Context* context = static_cast<Context*>(param);

  scoped_co_init init_com_apt(context->coinit_flags());
  ASSERT1(SUCCEEDED(init_com_apt.hresult()));

  context->pool()->ProcessWorkItem(context->work_item());
  delete context;
  return 0;
}

ThreadPool::ThreadPool()
    : work_item_count_(0),
      shutdown_delay_(0) {
  UTIL_LOG(L2, (_T("[ThreadPool::ThreadPool]")));
}

ThreadPool::~ThreadPool() {
  UTIL_LOG(L2, (_T("[ThreadPool::~ThreadPool]")));

  if (!shutdown_event_) {
    return;
  }

  DWORD baseline_tick_count = ::GetTickCount();
  if (::SetEvent(get(shutdown_event_))) {
    while (work_item_count_ != 0) {
      ::Sleep(1);
      if (TimeHasElapsed(baseline_tick_count, shutdown_delay_)) {
        UTIL_LOG(LE, (_T("[ThreadPool::~ThreadPool][timeout elapsed]")));

        // Exiting a thread pool that has active work items can result in a
        // race condition and undefined behavior during shutdown.
        ::RaiseException(EXCEPTION_ACCESS_VIOLATION,
                         EXCEPTION_NONCONTINUABLE,
                         0,
                         NULL);
        break;
      }
    }
  }
}

HRESULT ThreadPool::Initialize(int shutdown_delay) {
  shutdown_delay_ = shutdown_delay;
  reset(shutdown_event_, ::CreateEvent(NULL, true, false, NULL));
  return shutdown_event_ ? S_OK : HRESULTFromLastError();
}

void ThreadPool::ProcessWorkItem(UserWorkItem* work_item) {
  ASSERT1(work_item);
  work_item->Process();
  delete work_item;
  ::InterlockedDecrement(&work_item_count_);
}

HRESULT ThreadPool::QueueUserWorkItem(UserWorkItem* work_item,
                                      DWORD coinit_flags,
                                      uint32 flags) {
  UTIL_LOG(L4, (_T("[ThreadPool::QueueUserWorkItem]")));
  ASSERT1(work_item);

  scoped_ptr<Context> context(new Context(this, work_item, coinit_flags));
  work_item->set_shutdown_event(get(shutdown_event_));
  ::InterlockedIncrement(&work_item_count_);
  if (!::QueueUserWorkItem(&ThreadPool::ThreadProc, context.get(), flags)) {
    ::InterlockedDecrement(&work_item_count_);
    return HRESULTFromLastError();
  }

  // The thread pool has the ownership of the work item thereon.
  context.release();
  return S_OK;
}

}   // namespace omaha

