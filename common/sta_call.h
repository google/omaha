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
// sta_call.h generics for cross apartment calling.
//
// The code is using compile-time and run-time polymorphism to create
// type-safe call wrappers that can be used to cross call from a
// worker thread to an STA thread. The current implementation only
// supports calling the main STA.
//
// Functions as well as object methods can be called.
//
// Examples:
// class X {
//   public:
//     int Add(int i, int j) { return i + j; }
// };
//
//
// namespace {
//   int Add(long i, long j) { return i + j; }
// }
//
// X x;
// int sum = CallMethod(&x, X::Add, 10, 20);
// int j = CallFunction(Add, -10, 10);
//
// The central piece of machinery is a hierarchy of functors. A functor is
// instantiated by a function template (CallFunction or CallMethod) and its
// 'Invoke' method gets called.
// Calling 'Invoke' will send the functor using 'SendMessage'
// to a window. The window message handler picks up the functor and
// calls the functor virtual operator().
// This virtual call is what actually calls the specified function or the
// method, the only difference being that the call is now made in a thread
// different than the thread that called 'Invoke'. There is a partial
// specialization of the templates for void, so that void type is supported
// as a return type.
//
//
// !!! Limitations !!!
//
// There are a few important design and implementation limitations. They are
// mostly related to template parameters ambiguities (T or T&) especially
// for overloaded names or const types. The limitations are significant although
// the code is useful enough as it is in most of the cases.
// However, when using the code it is frustrating to discover that it does not
// compile for obvious and useful cases, a constant reminder that a better
// solution is to be seeked.
//
//
// The implementation does not support calling all 'stdcall' calling convention.
//
// The design does not support calling functions or methods that use pass by
// reference arguments: f(std::string&) .
//
// The design does not support well calling functions or methods that take
// pointer to const types parameters : f(const std::string*) .
//
// The implementation does not support calling methods of const objects.
//
// To reduce the number of templates that get instantiated, the types of the
// arguments of the call must match exactly the types of parameters of the
// function or method . In some cases static_casts mey be required
// at the point of the call. Example: CallMethod(f, static_cast<long>(10));

#ifndef OMAHA_COMMON_STA_CALL_H__
#define OMAHA_COMMON_STA_CALL_H__

#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"

namespace omaha {

// C4347: 'function template' is called instead of 'function'
#pragma warning(disable : 4347)

// The Base Functor is the base of the functor hierarchy.
class BaseFunctor {
 public:
  explicit BaseFunctor(bool is_async) :
      thread_id_(::GetCurrentThreadId()),
      is_async_(is_async) {
    CORE_LOG(L6, (_T("[BaseFunctor::BaseFunctor]")));
  }

  // Functors are polymorphic objects.
  virtual ~BaseFunctor() {
    CORE_LOG(L6, (_T("[BaseFunctor::~BaseFunctor]")));
  }

  // Abstract virtual function call operator. This is always called
  // in the callee thread by the dispatcher of the apartment.
  virtual void operator()(void* presult) = 0;

  // The thread id of the calling thread.
  DWORD thread_id() const { return thread_id_; }

  bool is_async() const { return is_async_; }

 protected:

  // Invoke is called by each of the derived functors. This is how
  // the  cross thread invocation is made and the result of the invocation
  // is retrieved. Invoke is always called in the caller thread.
  template <typename R>
  R Invoke() {
    R r = R();      // ensure r is initialized even for primitive types.
    if (!is_async_) {
      DoInvoke(&r);
    } else {
      // We handle the async calls as if the call returns void.
      DoInvoke(0);
    }
    return r;
  }

  // non-template method to be called by the derived functors
  // specialized for void.
  void Invoke() {
    // When the argument of the invocation is 0, we are not
    // interested in the result.
    DoInvoke(0);
  }

 private:
  void DoInvoke(void* presult);   // Does the actual invocation.
  DWORD thread_id_;               // The thread id of the calling thread.
  bool is_async_;                 // True for async calls.

