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
// Defines two classes
// 1. class SingletonBase.
// 2. template class Singleton.
// Creation of singletons is a common
// activity. Use Singleton class to not
// repeat code every time.

#ifndef OMAHA_COMMON_SINGLETON_H_
#define OMAHA_COMMON_SINGLETON_H_

#include "omaha/base/debug.h"
#include "omaha/base/synchronized.h"


// Very important design pattern.
// Singleton class can be used in two ways.
// 1. Pass the class you want to make into Singleton as a
//    template parameter.
//
//    class SomeClass {
//     protected:
//    SomeClass(){}  // note - it is protected
//    ~SomeClass(){} // note - it is protected
//    public:
//    void Foo() {}
//  };
//
//   Singleton<SomeClass> s;
//   s::Instance()->Foo();
//
//     OR
// 2. You class can be derived from Singleton in a following way:
//    class SomeClass : public Singleton<SomeClass> {
//     protected:
//    SomeClass(){}
//    ~SomeClass(){}
//    public:
//    void Foo() {}
//  };
//
//   SomeClass::Instance()->Foo();
//
//  There is no requirement on the class you want to make into
//  Singleton except is has to have constructor that takes nothing.
//  As long as the class has void constructor it can become a singleton.
//  However if you want you class to be trully singleton you have to make
//  it constructors and destructors protected. Than you can only access your
//  class through the singlenot interface Instance().
//  If simple void constructor is not enough for you class, provide some kind of
//  initialization function, which could be called after the instance is
// created.

#define kSingletonMutexName               kLockPrefix L"Singleton_Creation_Lock"

#ifdef _DEBUG
  #define InstanceReturnTypeDeclaration SingletonProxy<T>
  #define InstanceReturnTypeStatement   SingletonProxy<T>
#else
  #define InstanceReturnTypeDeclaration T*
  #define InstanceReturnTypeStatement
#endif

template <typename T> class Singleton  {
  // Caching pointers to Singletons is very dangerous and goes against
  // Singleton philosophy. So we will return proxy from instance in Debug mode.
  // In release mode we will not go this route for efficiency.
  template <typename T> class SingletonProxy {
    T* data_;
  public:
    explicit SingletonProxy(T* data) : data_(data) {}
    T* operator->() const  {
      return data_;
    }
    SingletonProxy& operator=(const SingletonProxy&);
  };

 public:
  Singleton() {}

  // Use double-check pattern for efficiency.
  // TODO(omaha): the pattern is broken on multicore.
  static InstanceReturnTypeDeclaration Instance() {
    if(instance_ == NULL) {
      // We use GLock here since LLock will not give us synchronization and
      // SimpleLock will create deadlock if one singleton is created in the
      // constructor of the other singleton.
      GLock creation_lock;
      TCHAR mutex_name[MAX_PATH] = {0};
      wsprintf(mutex_name, L"%s%d",
               kSingletonMutexName, ::GetCurrentProcessId());

      VERIFY1(creation_lock.Initialize(mutex_name));
      __mutexScope(creation_lock);
      if(instance_ == NULL)
         instance_ = GetInstance();
    }
    return InstanceReturnTypeStatement(instance_);
  }

 private:
  static T* GetInstance() {
    static MyT my_t;
    return &my_t;
  }

  // shared between the same type T.
  static T * instance_;

  // Needed to access the protected constructor
  // of a client.
  class MyT : public T {
  };
};

// This instance_ is shared between template of the same type.
template <typename T>  T* Singleton<T>::instance_ = NULL;

#endif  // OMAHA_COMMON_SINGLETON_H_
