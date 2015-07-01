// Copyright 2004-2010 Google Inc.
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
// Declares some classes and macros to encapsulate
// the synchronization primitives.

// TODO(omaha): remove dependency on atlstr

#ifndef OMAHA_BASE_SYNCHRONIZED_H_
#define OMAHA_BASE_SYNCHRONIZED_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

// This macros are used to create a unique name
// We need to go through two steps of expansion.
// Macro kMakeName1 will expand to string + number
// and macro kMakeName1 will put them together to create
// the unique name.
#define MAKE_NAME2(x, y) x##y
#define MAKE_NAME1(x, y) MAKE_NAME2(x, y)
#define MAKE_NAME(x) MAKE_NAME1(x, __COUNTER__)

// Declare the interface in implement mutual
// exclusion. For in process mutual exclusion
// simple critical sections can be used. For
// interprocess mutual exclusion some named
// kernel mode object will have to be used.
struct Lockable {
  virtual ~Lockable() {}
  virtual bool Lock() const = 0;
  virtual bool Unlock() const = 0;
};

// Scope based mutual exclusion. Locks
// the object on construction and unlocks
// during destruction. Very convinient to use
// with the macros __mutexScope and __mutexBlock
class AutoSync {
  bool first_time_;
 public:
  explicit AutoSync(const Lockable *pLock);
  explicit AutoSync(const Lockable &rLock);
  ~AutoSync();
  // this function is only needed to use with
  // the macro __mutexBlock
  bool FirstTime();
 private:
  const Lockable * lock_;
  DISALLOW_EVIL_CONSTRUCTORS(AutoSync);
};

// the usaage:
// class A : public Lockable {
//
//
//
//  void foo(){
//   __mutexScope(this);
// ......
// .......
// everything is synchronized till the end of the
// function or the time it returns (from any place)
// } // end foo.
//
// void bar() {
// ......
// ...... do something here.
// ......
//   __mutexBlock(this){
//    .... do some other stuff
//    ....
//    ....
//    } everything is synchronized till here
//
// }; // end class A

//
#define __mutexScope(lock) AutoSync MAKE_NAME(hiddenLock)(lock)
#define __mutexBlock(lock) \
    for (AutoSync hiddenLock(lock); hiddenLock.FirstTime(); )

// GLock stands for global lock.
// Implementaion of Lockable to allow mutual exclusion
// between different processes.
// For in-process mutual exclusion use LLock - local lock
class GLock : public Lockable {
 public:
  GLock();
  virtual ~GLock();

  // Create mutex returns the status of creation. Use ::GetLastError for
  // error information.
  bool InitializeWithSecAttr(const TCHAR* name,
                             LPSECURITY_ATTRIBUTES lock_attributes);

  // Create mutex return the status of creation. Sets to default DACL.
  bool Initialize(const TCHAR* name);

  virtual bool Lock() const;
  virtual bool Lock(DWORD dwMilliseconds) const;
  virtual bool Unlock() const;

 private:
#if defined(DEBUG) || defined(ASSERT_IN_RELEASE)
  CString name_;
#endif
  mutable HANDLE mutex_;
  DISALLOW_EVIL_CONSTRUCTORS(GLock);
};

// FakeGLock looks like a GLock, but none of its methods do anything.
// Only used with SharedMemoryPtr, in cases where locking is not required or
// desired.
class FakeGLock : public Lockable {
 public:
  FakeGLock() {}
  virtual ~FakeGLock() {}
  bool InitializeWithSecAttr(const TCHAR*, LPSECURITY_ATTRIBUTES) {
    return true;
  }
  bool Initialize(const TCHAR*) { return true; }
  virtual bool Lock() const { return true; }
  virtual bool Lock(DWORD) const { return true; }
  virtual bool Unlock() const { return true; }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FakeGLock);
};

// LLock stands for local lock.
// means works only inside the process.
// use GLock - global lock for inter-process
// guarded access to data.
class LLock : public Lockable {
 public:
  LLock();
  virtual ~LLock();
  virtual bool Lock() const;
  virtual bool Lock(DWORD wait_ms) const;
  virtual bool Unlock() const;

  // Returns the thread id of the owner or 0 if the lock is not owned.
  DWORD GetOwner() const;
 private:
  mutable CRITICAL_SECTION        critical_section_;
  DISALLOW_EVIL_CONSTRUCTORS(LLock);
};

// A gate is a synchronization object used to either stop all
// threads from proceeding through a point or to allow them all to proceed.
class Gate {
 public:
  // In process gate.
  Gate();

  // Interprocess gate.
  explicit Gate(const TCHAR * event_name);

  ~Gate();

  // Open the gate.
  bool Open();

  // Close the gate.
  bool Close();

  // Wait to enter the gate.
  bool Wait(DWORD msec);

  // Conversion from the object to a HANDLE.
  operator HANDLE() const {return gate_;}

  // Returns S_OK, and sets selected_gate to zero based index of the gate that
  // was opened.
  // Returns E_FAIL if timeout occured or gate was abandoned.
  static HRESULT WaitAny(Gate const * const *gates,
                         int num_gates,
                         DWORD msec,
                         int *selected_gate);

