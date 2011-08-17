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
// Defines template class used to share data
// between processes.
//

#ifndef OMAHA_COMMON_SHARED_MEMORY_PTR_H__
#define OMAHA_COMMON_SHARED_MEMORY_PTR_H__

#include "base/debug.h"
#include "base/singleton.h"
#include "base/system_info.h"
#include "base/vista_utils.h"

namespace omaha {

// SharedMemoryPtr class is designed to allow seamless data sharing process boundaries.
// All the data passed as a template parameter will be shared between processes.
// Very important to remember that for now - all shared data should be on stack.
// For example if the class A had stl vector as a member, the members of the vector
// would be allocated not from shared memory and therefore will not be shared.
// That could be solved with allocators, but for now we don't need that.
//
// Here is a typical example of usage:
//   Class A {
//    int i_;
//    double d_;
//    .......
//    ........
//
//    public:
//    set_double(double d){d_=d;}
//    double get_double(){return d_};
//
// };
//
// ... Prosess one...
//    SharedMemoryPtr<A> spA("ABC");
//    if (!spA)
//      return false;
//
//    spA->set_double(3.14);
//
//  ... Process two ...
//
//
//    SharedMemoryPtr<A> spA1("ABC");
//    if (!spA1)
//      return false;
//
//    process two will see the value set by process one.
//    it will be 3.14
//    double d = spA1->get_double();
//

// You should implement a class member of SystemSharedData if the data you want
// to share is several hundred bytes. Always try this approach first before you implement
// new class that is derived from SharedMemoryPtr. The main difference is that SharedMemoryPtr
// will allocate a least page of shared memory. If your class is just a member of SystemSharedData
// memory mapped file will be shared between members. It is just more efficient.
// Look in system_shared_data.h , shared_data_member.h, and system_shared_data_members.h
// for more details.

// Forward declaration.
template <typename LockType, typename T> class SharedMemoryPtr;

// During several code reviews it has been noticed that the same error gets repeated over and over.
// People create SharedMemoryPtr<SomeData>. And than the access to member functions of SomeData
// is not synchronized by __mutexBlock or __mutexScope. So we need to somehow find a way to make
// automatic syncronization whenever people access shared data methods or  members.
// Since by design the only way we can acess shared data is through operator -> of SharedMemoryPtr
// we need to somehow invoke synchronization at the time of access.
// We can implement this mainly because of the mechanics of operator-> dictated by C++ standard.
// When you apply operator-> to a type that's not a built-in pointer, the compiler does an interesting thing.
// After looking up and applying the user-defined operator-> to that type, it applies operator-> again to the result.
// The compiler keeps doing this recursively until it reaches a pointer to a built-in type, and only then proceeds with member access.
// It follows that a SharedMemoryPtr<T> operator-> does not have to return a pointer.
// It can return an object that in turn implements operator->, without changing the use syntax.
// So we can implement: pre- and postfunction calls. (See Stroustrup 2000)
// If you return an object of some type X
// by value from operator->, the sequence of execution is as follows:
// 1. Constructor of type X
// 2. X::operator-> called;  returns a pointer to an object of type T of SharedMemoryPtr
// 3. Member access
// 4. Destructor of X
// In a nutshell, we have a  way of implementing locked function calls.

template <typename LockType, typename T>
class SharedDataLockingProxy {
public:
  // Lock on construction.
  SharedDataLockingProxy(SharedMemoryPtr<LockType, T> * mem_ptr, T* shared_data)
    : mem_ptr_(mem_ptr), shared_data_(shared_data) {
      mem_ptr_->Lock();
    }
   // Unlock on destruction.
  ~SharedDataLockingProxy() {
    mem_ptr_->Unlock();
  }
  // operator
  T* operator->() const  {
    ASSERT(shared_data_ != NULL, (L"NULL object pointer being dereferenced"));
    return shared_data_;
  }
private:
  SharedDataLockingProxy& operator=(const SharedDataLockingProxy&);
  SharedMemoryPtr<LockType, T>* mem_ptr_;
  T* shared_data_;
  // To allow this implicit locking - copy constructor must be
  // enabled. hence, no DISALLOW_EVIL_CONSTRUCTORS
};

template <typename LockType, typename T> class SharedMemoryPtr
    : public LockType {
  // Handle to disk file if we're backing this shared memory by a file
  HANDLE file_;
  // Local handle to file mapping.
  HANDLE file_mapping_;
  // pointer to a view.
  T*     data_;
  // If the first time creation can do some initialization.
  bool   first_instance_;
public:
  // The heart of the whole idea. Points to shared memrory
  // instead of the beginning of the class.
  SharedDataLockingProxy<LockType, T> operator->() {
    return SharedDataLockingProxy<LockType, T>(this, data_);
  }
  // To check after creation.
  // For example:
  // SharedMemoryPtr<GLock, SomeClass> sm;
  // if (sm)
  // { do whatever you want}
  // else
  // {error reporting}
  operator bool() const {return ((file_mapping_ != NULL) && (data_ != NULL));}

  // Initialize memory mapped file and sync mechanics.
  // by calling InitializeSharedAccess
  SharedMemoryPtr(const CString& name,
                  LPSECURITY_ATTRIBUTES sa,
                  LPSECURITY_ATTRIBUTES sa_mutex,
                  bool read_only)
      : file_(INVALID_HANDLE_VALUE),
        file_mapping_(NULL),
        data_(NULL) {
    HRESULT hr = InitializeSharedAccess(name, false, sa, sa_mutex, read_only);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("InitializeSharedAccess failed [%s][%s][0x%x]"),
                    name, read_only ? _T("R") : _T("RW"), hr));
    }
  }

  // Use this constructor if you want to back the shared memory by a file.
  // NOTE: if using a persistent shared memory, every object with this same
  // name should be persistent. Otherwise, the objects marked as
  // non-persistent will lead to InitializeSharedData called again if
  // they are instantiated before the ones marked as persistent.
  SharedMemoryPtr(bool persist,
                  LPSECURITY_ATTRIBUTES sa,
                  LPSECURITY_ATTRIBUTES sa_mutex,
                  bool read_only)
      : file_(INVALID_HANDLE_VALUE),
        file_mapping_(NULL),
        data_(NULL) {
    // Each shared data must implement GetFileName() to use this c-tor. The
    // implementation should be:
    // const CString GetFileName() const {return L"C:\\directory\file";}
    // This is purposedly different from GetSharedName, so that the user is
    // well aware that a file name is expected, not a mutex name.
    HRESULT hr = InitializeSharedAccess(data_->GetFileName(),
                                        persist,
                                        sa,
                                        sa_mutex,
                                        read_only);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("InitializeSharedAccess failed [%s][%s][0x%x]"),
                    data_->GetFileName(), read_only ? _T("R") : _T("RW"), hr));
    }
  }

  // Initialize memory mapped file and sync mechanics.
  // by calling InitializeSharedAccess
  SharedMemoryPtr() :
      file_(INVALID_HANDLE_VALUE), file_mapping_(NULL), data_(NULL) {
    // This should never happen but let's assert
    // in case it does.
    // Each shared data must implement GetSharedData() to use this c-tor.
    // The implementation should be:
    // const TCHAR * GetSharedName() const
    //     {return L"Some_unique_string_with_no_spaces";}
    HRESULT hr = InitializeSharedAccess(data_->GetSharedName(),
                                        false,
                                        NULL,
                                        NULL,
                                        false);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("InitializeSharedAccess failed [%s][%s][0x%x]"),
                    data_->GetSharedName(), _T("RW"), hr));
    }
  }

  // Clean up.
  ~SharedMemoryPtr() {
    Cleanup();
  }

  void Cleanup() {
    __mutexScope(this);
    if (data_)
      UnmapViewOfFile(data_);
    if (file_mapping_)
      VERIFY(CloseHandle(file_mapping_), (L""));
    if (file_ != INVALID_HANDLE_VALUE)
      VERIFY(CloseHandle(file_), (L""));
  }

  // Initialize memory mapped file and sync object.
  bool InitializeSharedAccess(const CString& name,
                              bool persist,
                              LPSECURITY_ATTRIBUTES sa,
                              LPSECURITY_ATTRIBUTES sa_mutex,
                              bool read_only) {
    return InitializeSharedAccessInternal(name,
                                          persist,
                                          sa,
                                          sa_mutex,
                                          read_only,
                                          sizeof(T),
                                          &T::InitializeSharedData);
  }

 private:
  // Initialize memory mapped file and sync object.
  //
  // This internal method allows template method folding by only using things
  // that are consistent in all templates.  Things that vary are passed in.
  bool InitializeSharedAccessInternal(const CString& name, bool persist,
                                      LPSECURITY_ATTRIBUTES sa,
                                      LPSECURITY_ATTRIBUTES sa_mutex,
                                      bool read_only,
                                      size_t data_size,
                                      void (T::*initialize_shared_data)
                                          (const CString&)) {
    // If this memory mapped object is backed by a file, then "name" is a fully
    // qualified name with backslashes. Since we can't use backslashes in a
    // mutex's name, let's make another name where we convert them to
    // underscores.
    CString mem_name(name);
    if (persist) {
      mem_name.Replace(_T('\\'), _T('_'));
    }

    // Initialize the mutex
    CString mutex_name(mem_name + _T("MUTEX"));
    LPSECURITY_ATTRIBUTES mutex_attr = sa_mutex ? sa_mutex : sa;
    if (!InitializeWithSecAttr(mutex_name, mutex_attr)) {
      ASSERT(false, (L"Failed to initialize mutex. Err=%i", ::GetLastError()));
      return false;
    }

    // everything is synchronized till the end of the function or return.
    __mutexScope(this);

    first_instance_ = false;

    if (persist) {
      // Back this shared memory by a file
      file_ = CreateFile(name,
                         GENERIC_READ | (read_only ? 0 : GENERIC_WRITE),
                         FILE_SHARE_READ | (read_only ? 0 : FILE_SHARE_WRITE),
                         sa,
                         OPEN_ALWAYS,
                         NULL,
                         NULL);
      if (file_ == INVALID_HANDLE_VALUE)
        return false;

      if (!read_only && GetLastError() != ERROR_ALREADY_EXISTS)
        first_instance_ = true;
    } else {
      ASSERT(file_ == INVALID_HANDLE_VALUE, (L""));
      file_ = INVALID_HANDLE_VALUE;
    }

    if (read_only) {
      file_mapping_ = OpenFileMapping(FILE_MAP_READ, false, mem_name);
      if (!file_mapping_) {
        UTIL_LOG(LW, (L"[OpenFileMapping failed][error %i]", ::GetLastError()));
      }
    } else {
      file_mapping_ = CreateFileMapping(file_, sa,
                                        PAGE_READWRITE, 0, data_size, mem_name);
      ASSERT(file_mapping_, (L"CreateFileMapping. Err=%i", ::GetLastError()));
    }

    if (!file_mapping_) {
      return false;
    } else if (!read_only &&
               file_ == INVALID_HANDLE_VALUE &&
               GetLastError() != ERROR_ALREADY_EXISTS) {
      first_instance_ = true;
    }

    data_ = reinterpret_cast<T*>(MapViewOfFile(file_mapping_,
                                               FILE_MAP_READ |
                                               (read_only ? 0 : FILE_MAP_WRITE),
                                               0,
                                               0,
                                               data_size));

    if (!data_) {
      ASSERT(false, (L"MapViewOfFile. Err=%i", ::GetLastError()));
      VERIFY(CloseHandle(file_mapping_), (L""));
      file_mapping_ = NULL;

      if (file_ != INVALID_HANDLE_VALUE) {
        VERIFY(CloseHandle(file_), (L""));
        file_ = INVALID_HANDLE_VALUE;
      }

      return false;
    }

    if (!first_instance_) {
      return true;
    }

    // If this is the first instance of shared object
    // call initialization function. This is nice but
    // at the same time we can not share built in data types.
    // SharedMemoryPtr<double> - will not compile. But this is OK
    // We don't want all the overhead to just share couple of bytes.
    // Signature is void InitializeSharedData()
    (data_->*initialize_shared_data)(name);

    return true;
  }

  DISALLOW_EVIL_CONSTRUCTORS(SharedMemoryPtr);
};

// Sometimes we want Singletons that are shared between processes.
// SharedMemoryPtr can do that. But if used in C-written module there will be
// a need to make SharedMemoryPtr a global object. Making a Singleton from SharedMemoryPtr
// is possible in this situation, but syntactically this is very difficult to read.
// The following template solves the problem. It hides difficult to read details inside.
// Usage is the same as SharedMemoryPtr (ONLY through -> operator). Completely thread-safe.
// Can be used in two ways:
// Class A {
//  public:
//  void foo(){}
//
//};
// SharedMemorySingleton<A> a, b;
// a->foo();
// b->foo();  //refers to the same data in any process.
//
//  or
//
// class A : public SharedMemorySingleton<A> {
//  public:
//  void foo(){}
//};
//  A a, b;
//  a->foo();
//  b->foo(); //refers to the same data in any process.

template <typename LockType, typename T> class SharedMemorySingleton  {
public:
  SharedDataLockingProxy<LockType, T> operator->() {
    return
      Singleton<SharedMemoryPtr<LockType, T> >::Instance()->operator->();
  }
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SHARED_MEMORY_PTR_H__
