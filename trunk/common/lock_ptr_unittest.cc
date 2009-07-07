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
// lock_ptr_unittest.cpp

#include "omaha/common/lock_ptr.h"
#include "omaha/common/synchronized.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Test type.
class X {
 public:
  X() : i_(0) {}
  void f() { i_ = 10; }

 private:
  int i_;

  friend class LockTest;
  FRIEND_TEST(LockTest, X);
};

// A dummy lock just to test that locking and unlocking methods get called.
class DummyLock {
 public:
  DummyLock() : cnt_(0) {}

  void FakeLock() { ++cnt_; }
  void FakeUnlock() { --cnt_; }

 private:
  int cnt_;

  friend class LockTest;
  FRIEND_TEST(LockTest, DummyLock);
};

// Specialization of the policy for the DummyLock.
template<> void AcquireLock(DummyLock& lock) { lock.FakeLock(); }
template<> void ReleaseLock(DummyLock& lock) { lock.FakeUnlock(); }

// Empty test fixture.
class LockTest : public testing::Test {};

TEST_F(LockTest, X) {
  // Create a few synchronization objects.
  CriticalSection cs_lock;
  LLock local_lock;
  GLock global_lock;
  ASSERT_TRUE(global_lock.Initialize(_T("test")));

  // The instance to lock.
  X x;

  // Lock the instance and call a method.
  LockPtr<X>(x, local_lock)->f();
  ASSERT_EQ((*LockPtr<X>(x, cs_lock)).i_, 10);

  // Lock the instance and access a data member.
  LockPtr<X>(x, cs_lock)->i_ = 0;
  ASSERT_EQ((*LockPtr<X>(x, cs_lock)).i_, 0);

  // Lock the instance and call a method.
  LockPtr<X>(x, global_lock)->f();
  ASSERT_EQ((*LockPtr<X>(x, cs_lock)).i_, 10);
}

TEST_F(LockTest, DummyLock) {
  DummyLock dummy_lock;
  ASSERT_EQ(dummy_lock.cnt_, 0);

  // The instance to lock.
  X x;

  {
    LockPtr<X> p(x, dummy_lock);
    ASSERT_EQ(dummy_lock.cnt_, 1);
  }

  ASSERT_EQ(dummy_lock.cnt_, 0);
}

}  // namespace omaha

