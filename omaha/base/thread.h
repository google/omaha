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
//
// Defines interface Runnable and class Thread.
//
// Thread encapsulates win32 primitives of creating and
// manipulating win32 threads.

#ifndef OMAHA_COMMON_THREAD_H__
#define OMAHA_COMMON_THREAD_H__

#include "omaha/base/synchronized.h"

namespace omaha {

// Any class which requires part of its execution in a
// separate thread should be derived from Runnable interface.
// It can have member variable of type Thread. When the
// thread needs to be launched one does something like that.
//  A::func() {
//    thread_.start(this);
//  }

class Runnable {
  friend class Thread;
 protected:
  Runnable() {}
  virtual ~Runnable() {}
  virtual void Run() = 0;
 private:
  DISALLOW_EVIL_CONSTRUCTORS(Runnable);
};

// Any class devived from this one will be able to call
// Thread function QueueApc and have the function OnApc get
// executed in context of this thread. Thread must be in alertable
// state to be able to execute the apc function.
class ApcReceiver {
  friend class Thread;
 protected:
  ApcReceiver() {}
  virtual ~ApcReceiver() {}
  virtual void OnApc(ULONG_PTR param) = 0;
 private:
  DISALLOW_EVIL_CONSTRUCTORS(ApcReceiver);
};

// This class encapsulates win32 thread management functions.
class Thread {
 public:
  Thread();
  ~Thread();

  bool Start(Runnable* runner);
  bool Suspend();
  bool Resume();
  bool Terminate(int exit_code);
  bool SetPriority(int priority);
  bool GetPriority(int* priority) const;
  DWORD GetThreadId() const;
  HANDLE GetThreadHandle() const;

  // Checks if the thread is running.
  bool Running() const;

  // Waits until thread exits.
  bool WaitTillExit(DWORD msec) const;

  // Queues an APC to the ApcReceiver.
  bool QueueApc(ApcReceiver* receiver, ULONG_PTR param);

  // Posts message to a thread.
  bool PostMessage(UINT msg, WPARAM wparam, LPARAM lparam);
 private:
  static DWORD __stdcall Prepare(void* thisPointer);      // Thread proc.
  static void __stdcall APCProc(ULONG_PTR dwParam);

  Runnable* runner_;     // Interface to work with.
  HANDLE    thread_;
  DWORD     thread_id_;
  Gate start_gate_;     // Synchronizes the thread start.

  struct ApcInfo {
    ApcReceiver* receiver_;
    ULONG_PTR    param_;
  };

  DISALLOW_EVIL_CONSTRUCTORS(Thread);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_THREAD_H__
