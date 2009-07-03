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
// Defines methods of classes used to encapsulate
// the synchronization primitives.

#include "omaha/common/synchronized.h"

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/system.h"
#include "omaha/common/system_info.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"

namespace omaha {

typedef HANDLE WINAPI CreateMutexExFunction(
  LPSECURITY_ATTRIBUTES attributes,
  const WCHAR* name,
  DWORD flags,
  DWORD desired_access
);

#define CREATE_EVENT_MANUAL_RESET 0x01
typedef HANDLE WINAPI CreateEventExFunction(
  LPSECURITY_ATTRIBUTES attributes,
  const WCHAR* name,
  DWORD flags,
  DWORD desired_access
);

CreateMutexExFunction* create_mutex_ex_function = NULL;
CreateEventExFunction* create_event_ex_function = NULL;

void EnsureCreateEx() {
  if ((create_mutex_ex_function && create_event_ex_function) ||
      !SystemInfo::IsRunningOnVistaOrLater()) {
    return;
  }

  HMODULE kernel32_module = ::GetModuleHandle(_T("kernel32.dll"));
  if (!kernel32_module) {
    ASSERT(kernel32_module,
           (_T("[GetModuleHandle error 0x%x]"), ::GetLastError()));
    return;
  }
  GPA(kernel32_module, "CreateMutexExW", &create_mutex_ex_function);
  ASSERT(create_mutex_ex_function,
         (_T("[GPA error 0x%x]"), ::GetLastError()));

  GPA(kernel32_module, "CreateEventExW", &create_event_ex_function);
  ASSERT(create_event_ex_function,
         (_T("[GPA error 0x%x]"), ::GetLastError()));
}

HANDLE CreateMutexWithSyncAccess(const TCHAR* name,
                                 LPSECURITY_ATTRIBUTES lock_attributes) {
  EnsureCreateEx();
  if (create_mutex_ex_function) {
    return create_mutex_ex_function(lock_attributes, name, 0,
                                    SYNCHRONIZE |
                                    MUTEX_MODIFY_STATE);  // for ReleaseMutex
  }
  return ::CreateMutex(lock_attributes, false, name);
}

HANDLE CreateEventWithSyncAccess(const TCHAR* name,
                                 LPSECURITY_ATTRIBUTES event_attributes) {
  EnsureCreateEx();
  if (create_event_ex_function) {
    return create_event_ex_function(event_attributes, name,
                                    CREATE_EVENT_MANUAL_RESET,
                                    SYNCHRONIZE |
                                    EVENT_MODIFY_STATE);  // for Set/Reset, etc.
  }
  return ::CreateEvent(event_attributes, true, false, name);
}

// c-tor will take mutex.
AutoSync::AutoSync(const Lockable *pLock)
    : lock_(pLock),
      first_time_(true) {
  ASSERT(lock_, (L""));
  VERIFY(lock_->Lock(), (L"Failed to lock in constructor"));
}

// c-tor will take mutex.
AutoSync::AutoSync(const Lockable &rLock)
    : lock_(&rLock),
      first_time_(true) {
  ASSERT(lock_, (L""));
  VERIFY(lock_->Lock(), (L"Failed to lock in constructor"));
}

// d-tor will release mutex.
AutoSync::~AutoSync() {
  ASSERT(lock_, (L""));
  VERIFY(lock_->Unlock(), (L"Failed to unlock in denstructor"));
}

// Allows to write the for loop of __mutexBlock macro
bool AutoSync::FirstTime() {
  if (first_time_) {
     first_time_ = false;
     return true;
  }
  return false;
}

// Constructor.
GLock::GLock() : mutex_(NULL) {
}

bool GLock::InitializeWithSecAttr(const TCHAR* name,
                                  LPSECURITY_ATTRIBUTES lock_attributes) {
  ASSERT(!mutex_, (L""));

#if defined(DEBUG) || defined(ASSERT_IN_RELEASE)
  name_ = name;
#endif

  mutex_ = CreateMutexWithSyncAccess(name, lock_attributes);
  return mutex_ != NULL;
}

// Create mutex return the status of creation. Sets to default DACL.
bool GLock::Initialize(const TCHAR* name) {
  return InitializeWithSecAttr(name, NULL);
}

// Clean up.
GLock::~GLock() {
  if (mutex_) {
    VERIFY(::CloseHandle(mutex_), (_T("")));
  }
};

// Wait until signaled.
bool GLock::Lock() const {
  return Lock(INFINITE);
}

bool GLock::Lock(DWORD dwMilliseconds) const {
  ASSERT1(mutex_);

  DWORD ret = ::WaitForSingleObject(mutex_, dwMilliseconds);
  if (ret == WAIT_OBJECT_0) {
    return true;
  } else if (ret == WAIT_ABANDONED) {
    UTIL_LOG(LE, (_T("[GLock::Lock - mutex was abandoned %s]"), name_));
    return true;
  }
  return false;
}

// Release.
bool GLock::Unlock() const {
  ASSERT1(mutex_);

  bool ret = (false != ::ReleaseMutex(mutex_));
  ASSERT(ret, (_T("ReleaseMutex failed.  Err=%i"), ::GetLastError()));

  return ret;
}

LLock::LLock() {
  InitializeCriticalSection(&critical_section_);
}

LLock::~LLock() {
  DeleteCriticalSection(&critical_section_);
}

bool LLock::Lock() const {
  EnterCriticalSection(&critical_section_);
  return true;
}

// not very precise funcion, but OK for our goals.
bool LLock::Lock(DWORD wait_ms) const {
  if (::TryEnterCriticalSection(&critical_section_))
    return true;
  DWORD ticks_at_the_begin_of_wait = GetTickCount();
  do {
    ::Sleep(0);
    if (::TryEnterCriticalSection(&critical_section_)) {
      return true;
    }
  } while (::GetTickCount() - ticks_at_the_begin_of_wait <  wait_ms);
  return false;
}

bool LLock::Unlock() const {
  LeaveCriticalSection(&critical_section_);
  return true;
}

// Use this c-tor for interprocess gates.
Gate::Gate(const TCHAR * event_name) : gate_(NULL) {
  VERIFY(Initialize(event_name), (_T("")));
}

// Use this c-tor for in-process gates.
Gate::Gate() : gate_(NULL) {
  VERIFY(Initialize(NULL), (_T("")));
}

// clean up.
Gate::~Gate() {
  VERIFY(CloseHandle(gate_), (_T("")));
}

bool Gate::Initialize(const TCHAR * event_name) {
  // event_name may be NULL
  ASSERT1(gate_ == NULL);

  // Create the event. The gate is initially closed.
  // if this is in process gate we don't name event, otherwise we do.
  // Created with default permissions.
  gate_ = CreateEventWithSyncAccess(event_name, NULL);
  return (NULL != gate_);
}

// Open the gate. Anyone can go through.
bool Gate::Open() {
  return FALSE != SetEvent(gate_);
}

// Shut the gate closed.
bool Gate::Close() {
  return FALSE != ResetEvent(gate_);
}

bool Gate::Wait(DWORD msec) {
  return WAIT_OBJECT_0 == WaitForSingleObject(gate_, msec);
}

// Returns S_OK, and sets selected_gate to zero based index of the gate that
// was opened
// Returns E_FAIL if timeout occured or gate was abandoned.
HRESULT Gate::WaitAny(Gate const * const *gates,
                      int num_gates,
                      DWORD msec,
                      int *selected_gate) {
  ASSERT1(selected_gate);
  ASSERT1(gates);

  return WaitMultipleHelper(gates, num_gates, msec, selected_gate, false);
}

// Returns S_OK if all gates were opened
// Returns E_FAIL if timeout occured or gate was abandoned.
HRESULT Gate::WaitAll(Gate const * const *gates, int num_gates, DWORD msec) {
  ASSERT1(gates);

  return WaitMultipleHelper(gates, num_gates, msec, NULL, true);
}

HRESULT Gate::WaitMultipleHelper(Gate const * const *gates,
                                 int num_gates,
                                 DWORD msec,
                                 int *selected_gate,
                                 bool wait_all) {
  ASSERT1(gates);
  ASSERT(num_gates > 0, (_T("There must be at least 1 gate")));

  if ( num_gates <= 0 ) {
    return E_FAIL;
  }
  HANDLE *gate_array = new HANDLE[ num_gates ];
  ASSERT1(gate_array);
  for ( int i = 0 ; i < num_gates ; ++i ) {
    gate_array[ i ] = gates[ i ]->gate_;
  }
  DWORD res = WaitForMultipleObjects(num_gates,
                                     gate_array,
                                     wait_all ? TRUE : FALSE,
                                     msec);
  delete[] gate_array;

#pragma warning(disable : 4296)
// C4296: '>=' : expression is always true
  if (WAIT_OBJECT_0 <= res && res < (WAIT_OBJECT_0 + num_gates)) {
    if (selected_gate) {
      *selected_gate = res - WAIT_OBJECT_0;
    }
    return S_OK;
  }
#pragma warning(default : 4296)
  return E_FAIL;
}

bool WaitAllowRepaint(const Gate& gate, DWORD msec) {
  DWORD wait = 0;
  HANDLE gate_handle = gate;
  while ((wait = ::MsgWaitForMultipleObjects(1,
                                             &gate_handle,
                                             FALSE,
                                             msec,
                                             QS_PAINT)) == WAIT_OBJECT_0 + 1) {
    MSG msg;
    if (::PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE) ||
        ::PeekMessage(&msg, NULL, WM_NCPAINT, WM_NCPAINT, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }
  return (wait == WAIT_OBJECT_0);
}

// SimpleLock
// TODO(omaha): Replace InterlockedCompareExchange with
// InterlockedCompareExchangeAcquire
// and InterlockedDecrement with InterlockedDecrementRelease for Windows 2003

bool SimpleLock::Lock() const {
  while (1 == ::InterlockedCompareExchange(&lock_, 1, 0))
    ::SleepEx(0, TRUE);
  return true;
}

bool SimpleLock::Unlock() const {
  ::InterlockedDecrement(&lock_);
  return true;
}

// same with a delay in the loop to prevent CPU usage with significant
// contention

bool SimpleLockWithDelay::Lock() const {
  while (1 == ::InterlockedCompareExchange(&lock_, 1, 0))
    ::SleepEx(25, FALSE);
  return true;
}

bool SimpleLockWithDelay::Unlock() const {
  ::InterlockedDecrement(&lock_);
  return true;
}


CriticalSection::CriticalSection()
: number_entries_(0) {
  InitializeCriticalSection(&critical_section_);
}

// allow only one thread to hold a lock
CriticalSection::~CriticalSection() {
  // we should not have to do anything in the destructor
  ASSERT(!number_entries_, (_T("critical section destroyed while active")));
  while (number_entries_) {
    LeaveCriticalSection(&critical_section_);
    number_entries_--;
  }

  DeleteCriticalSection(&critical_section_);
}

// enter the critical section
// entries may be nested
void CriticalSection::Enter() {
  EnterCriticalSection(&critical_section_);
  number_entries_++;
}

// exit the critical section
// number of exits must match number of entries
void CriticalSection::Exit() {
  LeaveCriticalSection(&critical_section_);
  number_entries_--;
}

// Take a CriticalSection and lock it
SingleLock::SingleLock(CriticalSection * cs) {
  ASSERT(cs, (L""));
  critical_section_ = cs;
  critical_section_->Enter();
}

// If we haven't freed it yet, do so now since we fell out of scope
SingleLock::~SingleLock() {
  if (critical_section_) {
    critical_section_->Exit();
    critical_section_ = NULL;
  }
}

// Explicitly unlock
HRESULT SingleLock::Unlock() {
  // If they did not
  if (critical_section_ == NULL)
    return S_FALSE;

  critical_section_->Exit();
  critical_section_ = NULL;
  return S_OK;
}

// Encapsulation for kernel Event. Initializes and destroys with it's lifetime
void EventObj::Init(const TCHAR * event_name) {
  ASSERT(event_name, (L""));

  h_ = ::CreateEvent(NULL, false, false, event_name);
  ASSERT1(h_);
}

EventObj::~EventObj() {
  if (h_) {
    VERIFY(CloseHandle(h_), (L""));
    h_ = NULL;
  }
}

BOOL EventObj::SetEvent() {
  ASSERT(h_, (L""));
  return ::SetEvent(h_);
}

// Is the given handle signaled?
//
// Typically used for events.
bool IsHandleSignaled(HANDLE h) {
  ASSERT(h != NULL &&
         h != INVALID_HANDLE_VALUE, (_T("")));

  DWORD result = ::WaitForSingleObject(h, 0);
  if (result == WAIT_OBJECT_0) {
    return true;
  }

  ASSERT(result == WAIT_TIMEOUT,
         (_T("unexpected result value: %u (hr=0x%x)"),
          result, HRESULTFromLastError()));
  return false;
}


// Create an id for the events/mutexes that can be used at the given scope.
// TODO(omaha): Error handling.
void CreateSyncId(const TCHAR* id, SyncScope scope, CString* sync_id) {
  ASSERT1(id);
  ASSERT1(sync_id);

  CString postfix;
  switch (scope) {
    default:
      ASSERT1(false);
      break;

    case SYNC_LOCAL:
      sync_id->SetString(_T("Local\\"));
      // no postfix for local ids
      break;

    case SYNC_USER:
    case SYNC_GLOBAL:
      sync_id->SetString(_T("Global\\"));

      if (scope == SYNC_GLOBAL) {
        // (MSDN insists that you can create objects with the same name with the
        // prefixes "Global\" and "Local\" in a system "running Terminal
        // Services". And it also assures that XP when running Fast User
        // Switching uses Terminal Services. But when you try to create two
        // objects with the same name but in the different namespaces on an
        // XP Pro workstation NOT running Fast User Switching you can't - you
        // get ERROR_ALREADY_EXISTS. And the reason is that in the Object table,
        // Global and Local are both symlinks to the same object directory.
        // Yet every technique that you can use to interrogate the system on
        // whether or not the system is "running Terminal Services" says that
        // the system is, in fact, running Terminal Services.
        // Which is exactly what you'd expect, yet you can't create the
        // two objects with the same name in different workspaces.  So we change
        // the name slightly.)
        postfix.SetString(_T("_global"));
      } else {
        ASSERT1(scope == SYNC_USER);
        // make the postfix the sid
        VERIFY1(SUCCEEDED(omaha::user_info::GetCurrentUser(NULL,
                                                           NULL,
                                                           &postfix)));
      }
      break;
  }

  sync_id->Append(id);
  sync_id->Append(postfix);
}

}  // namespace omaha

