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

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/thread_pool.h"
#include "omaha/base/timer.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

volatile LONG g_completed_count = 0;

// Increments the global count by 1.
class MyJob1 : public UserWorkItem {
 public:
  MyJob1() {}

 private:
  virtual void DoProcess() { ::InterlockedExchangeAdd(&g_completed_count, 1); }

  DISALLOW_EVIL_CONSTRUCTORS(MyJob1);
};

// Increments the global count by 2.
class MyJob2 : public UserWorkItem {
 public:
  MyJob2() {}

 private:
  virtual void DoProcess() { ::InterlockedExchangeAdd(&g_completed_count, 2); }

  DISALLOW_EVIL_CONSTRUCTORS(MyJob2);
};

// Increments the global count by 3.
class MyJob3 : public UserWorkItem {
 public:
  MyJob3() {}

 private:
  virtual void DoProcess() { ::InterlockedExchangeAdd(&g_completed_count, 3); }

  DISALLOW_EVIL_CONSTRUCTORS(MyJob3);
};

// ThreadPool COM initialization test class. The class tests that the thread
// pool has COM initialized for the DoProcess method as well as in the Work
// Item destructor.
class UserWorkItemCoInitTest : public UserWorkItem {
 public:
  UserWorkItemCoInitTest(DWORD coinit_flags, HRESULT coinit_expected_hresult)
      : coinit_flags_(coinit_flags),
        coinit_expected_hresult_(coinit_expected_hresult) {}

  ~UserWorkItemCoInitTest() {
    scoped_co_init init_com_apt(coinit_flags_);
    EXPECT_EQ(coinit_expected_hresult_, init_com_apt.hresult());
  }

 private:
  virtual void DoProcess() {
    scoped_co_init init_com_apt(coinit_flags_);
    EXPECT_EQ(coinit_expected_hresult_, init_com_apt.hresult());
  }

  const DWORD coinit_flags_;
  const HRESULT coinit_expected_hresult_;

  DISALLOW_COPY_AND_ASSIGN(UserWorkItemCoInitTest);
};

HRESULT QueueMyJob1(ThreadPool* thread_pool) {
  scoped_ptr<MyJob1> job(new MyJob1);
  HRESULT hr = thread_pool->QueueUserWorkItem(job.get(),
                                              COINIT_MULTITHREADED,
                                              WT_EXECUTEDEFAULT);
  if (FAILED(hr)) {
    return hr;
  }
  job.release();
  return S_OK;
}

HRESULT QueueMyJob2(ThreadPool* thread_pool) {
  scoped_ptr<MyJob2> job(new MyJob2);
  HRESULT hr = thread_pool->QueueUserWorkItem(job.get(),
                                              COINIT_MULTITHREADED,
                                              WT_EXECUTEDEFAULT);
  if (FAILED(hr)) {
    return hr;
  }
  job.release();
  return S_OK;
}

HRESULT QueueMyJob3(ThreadPool* thread_pool) {
  scoped_ptr<MyJob3> job(new MyJob3);
  HRESULT hr = thread_pool->QueueUserWorkItem(job.get(),
                                              COINIT_MULTITHREADED,
                                              WT_EXECUTEDEFAULT);
  if (FAILED(hr)) {
    return hr;
  }
  job.release();
  return S_OK;
}

HRESULT QueueUserWorkItemCoInitTest(ThreadPool* thread_pool,
                                    DWORD coinit_flags_workitem,
                                    HRESULT coinit_expected_hresult,
                                    DWORD coinit_flags_threadpool) {
  scoped_ptr<UserWorkItemCoInitTest> job(
      new UserWorkItemCoInitTest(coinit_flags_workitem,
                                 coinit_expected_hresult));
  EXPECT_HRESULT_SUCCEEDED(
      thread_pool->QueueUserWorkItem(job.get(),
                                     coinit_flags_threadpool,
                                     WT_EXECUTEDEFAULT));
  job.release();
  return S_OK;
}

}   // namespace

// Creates several jobs to increment a global counter by different values and
// then it checks the value is correct.
TEST(ThreadPoolTest, ThreadPool) {
  const int kShutdownDelayMs = 0;
  const int kNumJobsEachType = 100;

  ThreadPool thread_pool;
  ASSERT_HRESULT_SUCCEEDED(thread_pool.Initialize(kShutdownDelayMs));

  for (int i = 0; i != kNumJobsEachType; ++i) {
    EXPECT_HRESULT_SUCCEEDED(QueueMyJob1(&thread_pool));
    EXPECT_HRESULT_SUCCEEDED(QueueMyJob2(&thread_pool));
    EXPECT_HRESULT_SUCCEEDED(QueueMyJob3(&thread_pool));
  }

  const int kMaxWaitForJobsMs = 2000;
  LowResTimer t(true);
  while (thread_pool.HasWorkItems() &&
         t.GetMilliseconds() < kMaxWaitForJobsMs) {
    ::Sleep(100);
  }
  EXPECT_EQ(g_completed_count, 6 * kNumJobsEachType);
}

TEST(ThreadPoolTest, UserWorkItemCoInitTest) {
  const int kShutdownDelayMs = 0;

  ThreadPool thread_pool;
  ASSERT_HRESULT_SUCCEEDED(thread_pool.Initialize(kShutdownDelayMs));

  EXPECT_HRESULT_SUCCEEDED(QueueUserWorkItemCoInitTest(
                               &thread_pool,
                               COINIT_APARTMENTTHREADED,
                               RPC_E_CHANGED_MODE,
                               COINIT_MULTITHREADED));

  EXPECT_HRESULT_SUCCEEDED(QueueUserWorkItemCoInitTest(
                               &thread_pool,
                               COINIT_MULTITHREADED,
                               RPC_E_CHANGED_MODE,
                               COINIT_APARTMENTTHREADED));

  EXPECT_HRESULT_SUCCEEDED(QueueUserWorkItemCoInitTest(
                               &thread_pool,
                               COINIT_APARTMENTTHREADED,
                               S_FALSE,
                               COINIT_APARTMENTTHREADED));

  EXPECT_HRESULT_SUCCEEDED(QueueUserWorkItemCoInitTest(
                               &thread_pool,
                               COINIT_MULTITHREADED,
                               S_FALSE,
                               COINIT_MULTITHREADED));

  const int kMaxWaitForJobsMs = 2000;
  LowResTimer t(true);
  while (thread_pool.HasWorkItems() &&
         t.GetMilliseconds() < kMaxWaitForJobsMs) {
    ::Sleep(100);
  }
}

}   // namespace omaha

