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


#include <atlstr.h>
#include "omaha/common/sta.h"
#include "omaha/common/sta_call.h"
#include "omaha/common/thread.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

class X {
 public:
  void f() {}
  void f(int) {}
  void f(unsigned int, X*) {}
  void f(bool, char, long*) {}

  static void g() {}
  static void g(int) {}
  static void g(unsigned int, X*) {}
  static void g(bool, char, long*) {}
};

class Y {
 public:
  HRESULT f() { return S_OK; }
  HRESULT f(int) { return S_OK; }
  HRESULT f(unsigned int, X*) { return S_OK; }
  HRESULT f(bool, char, long*) { return S_OK; }

  static HRESULT g() { return S_OK; }
  static HRESULT g(int) { return S_OK; }
  static HRESULT g(unsigned int, X*) { return S_OK; }
  static HRESULT g(bool, char, long*) { return S_OK; }
};

class Z {
 public:
  void f(char, signed char, unsigned char) {}

  void f(Y*) {}
  // void f(const Y*) {} // not supported !!!

  void m() {}
  void m() const {}
};

class Test {
 public:
  int Add(int i, int j) { return i + j; }
  void Add(int i, int j, int* sum) { *sum = i + j; }
};

int Add(long i, long j) { return i + j; }
void Add(long i, long j, long* sum) { *sum = i + j; }

void Print(const char*) {}
void Print1(CString*) {}

}  // namespace

class CompileTest : public Runnable {
 protected:
  virtual void Run();
};

void CompileTest::Run() {
  X x;
  Y y;

  CallFunction(X::g);
  CallFunction(X::g, 10);
  CallFunction(X::g, static_cast<unsigned int>(10), &x);
  CallFunction(X::g, true, 'a', static_cast<long*>(0));

  CallFunction(Y::g);
  CallFunction(Y::g, 10);
  CallFunction(Y::g, static_cast<unsigned int>(10), &x);
  CallFunction(Y::g, true, 'a', static_cast<long*>(0));

  CallMethod(&x, &X::f);
  CallMethod(&x, &X::f, 10);
  CallMethod(&x, &X::f, static_cast<unsigned int>(10), &x);
  CallMethod(&x, &X::f, true, 'a', static_cast<long*>(0));

  CallMethod(&y, &Y::f);
  CallMethod(&y, &Y::f, 20);
  CallMethod(&y, &Y::f, static_cast<unsigned int>(10), &x);
  CallMethod(&y, &Y::f, true, 'a', static_cast<long*>(0));

  Z z;
  CallMethod(&z,
             &Z::f,
             'a',
             static_cast<signed char>('a'),
             static_cast<unsigned char>('a'));


  CallMethod(&z, &Z::f, &y);

  // Does not compile: template parameter 'P' is ambiguous
  // const Y cy;
  // CallMethod(&z, &Z::f, &cy);

  CallMethod(&z, &Z::m);

  // Does not compile: template parameter 'T' is ambiguous
  // const Z cz;
  // CallMethod(&cz, &Z::m);

  // Does not compile: cannot convert from 'const Z *' to 'Z *const '
  // const Z cz;
  // CallMethod<const Z, void>(&cz, &Z::m);

  CString msg(_T("test"));
  CallFunction(Print, "test");
  CallFunction(Print1, &msg);
}

class RuntimeTest : public Runnable {
 protected:
  virtual void Run();
};

void RuntimeTest::Run() {
  Test test;
  ASSERT_EQ(CallMethod(&test, &Test::Add, 10, 20), 30);

  int sum(0);
  CallMethod(&test, &Test::Add, -10, 20, &sum);
  ASSERT_EQ(sum, 10);

  {
  ASSERT_EQ(CallFunction(Add, long(10), long(20)), 30);

  long sum = 0;
  CallFunction(Add, long(10), long(-20), &sum);
  ASSERT_EQ(sum, -10);
  }
}


class AsyncTest : public Runnable {
 protected:
  virtual void Run();
};

