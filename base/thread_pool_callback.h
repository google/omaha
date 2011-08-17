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

#ifndef OMAHA_BASE_THREAD_POOL_CALLBACK_H_
#define OMAHA_BASE_THREAD_POOL_CALLBACK_H_

#include <windows.h>
#include "base/basictypes.h"
#include "omaha/base/thread_pool.h"

namespace omaha {

template <typename T>
class ThreadPoolCallBack0 : public UserWorkItem {
 public:
  explicit ThreadPoolCallBack0(T* obj, void (T::*fun)())
      : obj_(obj), fun_(fun) {}
 private:
  virtual void DoProcess() {
    (obj_->*fun_)();
  }
  T* obj_;
  void (T::*fun_)();

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolCallBack0);
};

template <typename T, typename P1>
class ThreadPoolCallBack1 : public UserWorkItem {
 public:
  explicit ThreadPoolCallBack1(T* obj, void (T::*fun)(P1), P1 p1)
      : obj_(obj), fun_(fun), p1_(p1) {}
 private:
  virtual void DoProcess() {
    (obj_->*fun_)(p1_);
  }
  T* obj_;
  void (T::*fun_)(P1);
  P1 p1_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolCallBack1);
};

template <typename P1>
class StaticThreadPoolCallBack1 : public UserWorkItem {
 public:
  explicit StaticThreadPoolCallBack1(void (*fun)(P1), P1 p1)
      : fun_(fun), p1_(p1) {}
 private:
  virtual void DoProcess() {
    (*fun_)(p1_);
  }

  void (*fun_)(P1);
  P1 p1_;

  DISALLOW_COPY_AND_ASSIGN(StaticThreadPoolCallBack1);
};

template <typename T, typename P1, typename P2>
class ThreadPoolCallBack2 : public UserWorkItem {
 public:
  explicit ThreadPoolCallBack2(T* obj, void (T::*fun)(P1, P2), P1 p1, P2 p2)
      : obj_(obj), fun_(fun), p1_(p1), p2_(p2) {}
 private:
  virtual void DoProcess() {
    (obj_->*fun_)(p1_, p2_);
  }
  T* obj_;
  void (T::*fun_)(P1, P2);
  P1 p1_;
  P2 p2_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolCallBack2);
};

}  // namespace omaha

#endif  // OMAHA_BASE_THREAD_POOL_CALLBACK_H_

