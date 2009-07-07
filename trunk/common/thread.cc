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

#include "omaha/common/thread.h"

#include "omaha/common/debug.h"
#include "omaha/common/exception_barrier.h"
#include "omaha/common/logging.h"
#include "omaha/common/time.h"

namespace omaha {

// The system keeps an event associated with the thread message queue for a
// while even after the thread is dead. It can appear as a handle leak in the
// unit test but in fact it is not.

Thread::Thread() : thread_id_(0), thread_(NULL) {
}

Thread::~Thread() {
  if (thread_) {
    VERIFY1(CloseHandle(thread_));
  }
  thread_ = NULL;
}

// This is the thread proc function as required by win32.
DWORD __stdcall Thread::Prepare(void* this_pointer) {
  ExceptionBarrier eb;

  ASSERT1(this_pointer);
  Thread   * this_thread =  reinterpret_cast<Thread*>(this_pointer);
  Runnable * this_runner =  this_thread->runner_;

  // Create a message queue. Our thread should have one.
  MSG message = {0};
  PeekMessage(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);

  // Start method is waiting on this gate to be open in order
  // to proceed. By opening gate we say: OK thread is running.
  this_thread->start_gate_.Open();

  // Now call the interface method. We are done.
  UTIL_LOG(L4, (L"Thread::Prepare calling thread's Run()"));
  this_runner->Run();

  return 0;
}

// Starts the thread. It does not return until the thread is started.
bool Thread::Start(Runnable* runner) {
  ASSERT1(runner);

  // Allow the thread object to be reused by cleaning its state up.
  if (thread_) {
    VERIFY1(CloseHandle(thread_));
  }
  start_gate_.Close();

  runner_ = runner;
  thread_ = CreateThread(NULL,              // default security attributes
                         0,                 // use default stack size
                         &Thread::Prepare,  // thread function
                         this,              // argument to thread function
                         0,                 // use default creation flags
                         &thread_id_);      // returns the thread identifier
  if (!thread_) {
    return false;
  }
  // Wait until the newly created thread opens the gate for us.
  return start_gate_.Wait(INFINITE);
}

DWORD Thread::GetThreadId() const {
  return thread_id_;
}

HANDLE Thread::GetThreadHandle() const {
  return thread_;
}

bool Thread::Suspend() {
  return (static_cast<DWORD>(-1) != SuspendThread(thread_));
}

bool Thread::Resume() {
  return (static_cast<DWORD>(-1) != ResumeThread(thread_));
}

bool Thread::Terminate(int exit_code) {
  return TRUE == TerminateThread(thread_, exit_code);
}

bool Thread::SetPriority(int priority) {
  return TRUE == SetThreadPriority(thread_, priority);
}

bool Thread::GetPriority(int* priority) const {
  if (!priority) {
    return false;
  }
  *priority = GetThreadPriority(thread_);
  return THREAD_PRIORITY_ERROR_RETURN != *priority;
}

// Waits for handle to become signaled.
bool Thread::WaitTillExit(DWORD msec) const {
  if (!Running()) {
    return true;
  }
  return WAIT_OBJECT_0 == WaitForSingleObject(thread_, msec);
}

// Checks if the thread is running.
bool Thread::Running() const {
  if (NULL == thread_) {
    return false;
  }
  return WAIT_TIMEOUT == WaitForSingleObject(thread_, 0);
}

// Executes an APC request.
void __stdcall Thread::APCProc(ULONG_PTR param) {
  ApcInfo* pInfo = reinterpret_cast<ApcInfo*>(param);
  if (pInfo) {
    if (pInfo->receiver_) {
      pInfo->receiver_->OnApc(pInfo->param_);
    }
    // Deallocates what was allocated in QueueApc.
    delete pInfo;
  }
}

// ApcReceiver wants to execute its OnApc function in the
// context of this thread.
bool Thread::QueueApc(ApcReceiver* receiver, ULONG_PTR param) {
  ASSERT1(receiver);
  if (!Running()) {
    // No reason to queue anything to not running thread.
    return true;
  }

  // This allocation will be freed in Thread::APCProc
  ApcInfo* pInfo = new ApcInfo();
  pInfo->receiver_ = receiver;
  pInfo->param_    = param;
  return 0 != QueueUserAPC(&Thread::APCProc,
                           thread_,
                           reinterpret_cast<ULONG_PTR>(pInfo));
}

bool Thread::PostMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  return TRUE == PostThreadMessage(thread_id_, msg, wparam, lparam);
}

}  // namespace omaha

