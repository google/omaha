// Copyright 2006 and onwards Google Inc.
// Author: Michael Ellman (with suggestions from jrvb, m3b, toddw, jwray)
//
// A simple reference counted pointer implementation. It is a subset
// of the boost/tr1 shared_ptr class, which is expected to be part of
// the next C++ standard.  See section 20.8.10 [util.smartptr] of the
// draft standard for a full description of the standard interface.
//
// Standard features that have been omitted from this implementation include:
//   - no custom deallocators - uses delete
//   - shared_ptr<T>'s constructor isn't templated: its argument is just T*.
//   - no support for smart pointer casts
//   - no support for unowned pointers
//   - no support for variadic templates or rvalue references
//   - no integration with auto_ptr or unique_ptr
//   - not exception-safe
//   - no overloaded comparison operators (e.g. operator<). They're
//     convenient, but they can be explicitly defined outside the class.
//
// It's usually the case that when you want to share an object, there
// is a clear owner that outlives the other users.  If that's the case,
// the owner can use scoped_ptr and the rest can use a raw pointer.
//
// A somewhat common design pattern that doesn't have a clear object
// owner is when there is a shared container in which older versions
// of an object are replaced with newer versions.  The objects should be
// deleted only when (a) they are replaced with a new version and (b)
// there are no outside users of the old version.  Replacing raw pointers
// in the implementation with shared_ptr's ensures that the accounting
// and object lifetimes are handled appropriately.
//
// The typical usage is as follows.
//
//  1. Functions using shared_ptr's should declare shared_ptr parameters to
//     be of type const reference since the caller will still have its own
//     shared_ptr for the entire call.
//
//       void foo(const shared_ptr<T>& param)
//
//  2. Functions setting shared_ptr's should declare shared_ptr parameters
//     to be of pointer type.
//
//     typedef map<Key, shared_ptr<Value> > MyMap;
//     void GetAndSharedObject(const Key& key, shared_ptr<Value>* value) {
//       ReaderMutexLock l(&lock_);
//       MyMap::iterator iter = shared_container.find(key);
//       *value = iter->second;
//     }
//
// Thread Safety:
//   Once constructed, a shared_ptr has the same thread-safety as built-in
//   types.  In particular, it is safe to read a shared object simultaneously
//   from multiple threads.
//
// Weak ptrs
//   The weak_ptr auxiliary class (see clause 20.8.10.3 of the draft standard)
//   is used to break ownership cycles. A weak_ptr points to an object that's
//   owned by a shared_ptr, but the weak_ptr is an observer, not an owner. When
//   the last shared_ptr that points to the object disappears, the weak_ptr
//   expires, at which point the expired() member function will return true.
//
//   You can't directly get a raw pointer from weak_ptr, i.e. it has no get()
//   or operator*() member function. (These features were intentionally left out
//   to avoid the risk of dangling pointers.) To access a weak_ptr's pointed-to
//   object, use lock() to obtain a temporary shared_ptr.
//
// enable_shared_from_this
//   A user-defined class T can inherit from enable_shared_from_this<T> (see
//   clause 20.8.10.5 of the draft standard) to inherit T::shared_from_this(),
//   which returns a shared_ptr pointing to *this. It is similar to weak_ptr in
//   that there must already be at least one shared_ptr instance that owns
//   *this.

#ifndef BAR_COMMON_SHARED_PTR_H_
#define BAR_COMMON_SHARED_PTR_H_

#include <windows.h>
#include <algorithm>  // for swap

template <typename T> class shared_ptr;
template <typename T> class weak_ptr;

