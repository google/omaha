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

#include "omaha/common/sta.h"

#include <atlbase.h>
#include <atlwin.h>
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/sta_call.h"
#include "omaha/common/utils.h"

namespace omaha {

namespace {

class CallDispatcher;

// ApartmentState maintains the state of the apartment. It keeps a reference
// to a call dispatcher. The dispatcher is basically a window object that
// handles user messages corresponding to each call.
//
// ApartmentState is a singleton. It's lifetime is controlled by the
// 'InitializeApartment' and 'UnintializeApartment'.
//
// The current implementation is limited to the main STA. Creating multiple
// apartments in a process is not possible.
//
// TODO(omaha): implement a simple reference counting of the threads to make
// sure the all the calling threads have returned when the apt is destroyed.

class ApartmentState {
 public:
  ApartmentState(DWORD thread_id, CallDispatcher* call_disp)
      : thread_id_(thread_id), call_dispatcher_(call_disp) {}

  ~ApartmentState() {
    ASSERT1(ref_cnt_ == 0);
  }

  // Initialize the state of the singleton.
  static HRESULT Initialize(DWORD reserved);

  // Uninitialize the state of the singleton.
  static HRESULT Uninitialize();

  // Accessors.
  static ApartmentState* apartment_state() {
    return apartment_state_;
  }

  CallDispatcher& call_dispatcher() const {
    ASSERT(call_dispatcher_.get(),
      (_T("'InitializeApartment' has not been called.")));
    return *call_dispatcher_;
  }

  DWORD thread_id() const {
    ASSERT(thread_id_,
      (_T("'InitializeApartment' has not been called.")));
    return thread_id_;
  }

 private:
  static int ref_cnt_;    // the reference count of init/uninit.

  const DWORD thread_id_;   // thread that called the InitializeApartment
  scoped_ptr<CallDispatcher> call_dispatcher_;