void AsyncTest::Run() {
  static X x;
  static Y y;

  CallFunctionAsync(X::g);
  CallFunctionAsync(X::g, 10);
  CallFunctionAsync(X::g, static_cast<unsigned int>(10), &x);
  CallFunctionAsync(X::g, true, 'a', static_cast<long*>(0));

  CallFunctionAsync(Y::g);
  CallFunctionAsync(Y::g, 10);
  CallFunctionAsync(Y::g, static_cast<unsigned int>(10), &x);
  CallFunctionAsync(Y::g, true, 'a', static_cast<long*>(0));

  CallMethodAsync(&x, &X::f);
  CallMethodAsync(&x, &X::f, 10);
  CallMethodAsync(&x, &X::f, static_cast<unsigned int>(10), &x);
  CallMethodAsync(&x, &X::f, true, 'a', static_cast<long*>(0));

  CallMethodAsync(&y, &Y::f);
  CallMethodAsync(&y, &Y::f, 20);
  CallMethodAsync(&y, &Y::f, static_cast<unsigned int>(10), &x);
  CallMethodAsync(&y, &Y::f, true, 'a', static_cast<long*>(0));

  static Z z;
  CallMethodAsync(&z,
                  &Z::f,
                  'a',
                  static_cast<signed char>('a'),
                  static_cast<unsigned char>('a'));


  CallMethodAsync(&z, &Z::f, &y);

  // Does not compile: template parameter 'P' is ambiguous
  // const Y cy;
  // CallMethod(&z, &Z::f, &cy);

  CallMethodAsync(&z, &Z::m);

  // Does not compile: template parameter 'T' is ambiguous
  // const Z cz;
  // CallMethod(&cz, &Z::m);

  // Does not compile: cannot convert from 'const Z *' to 'Z *const '
  // const Z cz;
  // CallMethod<const Z, void>(&cz, &Z::m);

  CString msg(_T("test"));
  CallFunctionAsync(Print, "test");
  CallFunctionAsync(Print1, &msg);

  WaitWithMessageLoopTimed(1000);
}


TEST(STATest, CompileTest) {
  ASSERT_SUCCEEDED(InitializeApartment(0));

  Thread t;
  CompileTest compile_test;
  t.Start(&compile_test);
  EXPECT_TRUE(WaitWithMessageLoop(t.GetThreadHandle()));

  ASSERT_SUCCEEDED(UninitializeApartment());
}

TEST(STATest, RuntimeTest) {
  ASSERT_SUCCEEDED(InitializeApartment(0));

  Thread t;
  RuntimeTest runtime_test;
  t.Start(&runtime_test);
  EXPECT_TRUE(WaitWithMessageLoop(t.GetThreadHandle()));

  ASSERT_SUCCEEDED(UninitializeApartment());
}


TEST(STATest, AsyncTest) {
  ASSERT_SUCCEEDED(InitializeApartment(0));

  Thread t;
  AsyncTest async_test;
  t.Start(&async_test);
  EXPECT_TRUE(WaitWithMessageLoop(t.GetThreadHandle()));

  ASSERT_SUCCEEDED(UninitializeApartment());
}

TEST(STATest, ApartmentRefCounting) {
  // Check the reference counting is working.
  ASSERT_SUCCEEDED(InitializeApartment(0));
  ASSERT_SUCCEEDED(InitializeApartment(0));
  ASSERT_SUCCEEDED(UninitializeApartment());
  ASSERT_SUCCEEDED(UninitializeApartment());

  // The call below will raise an assert in the the STA code.
  ExpectAsserts expect_asserts;
  ASSERT_EQ(E_UNEXPECTED, UninitializeApartment());
}

TEST(STATest, ScopedSTA) {
  {
    scoped_sta sta(0);
    ASSERT_SUCCEEDED(sta.result());
  }
  {
    scoped_sta sta(0);
    ASSERT_SUCCEEDED(sta.result());
  }
  {
    scoped_sta sta1(0);
    scoped_sta sta2(0);
    ASSERT_SUCCEEDED(sta1.result());
    ASSERT_SUCCEEDED(sta2.result());
  }

  ExpectAsserts expect_asserts;
  ASSERT_EQ(E_UNEXPECTED, UninitializeApartment());
}

}  // namespace omaha