// This class is an internal implementation detail for shared_ptr. If two
// shared_ptrs point to the same object, they also share a control block.
// An "empty" shared_pointer refers to NULL and also has a NULL control block.
// It contains all of the state that's needed for reference counting or any
// other kind of resource management. In this implementation the control block
// happens to consist of two atomic words, the reference count (the number
// of shared_ptrs that share ownership of the object) and the weak count
// (the number of weak_ptrs that observe the object, plus 1 if the
// refcount is nonzero).
//
// The "plus 1" is to prevent a race condition in the shared_ptr and
// weak_ptr destructors. We need to make sure the control block is
// only deleted once, so we need to make sure that at most one
// object sees the weak count decremented from 1 to 0.
class SharedPtrControlBlock {
  template <typename T> friend class shared_ptr;
  template <typename T> friend class weak_ptr;
 private:
  SharedPtrControlBlock() : refcount_(1), weak_count_(1) { }
  LONG refcount_;
  LONG weak_count_;
};

// Forward declaration. The class is defined below.
template <typename T> class enable_shared_from_this;

template <typename T>
class shared_ptr {
  template <typename U> friend class weak_ptr;
 public:
  typedef T element_type;

  explicit shared_ptr(T* ptr = NULL)
      : ptr_(ptr),
        control_block_(ptr != NULL ? new SharedPtrControlBlock : NULL) {
    // If p is non-null and T inherits from enable_shared_from_this, we
    // set up the data that shared_from_this needs.
    MaybeSetupWeakThis(ptr);
  }

  // Copy constructor: makes this object a copy of ptr, and increments
  // the reference count.
  template <typename U>
  shared_ptr(const shared_ptr<U>& ptr)
      : ptr_(NULL),
        control_block_(NULL) {
    Initialize(ptr);
  }
  // Need non-templated version to prevent the compiler-generated default
  shared_ptr(const shared_ptr<T>& ptr)
      : ptr_(NULL),
        control_block_(NULL) {
    Initialize(ptr);
  }

  // Assignment operator. Replaces the existing shared_ptr with ptr.
  // Increment ptr's reference count and decrement the one being replaced.
  template <typename U>
  shared_ptr<T>& operator=(const shared_ptr<U>& ptr) {
    if (ptr_ != ptr.ptr_) {
      shared_ptr<T> me(ptr);   // will hold our previous state to be destroyed.
      swap(me);
    }
    return *this;
  }

  // Need non-templated version to prevent the compiler-generated default
  shared_ptr<T>& operator=(const shared_ptr<T>& ptr) {
    if (ptr_ != ptr.ptr_) {
      shared_ptr<T> me(ptr);   // will hold our previous state to be destroyed.
      swap(me);
    }
    return *this;
  }

  // TODO(austern): Consider providing this constructor. The draft C++ standard
  // (20.8.10.2.1) includes it. However, it says that this constructor throws
  // a bad_weak_ptr exception when ptr is expired. Is it better to provide this
  // constructor and make it do something else, like fail with a CHECK, or to
  // leave this constructor out entirely?
  //
  // template <typename U>
  // shared_ptr(const weak_ptr<U>& ptr);

  ~shared_ptr() {
    if (ptr_ != NULL) {
      if (::InterlockedDecrement(&control_block_->refcount_) == 0) {
        delete ptr_;

        // weak_count_ is defined as the number of weak_ptrs that observe
        // ptr_, plus 1 if refcount_ is nonzero.
        if (::InterlockedDecrement(&control_block_->weak_count_) == 0) {
          delete control_block_;
        }
      }
    }
  }

  // Replaces underlying raw pointer with the one passed in.  The reference
  // count is set to one (or zero if the pointer is NULL) for the pointer
  // being passed in and decremented for the one being replaced.
  void reset(T* p = NULL) {
    if (p != ptr_) {
      shared_ptr<T> tmp(p);
      tmp.swap(*this);
    }
  }

  // Exchanges the contents of this with the contents of r.  This function
  // supports more efficient swapping since it eliminates the need for a
  // temporary shared_ptr object.
  void swap(shared_ptr<T>& r) {
    std::swap(ptr_, r.ptr_);
    std::swap(control_block_, r.control_block_);
  }