  DISALLOW_EVIL_CONSTRUCTORS(BaseFunctor);
};

//
// 0-ary method functor.
//
template <class T, typename R>
class MethodFunctor0 : public BaseFunctor {
 public:
  MethodFunctor0(bool is_async, T* pt, R (T::*pm)()) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    if (presult) {
      *static_cast<R*>(presult) = (pobj_->*pm_)();
    } else {
      (pobj_->*pm_)();
    }
  }

  R Invoke() {
    // Don't forget to call the base implementation.
    return BaseFunctor::Invoke<R>();
  }

 private:
  T* pobj_;
  R (T::*pm_)();
};

//
// 0-ary partial specialization for void return types.
//
template <class T>
class MethodFunctor0<T, void> : public BaseFunctor {
 public:
  MethodFunctor0(bool is_async, T* pt, void (T::*pm)()) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    // the actual call. There is no return value when the return type is void.
    (pobj_->*pm_)();
  }

  // Bring in the name from the Base
  using BaseFunctor::Invoke;

 private:
  T* pobj_;
  void (T::*pm_)();
};

//
// 0-ary functor and specialization for void.
//
template <typename R>
class Functor0 : public BaseFunctor {
 public:
  Functor0(bool is_async, R (*pf)()) :
      BaseFunctor(is_async), pf_(pf) {}

  virtual void operator()(void* presult) {
    if (presult) {
      *static_cast<R*>(presult) = (*pf_)();
    } else {
      (*pf_)();
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  R (*pf_)();
};

template <>
class Functor0<void> : public BaseFunctor {
 public:
  Functor0(bool is_async, void (*pf)()) :
      BaseFunctor(is_async), pf_(pf) {}

  virtual void operator()(void* presult) {
    ASSERT1(!presult);
    presult;  // unreferenced formal parameter

    (*pf_)();
  }

  using BaseFunctor::Invoke;

 private:
  void (*pf_)();
};


//
// 1-ary
//
template <class T, typename R, typename P>
class MethodFunctor1 : public BaseFunctor {
 public:
  MethodFunctor1(bool is_async, T* pt, R (T::*pm)(P), P p) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm), p_(p) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    if (presult) {
      *static_cast<R*>(presult) = (pobj_->*pm_)(p_);
    } else {
      (pobj_->*pm_)(p_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  T* pobj_;
  R (T::*pm_)(P);
  P p_;
};

template <class T, typename P>
class MethodFunctor1<T, void, P> : public BaseFunctor {
 public:
  MethodFunctor1(bool is_async, T* pt, void (T::*pm)(P), P p) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm), p_(p) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (pobj_->*pm_)(p_);
  }

  using BaseFunctor::Invoke;

 private:
  T* pobj_;
  void (T::*pm_)(P);
  P p_;
};

template <typename R, typename P1>
class Functor1 : public BaseFunctor {
 public:
  Functor1(bool is_async, R (*pf)(P1), P1 p1) :
      BaseFunctor(is_async), pf_(pf), p1_(p1) {}

