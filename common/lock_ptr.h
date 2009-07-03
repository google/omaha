// Copyright 2003-2009 Google Inc.
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
// lock_ptr.h
//
// A smart pointer to manage synchronized access to a shared resource.
//
// LockPtr provides a simple and concise syntax for accessing a
// shared resource. The LockPtr is a smart pointer and provides
// pointer operators -> and *. LockPtr does not have copy semantics and it is
// not intended to be stored in containers. Instead, instances of LockPtr are
// usually unamed or short-lived named variables.
//
// LockPtr uses an external lock, it acquires the lock in the constructor, and
// it guarantees the lock is released in the destructor.
//
// Since different types of locks have different method names, such as
// Enter/Exit or Lock/Unlock, etc, LockPtr uses an external customizable policy
// to bind to different operations. The external policy is a set of template
// functions that can be specialized for different types of locks, if needed.
// Think of this policy as an adapter between the lock type and the LockPtr.
//
// Usage: let's assume that we have the type below:
//
// class X {
//  public:
//    X() : i_(0) {}
//    void f() {}
//
//  private:
//    int i_;
//
//    friend int LockPtrTest(int, int);
// };
//
// We have an instance of this type and an external lock instance to serialize
// the access to the X instance.
//
// Using LockPtr, the code is:
//
//    X x;
//    LLock local_lock;
//
//    LockPtr<X>(x, local_lock)->f();
//
// For more example, please see the unit test of the module.



#ifndef OMAHA_COMMON_LOCK_PTR_H_
#define OMAHA_COMMON_LOCK_PTR_H_

#include "omaha/common/debug.h"

namespace omaha {

template <typename T>
class LockPtr {
 public:
  template <typename U>
  LockPtr(T& obj, U& lock)
      : pobj_(&obj),
        plock_(&lock),
        punlock_method_(&LockPtr::Unlock<U>) {
    AcquireLock(lock);
  }

  ~LockPtr() {
    ASSERT1(punlock_method_);
    (this->*punlock_method_)();
  }

  // Pointer behavior
  T& operator*() {
    ASSERT1(pobj_);
    return *pobj_;
  }

  T* operator->() {
    return pobj_;
  }

 private:
  // template method to restore the type of the lock and to call the
  // release policy for the lock
  template <class U>
  void Unlock() {
    ASSERT1(plock_);
    U& lock = *(static_cast<U*>(plock_));
    ReleaseLock(lock);
  }

  T* pobj_;       // managed shared object
  void* plock_;   // type-less lock to control access to pobj_

  void (LockPtr::*punlock_method_)();   // the address of the method to Unlock

  DISALLOW_EVIL_CONSTRUCTORS(LockPtr);
};

// template functions to define the policy of acquiring and releasing
// the locks.
template <class Lock> inline void AcquireLock(Lock& lock) { lock.Lock(); }
template <class Lock> inline void ReleaseLock(Lock& lock) { lock.Unlock(); }

// specialization of policy for diferent types of locks.
#include "omaha/common/synchronized.h"
template <> void inline AcquireLock(CriticalSection& cs) { cs.Enter(); }
template <> void inline ReleaseLock(CriticalSection& cs) { cs.Exit(); }

// Add more policy specializations below, if needed.

}  // namespace omaha

#endif  // OMAHA_COMMON_LOCK_PTR_H_