  // The following function is useful for gaining access to the underlying
  // pointer when a shared_ptr remains in scope so the reference-count is
  // known to be > 0 (e.g. for parameter passing).
  T* get() const {
    return ptr_;
  }

  T& operator*() const {
    return *ptr_;
  }

  T* operator->() const {
    return ptr_;
  }

  LONG use_count() const {
    return control_block_ ? control_block_->refcount_ : 1;
  }

  bool unique() const {
    return use_count() == 1;
  }

 private:
  // If r is non-empty, initialize *this to share ownership with r,
  // increasing the underlying reference count.
  // If r is empty, *this remains empty.
  // Requires: this is empty, namely this->ptr_ == NULL.
  template <typename U>
  void Initialize(const shared_ptr<U>& r) {
    if (r.control_block_ != NULL) {
      ::InterlockedIncrement(&r.control_block_->refcount_);

      ptr_ = r.ptr_;
      control_block_ = r.control_block_;
    }
  }

  // Helper function for the constructor that takes a raw pointer. If T
  // doesn't inherit from enable_shared_from_this<T> then we have nothing to
  // do, so this function is trivial and inline. The other version is declared
  // out of line, after the class definition of enable_shared_from_this.
  void MaybeSetupWeakThis(enable_shared_from_this<T>* ptr);
  void MaybeSetupWeakThis(...) { }

  T* ptr_;
  SharedPtrControlBlock* control_block_;

  template <typename U>
  friend class shared_ptr;
};

// Matches the interface of std::swap as an aid to generic programming.
template <typename T> void swap(shared_ptr<T>& r, shared_ptr<T>& s) {
  r.swap(s);
}

// See comments at the top of the file for a description of why this
// class exists, and the draft C++ standard (as of July 2009 the
// latest draft is N2914) for the detailed specification.
template <typename T>
class weak_ptr {
  template <typename U> friend class weak_ptr;
 public:
  typedef T element_type;

  // Create an empty (i.e. already expired) weak_ptr.
  weak_ptr() : ptr_(NULL), control_block_(NULL) { }

  // Create a weak_ptr that observes the same object that ptr points
  // to.  Note that there is no race condition here: we know that the
  // control block can't disappear while we're looking at it because
  // it is owned by at least one shared_ptr, ptr.
  template <typename U> weak_ptr(const shared_ptr<U>& ptr) {
    CopyFrom(ptr.ptr_, ptr.control_block_);
  }

  // Copy a weak_ptr. The object it points to might disappear, but we
  // don't care: we're only working with the control block, and it can't
  // disappear while we're looking at because it's owned by at least one
  // weak_ptr, ptr.
  template <typename U> weak_ptr(const weak_ptr<U>& ptr) {
    CopyFrom(ptr.ptr_, ptr.control_block_);
  }

  // Need non-templated version to prevent default copy constructor
  weak_ptr(const weak_ptr& ptr) {
    CopyFrom(ptr.ptr_, ptr.control_block_);
  }

  // Destroy the weak_ptr. If no shared_ptr owns the control block, and if
  // we are the last weak_ptr to own it, then it can be deleted. Note that
  // weak_count_ is defined as the number of weak_ptrs sharing this control
  // block, plus 1 if there are any shared_ptrs. We therefore know that it's
  // safe to delete the control block when weak_count_ reaches 0, without
  // having to perform any additional tests.
  ~weak_ptr() {
    if (control_block_ != NULL &&
        ::InterlockedDecrement(&control_block_->weak_count_) == 0) {
      delete control_block_;
    }
  }

  weak_ptr& operator=(const weak_ptr& ptr) {
    if (&ptr != this) {
      weak_ptr tmp(ptr);
      tmp.swap(*this);
    }
    return *this;
  }
  template <typename U> weak_ptr& operator=(const weak_ptr<U>& ptr) {
    weak_ptr tmp(ptr);
    tmp.swap(*this);
    return *this;
  }
  template <typename U> weak_ptr& operator=(const shared_ptr<U>& ptr) {
    weak_ptr tmp(ptr);
    tmp.swap(*this);
    return *this;
  }