  virtual void operator()(void* presult) {
    if (presult) {
      *static_cast<R*>(presult) = (*pf_)(p1_);
    } else {
      (*pf_)(p1_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  R (*pf_)(P1);
  P1 p1_;
};

template <typename P1>
class Functor1<void, P1> : public BaseFunctor {
 public:
  Functor1(bool is_async, void (*pf)(P1), P1 p1) :
      BaseFunctor(is_async), pf_(pf), p1_(p1) {}

  virtual void operator()(void* presult) {
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (*pf_)(p1_);
  }

  using BaseFunctor::Invoke;

 private:
  void (*pf_)(P1);
  P1 p1_;
};


//
// 2-ary
//
template <class T, typename R, typename P1, typename P2>
class MethodFunctor2 : public BaseFunctor {
 public:
  MethodFunctor2(bool is_async, T* pt, R (T::*pm)(P1, P2), P1 p1, P2 p2) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm), p1_(p1), p2_(p2) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    if (presult) {
      *static_cast<R*>(presult) = (pobj_->*pm_)(p1_, p2_);
    } else {
      (pobj_->*pm_)(p1_, p2_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  T* pobj_;
  R (T::*pm_)(P1, P2);
  P1 p1_;
  P2 p2_;
};

template <class T, typename P1, typename P2>
class MethodFunctor2<T, void, P1, P2> : public BaseFunctor {
 public:
  MethodFunctor2(bool is_async, T* pt, void (T::*pm)(P1, P2), P1 p1, P2 p2) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm), p1_(p1), p2_(p2) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (pobj_->*pm_)(p1_, p2_);
  }

  using BaseFunctor::Invoke;

 private:
  T* pobj_;
  void (T::*pm_)(P1, P2);
  P1 p1_;
  P2 p2_;
};

template <typename R, typename P1, typename P2>
class Functor2 : public BaseFunctor {
 public:
  Functor2(bool is_async, R (*pf)(P1, P2), P1 p1, P2 p2) :
      BaseFunctor(is_async), pf_(pf), p1_(p1), p2_(p2) {}

  virtual void operator()(void* presult) {
    if (presult) {
      *static_cast<R*>(presult) = pf_(p1_, p2_);
    } else {
      pf_(p1_, p2_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  R (*pf_)(P1, P2);
  P1 p1_;
  P2 p2_;
};

template <typename P1, typename P2>
class Functor2<void, P1, P2> : public BaseFunctor {
 public:
  Functor2(bool is_async, void (*pf)(P1, P2), P1 p1, P2 p2) :
      BaseFunctor(is_async), pf_(pf), p1_(p1), p2_(p2) {}

  virtual void operator()(void* presult) {
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (*pf_)(p1_, p2_);
  }

  using BaseFunctor::Invoke;

 private:
  void (*pf_)(P1, P2);
  P1 p1_;
  P2 p2_;
};

//
// 3-ary
//
template <class T, typename R, typename P1, typename P2, typename P3>
class MethodFunctor3 : public BaseFunctor {
 public:
  MethodFunctor3(bool is_async,
                 T* pt,
                 R (T::*pm)(P1, P2, P3),
                 P1 p1,
                 P2 p2,
                 P3 p3) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm), p1_(p1), p2_(p2), p3_(p3) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    if (presult) {
      *static_cast<R*>(presult) = (pobj_->*pm_)(p1_, p2_, p3_);
    } else {
      (pobj_->*pm_)(p1_, p2_, p3_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  T* pobj_;
  R (T::*pm_)(P1, P2, P3);
  P1 p1_;
  P2 p2_;
  P3 p3_;
};

template <class T, typename P1, typename P2, typename P3>
class MethodFunctor3<T, void, P1, P2, P3> : public BaseFunctor {
 public:
  MethodFunctor3(bool is_async,
                 T* pt,
                 void (T::*pm)(P1, P2, P3),
                 P1 p1,
                 P2 p2,
                 P3 p3) :
      BaseFunctor(is_async), pobj_(pt), pm_(pm), p1_(p1), p2_(p2), p3_(p3) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (pobj_->*pm_)(p1_, p2_, p3_);
  }

  using BaseFunctor::Invoke;

 private:
  T* pobj_;
  void (T::*pm_)(P1, P2, P3);
  P1 p1_;
  P2 p2_;
  P3 p3_;
};


template <typename R, typename P1, typename P2, typename P3>
class Functor3 : public BaseFunctor {
 public:
  Functor3(bool is_async, R (*pf)(P1, P2, P3), P1 p1, P2 p2, P3 p3) :
      BaseFunctor(is_async), pf_(pf), p1_(p1), p2_(p2), p3_(p3) {}
  virtual void operator()(void* presult) {
    if (presult) {
      *static_cast<R*>(presult) = (*pf_)(p1_, p2_, p3_);
    } else {
      (*pf_)(p1_, p2_, p3_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  R (*pf_)(P1, P2, P3);
  P1 p1_;
  P2 p2_;
  P3 p3_;
};

template <typename P1, typename P2, typename P3>
class Functor3<void, P1, P2, P3> : public BaseFunctor {
 public:
  Functor3(bool is_async, void (*pf)(P1, P2, P3), P1 p1, P2 p2, P3 p3) :
      BaseFunctor(is_async), pf_(pf), p1_(p1), p2_(p2), p3_(p3) {}

  virtual void operator()(void* presult) {
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (*pf_)(p1_, p2_, p3_);
  }

  using BaseFunctor::Invoke;

 private:
  void (*pf_)(P1, P2, P3);
  P1 p1_;
  P2 p2_;
  P3 p3_;
};

//
// 4-ary
//
template <class T,
          typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4>
class MethodFunctor4 : public BaseFunctor {
 public:
  MethodFunctor4(bool is_async,
                 T* pt,
                 R (T::*pm)(P1, P2, P3, P4),
                 P1 p1,
                 P2 p2,
                 P3 p3,
                 P4 p4) :
      BaseFunctor(is_async),
      pobj_(pt),
      pm_(pm),
      p1_(p1),
      p2_(p2),
      p3_(p3),
      p4_(p4) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    if (presult) {
      *static_cast<R*>(presult) = (pobj_->*pm_)(p1_, p2_, p3_, p4_);
    } else {
      (pobj_->*pm_)(p1_, p2_, p3_, p4_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  T* pobj_;
  R (T::*pm_)(P1, P2, P3, P4);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
};

template <class T, typename P1, typename P2, typename P3, typename P4>
class MethodFunctor4<T, void, P1, P2, P3, P4> : public BaseFunctor {
 public:
  MethodFunctor4(bool is_async,
                 T* pt,
                 void (T::*pm)(P1, P2, P3, P4),
                 P1 p1,
                 P2 p2,
                 P3 p3,
                 P4 p4) :
      BaseFunctor(is_async),
      pobj_(pt),
      pm_(pm),
      p1_(p1),
      p2_(p2),
      p3_(p3),
      p4_(p4) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (pobj_->*pm_)(p1_, p2_, p3_, p4_);
  }

  using BaseFunctor::Invoke;

 private:
  T* pobj_;
  void (T::*pm_)(P1, P2, P3, P4);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
};


template <typename R, typename P1, typename P2, typename P3, typename P4>
class Functor4 : public BaseFunctor {
 public:
  Functor4(bool is_async, R (*pf)(P1, P2, P3, P4), P1 p1, P2 p2, P3 p3, P4 p4) :
      BaseFunctor(is_async), pf_(pf), p1_(p1), p2_(p2), p3_(p3), p4_(p4) {}

  virtual void operator()(void* presult) {
    if (presult) {
      *static_cast<R*>(presult) = (*pf_)(p1_, p2_, p3_, p4_);
    } else {
      (*pf_)(p1_, p2_, p3_, p4_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  R (*pf_)(P1, P2, P3, P4);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
};

template <typename P1, typename P2, typename P3, typename P4>
class Functor4<void, P1, P2, P3, P4> : public BaseFunctor {
 public:
  Functor4(bool is_async,
           void (*pf)(P1, P2, P3, P4),
           P1 p1,
           P2 p2,
           P3 p3,
           P4 p4) :
      BaseFunctor(is_async), pf_(pf), p1_(p1), p2_(p2), p3_(p3), p4_(p4) {}

  virtual void operator()(void* presult) {
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (*pf_)(p1_, p2_, p3_, p4_);
  }

  using BaseFunctor::Invoke;

 private:
  void (*pf_)(P1, P2, P3, P4);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
};

//
// 5-ary
//
template <class T,
          typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
class MethodFunctor5 : public BaseFunctor {
 public:
  MethodFunctor5(bool is_async,
                 T* pt,
                 R (T::*pm)(P1, P2, P3, P4, P5),
                 P1 p1,
                 P2 p2,
                 P3 p3,
                 P4 p4,
                 P5 p5) :
      BaseFunctor(is_async),
      pobj_(pt),
      pm_(pm),
      p1_(p1),
      p2_(p2),
      p3_(p3),
      p4_(p4),
      p5_(p5) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    if (presult) {
      *static_cast<R*>(presult) = (pobj_->*pm_)(p1_, p2_, p3_, p4_, p5_);
    } else {
      (pobj_->*pm_)(p1_, p2_, p3_, p4_, p5_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  T* pobj_;
  R (T::*pm_)(P1, P2, P3, P4, P5);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
  P5 p5_;
};

template <class T,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
class MethodFunctor5<T, void, P1, P2, P3, P4, P5> : public BaseFunctor {
 public:
  MethodFunctor5(bool is_async,
                 T* pt,
                 void (T::*pm)(P1, P2, P3, P4, P5),
                 P1 p1,
                 P2 p2,
                 P3 p3,
                 P4 p4,
                 P5 p5) :
      BaseFunctor(is_async),
      pobj_(pt),
      pm_(pm),
      p1_(p1),
      p2_(p2),
      p3_(p3),
      p4_(p4),
      p5_(p5) {}

  virtual void operator()(void* presult) {
    ASSERT(pobj_, (_T("Null object.")));
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (pobj_->*pm_)(p1_, p2_, p3_, p4_, p5_);
  }

  using BaseFunctor::Invoke;

 private:
  T* pobj_;
  void (T::*pm_)(P1, P2, P3, P4, P5);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
  P5 p5_;
};

template <typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
class Functor5 : public BaseFunctor {
 public:
  Functor5(bool is_async,
           R (*pf)(P1, P2, P3, P4, P5),
           P1 p1,
           P2 p2,
           P3 p3,
           P4 p4,
           P5 p5) :
      BaseFunctor(is_async),
      pf_(pf),
      p1_(p1),
      p2_(p2),
      p3_(p3),
      p4_(p4),
      p5_(p5) {}
  virtual void operator()(void* presult) {
    if (presult) {
      *static_cast<R*>(presult) = (*pf_)(p1_, p2_, p3_, p4_, p5_);
    } else {
      (*pf_)(p1_, p2_, p3_, p4_, p5_);
    }
  }

  R Invoke() {
    return BaseFunctor::Invoke<R>();
  }

 private:
  R (*pf_)(P1, P2, P3, P4, P5);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
  P5 p5_;
};

template <typename P1, typename P2, typename P3, typename P4, typename P5>
class Functor5<void, P1, P2, P3, P4, P5> : public BaseFunctor {
 public:
  Functor5(bool is_async,
           void (*pf)(P1, P2, P3, P4, P5),
           P1 p1,
           P2 p2,
           P3 p3,
           P4 p4,
           P5 p5) :
      BaseFunctor(is_async),
      pf_(pf),
      p1_(p1),
      p2_(p2),
      p3_(p3),
      p4_(p4),
      p5_(p5) {}

  virtual void operator()(void* presult) {
    ASSERT1(!presult);
    presult;  //  unreferenced formal parameter

    (*pf_)(p1_, p2_, p3_, p4_, p5_);
  }

  using BaseFunctor::Invoke;

 private:
  void (*pf_)(P1, P2, P3, P4, P5);
  P1 p1_;
  P2 p2_;
  P3 p3_;
  P4 p4_;
  P5 p5_;
};


// This is what the clients of the STA code instantiate and call.
//
// Synchronous Callers.
//
template <class T, typename R>
R CallMethod(T* object, R (T::*pm)()) {
  return MethodFunctor0<T, R>(false, object, pm).Invoke();
}

template <typename R>
R CallFunction(R (*pf)()) {
  return Functor0<R>(false, pf).Invoke();
}

template <class T, typename R, typename P>
R CallMethod(T* object, R (T::*pm)(P), P p) {
  return MethodFunctor1<T, R, P>(false, object, pm, p).Invoke();
}

template <typename R, typename P>
R CallFunction(R (*pf)(P), P p) {
  return Functor1<R, P>(false, pf, p).Invoke();
}

template <class T, typename R, typename P1, typename P2>
R CallMethod(T* object, R (T::*pm)(P1, P2), P1 p1, P2 p2) {
  return MethodFunctor2<T, R, P1, P2>(false, object, pm, p1, p2).Invoke();
}

template <typename R, typename P1, typename P2>
R CallFunction(R (*pf)(P1, P2), P1 p1, P2 p2) {
  return Functor2<R, P1, P2>(false, pf, p1, p2).Invoke();
}

template <class T, typename R, typename P1, typename P2, typename P3>
R CallMethod(T* object, R (T::*pm)(P1, P2, P3), P1 p1, P2 p2, P3 p3) {
  return MethodFunctor3<T, R, P1, P2, P3>(false,
                                          object, pm, p1, p2, p3).Invoke();
}

template <typename R, typename P1, typename P2, typename P3>
R CallFunction(R (*pf)(P1, P2, P3), P1 p1, P2 p2, P3 p3) {
  return Functor3<R, P1, P2, P3>(false, pf, p1, p2, p3).Invoke();
}

template <class T,
          typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4>
R CallMethod(T* object,
             R (T::*pm)(P1, P2, P3, P4),
             P1 p1,
             P2 p2,
             P3 p3,
             P4 p4) {
  return MethodFunctor4<T, R, P1, P2, P3, P4>(false,
                                              object,
                                              pm,
                                              p1,
                                              p2,
                                              p3,
                                              p4).Invoke();
}

template <typename R, typename P1, typename P2, typename P3, typename P4>
R CallFunction(R (*pf)(P1, P2, P3, P4), P1 p1, P2 p2, P3 p3, P4 p4) {
  return Functor4<R, P1, P2, P3, P4>(false, pf, p1, p2, p3, p4).Invoke();
}

template <class T,
          typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
R CallMethod(T* object,
             R (T::*pm)(P1, P2, P3, P4, P5),
             P1 p1,
             P2 p2,
             P3 p3,
             P4 p4,
             P5 p5) {
  return MethodFunctor5<T, R, P1, P2, P3, P4, P5>(false,
                                                  object,
                                                  pm,
                                                  p1,
                                                  p2,
                                                  p3,
                                                  p4,
                                                  p5).Invoke();
}

template <typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
R CallFunction(R (*pf)(P1, P2, P3, P4, P5), P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
  return Functor5<R, P1, P2, P3, P4, P5>(false,
                                         pf,
                                         p1,
                                         p2,
                                         p3,
                                         p4,
                                         p5).Invoke();
}

//
// Asynchronous Callers.
//
template <class T, typename R>
void CallMethodAsync(T* object, R (T::*pm)()) {
  scoped_ptr<MethodFunctor0<T, R> > fun(
      new MethodFunctor0<T, R>(true, object, pm));
  fun->Invoke();
  fun.release();
}

template <typename R>
void CallFunctionAsync(R (*pf)()) {
  scoped_ptr<Functor0<R> > fun(new Functor0<R>(true, pf));
  fun->Invoke();
  fun.release();
}

template <class T, typename R, typename P>
void CallMethodAsync(T* object, R (T::*pm)(P), P p) {
  scoped_ptr<MethodFunctor1<T, R, P> > fun(
      new MethodFunctor1<T, R, P>(true, object, pm, p));
  fun->Invoke();
  fun.release();
}

template <typename R, typename P>
void CallFunctionAsync(R (*pf)(P), P p) {
  scoped_ptr<Functor1<R, P> > fun(new Functor1<R, P>(true, pf, p));
  fun->Invoke();
  fun.release();
}

template <class T, typename R, typename P1, typename P2>
void CallMethodAsync(T* object, R (T::*pm)(P1, P2), P1 p1, P2 p2) {
  scoped_ptr<MethodFunctor2<T, R, P1, P2> > fun(
      new MethodFunctor2<T, R, P1, P2>(true, object, pm, p1, p2));
  fun->Invoke();
  fun.release();
}

template <typename R, typename P1, typename P2>
void CallFunctionAsync(R (*pf)(P1, P2), P1 p1, P2 p2) {
  scoped_ptr<Functor2<R, P1, P2> > fun(
      new Functor2<R, P1, P2>(true, pf, p1, p2));
  fun->Invoke();
  fun.release();
}

template <class T, typename R, typename P1, typename P2, typename P3>
void CallMethodAsync(T* object, R (T::*pm)(P1, P2, P3), P1 p1, P2 p2, P3 p3) {
  scoped_ptr<MethodFunctor3<T, R, P1, P2, P3> > fun(
      new MethodFunctor3<T, R, P1, P2, P3>(true, object, pm, p1, p2, p3));
  fun->Invoke();
  fun.release();
}

template <typename R, typename P1, typename P2, typename P3>
void CallFunctionAsync(R (*pf)(P1, P2, P3), P1 p1, P2 p2, P3 p3) {
  scoped_ptr<Functor3<R, P1, P2, P3> > fun(
      new Functor3<R, P1, P2, P3>(true, pf, p1, p2, p3));
  fun->Invoke();
  fun.release();
}

template <class T,
          typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4>
void CallMethodAsync(T* obj,
                     R (T::*pm)(P1, P2, P3, P4),
                     P1 p1,
                     P2 p2,
                     P3 p3,
                     P4 p4) {
  scoped_ptr<MethodFunctor4<T, R, P1, P2, P3, P4> > fun(
      new MethodFunctor4<T, R, P1, P2, P3, P4>(true, obj, pm, p1, p2, p3, p4));
  fun->Invoke();
  fun.release();
}

template <typename R, typename P1, typename P2, typename P3, typename P4>
void CallFunctionAsync(R (*pf)(P1, P2, P3, P4), P1 p1, P2 p2, P3 p3, P4 p4) {
  scoped_ptr<Functor4<R, P1, P2, P3, P4> > fun(
      new Functor4<R, P1, P2, P3, P4>(true, pf, p1, p2, p3, p4));
  fun->Invoke();
  fun.release();
}

template <class T,
          typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
void CallMethodAsync(T* object,
                     R (T::*pm)(P1, P2, P3, P4, P5),
                     P1 p1,
                     P2 p2,
                     P3 p3,
                     P4 p4,
                     P5 p5) {
  scoped_ptr<MethodFunctor5<T, R, P1, P2, P3, P4, P5> > fun(
      new MethodFunctor5<T, R, P1, P2, P3, P4, P5>(true,
                                                   object,
                                                   pm,
                                                   p1,
                                                   p2,
                                                   p3,
                                                   p4,
                                                   p5));
  fun->Invoke();
  fun.release();
}

template <typename R,
          typename P1,
          typename P2,
          typename P3,
          typename P4,
          typename P5>
void CallFunctionAsync(R (*pf)(P1, P2, P3, P4, P5),
                       P1 p1,
                       P2 p2,
                       P3 p3,
                       P4 p4,
                       P5 p5) {
  scoped_ptr<Functor5<R, P1, P2, P3, P4, P5> > fun(
      new Functor5<R, P1, P2, P3, P4, P5>(true, pf, p1, p2, p3, p4, p5));
  fun->Invoke();
  fun.release();
}

#pragma warning(default : 4347)

}  // namespace omaha

#endif  // OMAHA_COMMON_STA_CALL_H__

