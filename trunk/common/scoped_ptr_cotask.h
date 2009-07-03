// Copyright 2007-2009 Google Inc.
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
// Defines utility classes and functions which facilitate working with the
// memory allocation functions CoTaskMemAlloc and CoTaskMemFree.

#ifndef OMAHA_COMMON_SCOPED_PTR_COTASK_H__
#define OMAHA_COMMON_SCOPED_PTR_COTASK_H__

#include "omaha/common/debug.h"

// scoped_ptr_cotask is identical to scoped_ptr, except that CoTaskMemFree is
// called instead of delete.  For documentation of the interface, see
// scoped_ptr.h.

template <typename T>
class scoped_ptr_cotask;

template <typename T>
scoped_ptr_cotask<T> make_scoped_ptr_cotask(T* p);

template <typename T>
class scoped_ptr_cotask {
 private:
  T* ptr_;

  scoped_ptr_cotask(scoped_ptr_cotask const &);
  scoped_ptr_cotask & operator=(scoped_ptr_cotask const &);

  friend scoped_ptr_cotask<T> make_scoped_ptr_cotask<T>(T* p);

 public:
  typedef T element_type;

  explicit scoped_ptr_cotask(T* p = 0): ptr_(p) {}

  ~scoped_ptr_cotask() {
    typedef char type_must_be_complete[sizeof(T)];
    ::CoTaskMemFree(ptr_);
  }

  void reset(T* p = 0) {
    typedef char type_must_be_complete[sizeof(T)];

    if (ptr_ != p) {
      ::CoTaskMemFree(ptr_);
      ptr_ = p;
    }
  }

  T& operator*() const {
    assert(ptr_ != 0);
    return *ptr_;
  }

  T* operator->() const  {
    assert(ptr_ != 0);
    return ptr_;
  }

  bool operator==(T* p) const {
    return ptr_ == p;
  }

  bool operator!=(T* p) const {
    return ptr_ != p;
  }

  T* get() const  {
    return ptr_;
  }

  void swap(scoped_ptr_cotask & b) {
    T* tmp = b.ptr_;
    b.ptr_ = ptr_;
    ptr_ = tmp;
  }

  T* release() {
    T* tmp = ptr_;
    ptr_ = 0;
    return tmp;
  }

 private:
  template <typename U> bool operator==(scoped_ptr_cotask<U> const& p) const;
  template <typename U> bool operator!=(scoped_ptr_cotask<U> const& p) const;
};

template <typename T>
scoped_ptr_cotask<T> make_scoped_ptr_cotask(T* p) {
  return scoped_ptr_cotask<T>(p);
}

template <typename T> inline
void swap(scoped_ptr_cotask<T>& a, scoped_ptr_cotask<T>& b) {
  a.swap(b);
}

template <typename T> inline
bool operator==(T* p, const scoped_ptr_cotask<T>& b) {
  return p == b.get();
}

template <typename T> inline
bool operator!=(T* p, const scoped_ptr_cotask<T>& b) {
  return p != b.get();
}

template <typename T>
inline T** address(const scoped_ptr_cotask<T>& t) {
  COMPILE_ASSERT(sizeof(T*) == sizeof(t), types_do_not_match);
  ASSERT1(!t.get());
  return reinterpret_cast<T**>(&const_cast<scoped_ptr_cotask<T>&>(t));
}

// scoped_array_cotask manages an array of pointers to objects allocated by
// CoTaskMemAlloc.  The array is also allocated via CoTaskMemAlloc.  The
// interface is similar to scoped_array, except that an array length must be
// explicitly provided.  When the object is destructed, the array and each
// element of the array are explicitly freed.

template <typename T>
class scoped_array_cotask;

template <typename T>
class scoped_array_cotask<T*> {
 private:
  size_t count_;
  T** ptr_;

  scoped_array_cotask(scoped_array_cotask const &);
  scoped_array_cotask & operator=(scoped_array_cotask const &);

 public:
  typedef T* element_type;

  explicit scoped_array_cotask(size_t c, T** p = 0)
      : count_(c), ptr_(p) {
    if (!ptr_) {
      const size_t array_size = sizeof(T*) * count_;
      ptr_ = static_cast<T**>(::CoTaskMemAlloc(array_size));
      memset(ptr_, 0, array_size);
    }
  }

  ~scoped_array_cotask() {
    typedef char type_must_be_complete[sizeof(T*)];
    if (ptr_) {
      for (size_t i = 0; i < count_; ++i) {
        ::CoTaskMemFree(ptr_[i]);
      }
      ::CoTaskMemFree(ptr_);
    }
  }

  size_t size() const { return count_; }

  void reset(size_t c, T** p = 0) {
    typedef char type_must_be_complete[sizeof(T*)];

    if (ptr_ != p) {
      if (ptr_) {
        for (size_t i = 0; i < count_; ++i) {
          ::CoTaskMemFree(ptr_[i]);
        }
        ::CoTaskMemFree(ptr_);
      }
      ptr_ = p;
    }
    count_ = c;
    if (!ptr_) {
      const size_t array_size = sizeof(T*) * count_;
      ptr_ = static_cast<T**>(::CoTaskMemAlloc(array_size));
      memset(ptr_, 0, array_size);
    }
  }

  T*& operator[](std::ptrdiff_t i) const {
    assert(ptr_ != 0);
    assert(i >= 0);
    return ptr_[i];
  }

  bool operator==(T** p) const {
    return ptr_ == p;
  }

  bool operator!=(T** p) const {
    return ptr_ != p;
  }

  T** get() const {
    return ptr_;
  }

  void swap(scoped_array_cotask & b) {
    T** tmp = b.ptr_;
    b.ptr_ = ptr_;
    ptr_ = tmp;
  }

  T** release() {
    T** tmp = ptr_;
    ptr_ = 0;
    return tmp;
  }

 private:
  template <typename U> bool operator==(scoped_array_cotask<U> const& p) const;
  template <typename U> bool operator!=(scoped_array_cotask<U> const& p) const;
};

template <typename T> inline
void swap(scoped_array_cotask<T>& a, scoped_array_cotask<T>& b) {
  a.swap(b);
}

template <typename T> inline
bool operator==(T* p, const scoped_array_cotask<T>& b) {
  return p == b.get();
}

template <typename T> inline
bool operator!=(T* p, const scoped_array_cotask<T>& b) {
  return p != b.get();
}

// address() is not relevant to scoped_array_cotask, due to the count parameter.
//
// template <typename T>
// inline T** address(const scoped_array_cotask<T>& t) {
//   COMPILE_ASSERT(sizeof(T*) == sizeof(t), types_do_not_match);
//   ASSERT1(!t.get());
//   return reinterpret_cast<T**>(&const_cast<scoped_array_cotask<T>&>(t));
// }

// StrDupCoTask allocates a copy of a string using CoTaskMemAlloc.

template <class T>
inline T* StrDupCoTask(const T* str, size_t length) {
  T* mem = static_cast<T*>(::CoTaskMemAlloc(sizeof(T) * (length + 1)));
  memcpy(mem, str, sizeof(T) * length);
  mem[length] = 0;
  return mem;
}

#endif  // OMAHA_COMMON_SCOPED_PTR_COTASK_H__