  void swap(weak_ptr& ptr) {
    std::swap(ptr_, ptr.ptr_);
    std::swap(control_block_, ptr.control_block_);
  }

  void reset() {
    weak_ptr tmp;
    tmp.swap(*this);
  }

  // Return the number of shared_ptrs that own the object we are observing.
  // Note that this number can be 0 (if this pointer has expired).
  LONG use_count() const {
    return control_block_ != NULL ? control_block_->refcount_ : 0;
  }

  bool expired() const { return use_count() == 0; }

  // Return a shared_ptr that owns the object we are observing. If we
  // have expired, the shared_ptr will be empty. We have to be careful
  // about concurrency, though, since some other thread might be
  // destroying the last owning shared_ptr while we're in this
  // function.  We want to increment the refcount only if it's nonzero
  // and get the new value, and we want that whole operation to be
  // atomic.
  shared_ptr<T> lock() const {
    shared_ptr<T> result;
    if (control_block_ != NULL) {
      LONG old_refcount;
      do {
        old_refcount = control_block_->refcount_;
        if (old_refcount == 0)
          break;
      } while (old_refcount !=
               ::InterlockedCompareExchange(
                      &control_block_->refcount_, old_refcount + 1,
                      old_refcount));
      if (old_refcount > 0) {
        result.ptr_ = ptr_;
        result.control_block_ = control_block_;
      }
    }

    return result;
  }

 private:
  void CopyFrom(T* ptr, SharedPtrControlBlock* control_block) {
    ptr_ = ptr;
    control_block_ = control_block;
    if (control_block_ != NULL)
      ::InterlockedIncrement(&control_block_->weak_count_);
  }

 private:
  element_type* ptr_;
  SharedPtrControlBlock* control_block_;
};

template <typename T> void swap(weak_ptr<T>& r, weak_ptr<T>& s) {
  r.swap(s);
}

// See comments at the top of the file for a description of why this class
// exists, and section 20.8.10.5 of the draft C++ standard (as of July 2009
// the latest draft is N2914) for the detailed specification.
template <typename T>
class enable_shared_from_this {
  friend class shared_ptr<T>;
 public:
  // Precondition: there must be a shared_ptr that owns *this and that was
  // created, directly or indirectly, from a raw pointer of type T*. (The
  // latter part of the condition is technical but not quite redundant; it
  // rules out some complicated uses involving inheritance hierarchies.)
  shared_ptr<T> shared_from_this() {
    // Behavior is undefined if the precondition isn't satisfied; we choose
    // to die with an access violation exception.
#if DEBUG
    if (weak_this_.expired()) {
      // No shared_ptr owns this object.
      *static_cast<int*>(NULL) = 0;
    }
#endif
    return weak_this_.lock();
  }
  shared_ptr<const T> shared_from_this() const {
#if DEBUG
    if (weak_this_.expired()) {
      // No shared_ptr owns this object.
      *static_cast<int*>(NULL) = 0;
    }
#endif
    return weak_this_.lock();
  }

 protected:
  enable_shared_from_this() { }
  enable_shared_from_this(const enable_shared_from_this& other) { }
  enable_shared_from_this& operator=(const enable_shared_from_this& other) {
    return *this;
  }
  ~enable_shared_from_this() { }

 private:
  weak_ptr<T> weak_this_;
};

// This is a helper function called by shared_ptr's constructor from a raw
// pointer. If T inherits from enable_shared_from_this<T>, it sets up
// weak_this_ so that shared_from_this works correctly. If T does not inherit
// from weak_this we get a different overload, defined inline, which does
// nothing.
template<typename T>
void shared_ptr<T>::MaybeSetupWeakThis(enable_shared_from_this<T>* ptr) {
  if (ptr)
    ptr->weak_this_ = *this;
}

#endif  // BAR_COMMON_SHARED_PTR_H_