  static ApartmentState* apartment_state_;    // the instance of the state
  DISALLOW_EVIL_CONSTRUCTORS(ApartmentState);
};


// CallDispatcher uses functors to cross call from the caller's thread to
// this apartment thread (main thread).
class CallDispatcher
    : public CWindowImpl<CallDispatcher,
                         CWindow,
                         CWinTraits<WS_OVERLAPPED, WS_EX_TOOLWINDOW> > {
 public:
  explicit CallDispatcher(DWORD options);
  ~CallDispatcher();

  // Two-phase initialization
  HRESULT Init();

  // The initiator of the cross call.
  HRESULT DoCrossApartmentCall(BaseFunctor* caller, void* presult);

 private:
  static const UINT WM_METHOD_CALL = WM_USER + 0x100;
  static const UINT WM_METHOD_CALL_COMPLETE = WM_USER + 0x101;

  BEGIN_MSG_MAP(CallDispatcher)
    MESSAGE_HANDLER(WM_METHOD_CALL, OnMethodCall)
  END_MSG_MAP()

 private:
  LRESULT OnMethodCall(UINT uMsg, WPARAM wParam,
    LPARAM lParam, BOOL& bHandled);

  DWORD options_;

  // Currently only one option is supported for testing purposes.
  // It testing mode, this option disables the cross-call mechanism and
  // directly calls the functor in the same thread as the invoker.
  static const DWORD kTestingMode = DWORD(-1);
};

// initialize the static data memebers
ApartmentState* ApartmentState::apartment_state_ = 0;
int ApartmentState::ref_cnt_ = 0;

HRESULT ApartmentState::Initialize(DWORD reserved) {
  CORE_LOG(L3, (_T("[ApartmentState::Initialize]")));
  ASSERT(ref_cnt_ >= 0, (_T("Apartment Reference Counting")));
  if (ref_cnt_ < 0) return E_UNEXPECTED;

  DWORD thread_id = ::GetCurrentThreadId();
  ASSERT1(thread_id);

  if (ref_cnt_ > 0) {
    ASSERT(apartment_state(), (_T("Apartment State is 0.")));
    bool same_thread = thread_id == apartment_state()->thread_id();
    // if initialized multiple times verify the thread identity just in case
    ASSERT(same_thread, (_T("Wrong Thread.")));
    if (!same_thread) return E_UNEXPECTED;
    ++ref_cnt_;
    return S_OK;
  }

  ASSERT1(ref_cnt_ == 0);

  // do the initialization of the apartment
  scoped_ptr<CallDispatcher> call_disp(new CallDispatcher(reserved));
  RET_IF_FAILED(call_disp->Init());
  scoped_ptr<ApartmentState> ap_state(
    new ApartmentState(thread_id, call_disp.get()));

  call_disp.release();
  ApartmentState::apartment_state_ = ap_state.release();

  ++ref_cnt_;
  return S_OK;
}

HRESULT ApartmentState::Uninitialize() {
  ASSERT(ref_cnt_ > 0, (_T("Apartment Reference Counting")));
  if (ref_cnt_ <= 0) return E_UNEXPECTED;

  DWORD thread_id = ::GetCurrentThreadId();
  ASSERT1(thread_id);

  ASSERT(apartment_state(), (_T("Apartment State is 0.")));
  bool same_thread = thread_id == apartment_state()->thread_id();
  // verify the thread identity just in case
  ASSERT(same_thread, (_T("Wrong Thread.")));
  if (!same_thread) return E_UNEXPECTED;

  if (--ref_cnt_ == 0) {
    delete ApartmentState::apartment_state();
    ApartmentState::apartment_state_ = 0;
  }

  return S_OK;
}

CallDispatcher::CallDispatcher(DWORD options) : options_(options) {
  // TODO(omaha): Log
}

CallDispatcher::~CallDispatcher() {
  // TODO(omaha): Log
  if (m_hWnd) {
    DestroyWindow();
  }
}

HRESULT CallDispatcher::Init() {
  // Create a message-only window for the dispatcher. It is not visible,
  // has no z-order, cannot be enumerated, and does not receive broadcast
  // messages. The window simply dispatches messages.
  const TCHAR kWndName[] = _T("{FFE21900-612E-44a9-8424-3FC71B382E61}");
  HWND hwnd = Create(HWND_MESSAGE, NULL, kWndName);
  return hwnd ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
}

//
LRESULT CallDispatcher::OnMethodCall(UINT, WPARAM wParam,
                                     LPARAM result, BOOL&) {
  CORE_LOG(L6, (_T("[CallDispatcher::OnMethodCall]")));

  ASSERT1(wParam);
  BaseFunctor& call = *reinterpret_cast<BaseFunctor*>(wParam);

  // presult is non-zero if the method or function has a return type.
  // presult is zero for void methods and functions.

  void* presult = reinterpret_cast<void*>(result);

  ASSERT(
    ApartmentState::apartment_state()->thread_id() == ::GetCurrentThreadId(),
    (_T("Wrong Thread")));

  // the function object virtual call;
  call(presult);

  bool is_async = call.is_async();
  // For async calls, do not post a message, because the caller will not be
  // waiting for the call to complete.
  if (!is_async &&
      !::PostThreadMessage(call.thread_id(), WM_METHOD_CALL_COMPLETE, 0, 0)) {
    DWORD error = ::GetLastError();
    CORE_LOG(LEVEL_ERROR,
        (_T("[CallDispatcher::OnMethodCall - PostThreadMessage][%d]"), error));
    ASSERT(false, (_T("Failed to PostThreadMessage.")));

    // TODO(omaha): raise here.
  }

  // DO NOT ACCESS THE CALL OBJECT FROM DOWN ON. IN THE CASE OF A SYNCHRONOUS
  // CALL THE CALL OBJECT MAY HAVE ALREADY DESTROYED.

  if (is_async) {
    // Auto cleanup of the call object in the case of a async call.
    delete &call;
  }

  CORE_LOG(L6, (_T("CallDispatcher::OnMethodCall returns.")));
  return true;
}

//
HRESULT CallDispatcher::DoCrossApartmentCall(BaseFunctor* call,
                                             void* presult) {
  CORE_LOG(L6, (_T("[CallDispatcher::DoCrossApartmentCall]")));

  ASSERT(IsWindow(), (_T("The dispatcher must have a window.")));
  bool is_async = call->is_async();
  if (options_ == kTestingMode) {
    (*call)(presult);
    if (is_async) {
      // We need to delete the functor as if we were the callee.
      delete call;
    }
    return S_OK;
  }

  if (!is_async) {
    // Usually it is a mistake to call a synchronous method from the main STA
    // to the main STA.

    DWORD thread_id = ApartmentState::apartment_state()->thread_id();
    ASSERT(thread_id != ::GetCurrentThreadId(), (_T("Wrong Thread")));

    ASSERT(GetWindowThreadID() != ::GetCurrentThreadId(),
           (_T("DoCrossApartmentCall calling its own thread.")));
  }

  if (!PostMessage(WM_METHOD_CALL,
                   reinterpret_cast<WPARAM>(call),
                   reinterpret_cast<LPARAM>(presult))) {
    DWORD err = ::GetLastError();
    CORE_LOG(LEVEL_ERROR,
        (_T("[CallDispatcher::DoCrossApartmentCall - PostMessage][%d]"), err));
    ASSERT(false, (_T("Failed to PostMessage.")));

    return HRESULT_FROM_WIN32(err);
  }

  // Once the call has been made, do not access the state of the functor as
  // the other end might have already executed the call and delete the functor.
  // This is true for asyncronous calls but it would not hurt for synchronous
  // calls as well.
  call = NULL;

  if (is_async) {
    // Do not wait for the call to complete. The call will complete at
    // some time in the future and the call object is going to be cleaned up.
    return S_OK;
  }

  // Pump all messages, waiting for WM_METHOD_CALL_COMPLETE or WM_QUIT.
  MSG msg;
  SetZero(msg);
  int ret = 0;
  while ((ret = ::GetMessage(&msg, 0, 0, 0)) != 0) {
    if (ret == -1) {
      DWORD error = ::GetLastError();
      CORE_LOG(LEVEL_ERROR,
               (_T("[CallDispatcher::DoCrossApartmentCall - GetMessage][%d]"),
               error));
      // TODO(omaha): raise here.
    }

    if (msg.message == WM_METHOD_CALL_COMPLETE) {
      break;
    }

    ::DispatchMessage(&msg);
  }

  // Repost the WM_QUIT message to properly exit all message loops.
  if (msg.message == WM_QUIT) {
    ASSERT1(ret == 0);
    ::PostQuitMessage(msg.wParam);
  }

  CORE_LOG(L6, (_T("CallDispatcher::DoCrossApartmentCall returns")));
  return S_OK;
}

}  // namespace

void BaseFunctor::DoInvoke(void* presult) {
  ASSERT(ApartmentState::apartment_state(),
    (_T("Did you forgot to call 'InitializeApartment'?")));

  CallDispatcher& call_dispatcher =
    ApartmentState::apartment_state()->call_dispatcher();

  HRESULT hr = call_dispatcher.DoCrossApartmentCall(this, presult);

  if (FAILED(hr)) {
    ASSERT(false, (_T("Failed to call across apartments.")));
    // TODO(omaha): log, report, raise.
  }
}

HRESULT InitializeApartment(DWORD reserved) {
  return ApartmentState::Initialize(reserved);
}

HRESULT UninitializeApartment() {
  return ApartmentState::Uninitialize();
}

}  // namespace omaha