  // Returns S_OK if all gates were opened
  // Returns E_FAIL if timeout occured or gate was abandoned.
  static HRESULT WaitAll(Gate const * const *gates, int num_gates, DWORD msec);

 private:
  bool Initialize(const TCHAR * event_name);
  static HRESULT WaitMultipleHelper(Gate const * const *gates,
                                    int num_gates,
                                    DWORD msec,
                                    int *selected_gate,
                                    bool wait_all);
  HANDLE gate_;
  DISALLOW_EVIL_CONSTRUCTORS(Gate);
};

bool WaitAllowRepaint(const Gate& gate, DWORD msec);

class AutoGateKeeper {
 public:
  explicit AutoGateKeeper(Gate *gate) : gate_(gate) {
    gate_->Open();
  }
  ~AutoGateKeeper() {
    gate_->Close();
  }
 private:
  Gate *gate_;
  DISALLOW_EVIL_CONSTRUCTORS(AutoGateKeeper);
};

// A very simple rather fast lock - if uncontested.  USE ONLY AS A GLOBAL OBJECT
// (i.e., DECLARED AT FILE SCOPE or as a STATIC CLASS MEMBER) - this is not
// enforced.  Uses interlocked instructions on an int to get a fast user-mode
// lock.  (Locks the bus and does a couple of memory references so it isn't
// free.)  Spin-waits to get the lock.  Has the advantage that it needs no
// initialization - thus has no order-of-evaluation problems with respect to
// other global objects.  Does not work (causes deadlock) if locked twice by
// the same thread. (Has no constructor so is initialized to 0 by C++.
// This is why it must be a global or static class member: it doesn't initialize
// itself to 0.  This is also why it doesn't inherit from Lockable, which would
// make it need to initialize a virtual table.)
struct SimpleLock {
  bool Lock() const;
  bool Unlock() const;
 private:
  mutable volatile long lock_;
};

struct SimpleLockWithDelay {
  bool Lock() const;
  bool Unlock() const;
 private:
  mutable volatile long lock_;
};

class AutoSimpleLock {
 public:
  explicit AutoSimpleLock(const SimpleLock& lock)
      : lock_(lock) { lock_.Lock(); }
  ~AutoSimpleLock() { lock_.Unlock(); }
 private:
  const SimpleLock& lock_;
  DISALLOW_EVIL_CONSTRUCTORS(AutoSimpleLock);
};

class AutoSimpleLockWithDelay {
 public:
  explicit AutoSimpleLockWithDelay(const SimpleLockWithDelay& lock)
      : lock_(lock) { lock_.Lock(); }
  ~AutoSimpleLockWithDelay() { lock_.Unlock(); }
 private:
  const SimpleLockWithDelay& lock_;
  DISALLOW_EVIL_CONSTRUCTORS(AutoSimpleLockWithDelay);
};


// allow only one thread to hold a lock
class CriticalSection {
 public:
  CriticalSection();
  ~CriticalSection();

  void Enter();
  void Exit();

 private:
  CRITICAL_SECTION critical_section_;
  uint32 number_entries_;

  DISALLOW_EVIL_CONSTRUCTORS(CriticalSection);
};

// A class that manages a CriticalSection with its lifetime, you pass
// it one in the constructor and then it will either be freed in the
// destructor or implicitly [but only once]
class SingleLock {
 public:
  // TODO(omaha): Not sure if immediately locking is a good idea;
  // the API is asymmetrical (there's an Unlock but no Lock).

  // Lock a critical section immediately
  explicit SingleLock(CriticalSection * cs);

  // If we have not explicitly unlocked it, this destructor will
  ~SingleLock();

  // Release the lock explicitly [should be called only once, after that
  // does nothing. If we do not do so, the destructor will]
  HRESULT Unlock();

 private:
  CriticalSection * critical_section_;

  DISALLOW_EVIL_CONSTRUCTORS(SingleLock);
};

// Encapsulation for kernel Event. Initializes and destroys with it's lifetime
class EventObj {
 public:
  explicit EventObj(const TCHAR * event_name) {
    Init(event_name);
  }
  ~EventObj();
  void Init(const TCHAR * event_name);
  BOOL SetEvent();
  HANDLE GetHandle() { return h_; }

 private:
  HANDLE h_;

  DISALLOW_EVIL_CONSTRUCTORS(EventObj);
};

// Is the given handle signaled?
//
// Typically used for events.
bool IsHandleSignaled(HANDLE h);


enum SyncScope {
  // local to a session
  SYNC_LOCAL,

  // global scope but the name is decorated to make it unique for the user
  SYNC_USER,

  // a globally scoped name
  SYNC_GLOBAL,
};

// Create an id for the events/mutexes that can be used at the given scope
void CreateSyncId(const TCHAR* id, SyncScope scope, CString* sync_id);

// If any place needs to create a mutex that multiple
// processes need to access, use this.
HANDLE CreateMutexWithSyncAccess(const TCHAR* name,
                                 LPSECURITY_ATTRIBUTES lock_attributes);

}  // namespace omaha

#endif  // OMAHA_BASE_SYNCHRONIZED_H_

