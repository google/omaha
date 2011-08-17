// Copyright 2005-2009 Google Inc.
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
// Unit Test Debug Helper

// This file contains assert override functions
// It is meant to be #included only in unittest.cc files.

#ifndef OMAHA_TESTING_UNITTEST_DEBUG_HELPER_H__
#define OMAHA_TESTING_UNITTEST_DEBUG_HELPER_H__

#include "base/basictypes.h"
#include "omaha/base/debug.h"
#include "third_party/gtest/include/gtest/gtest.h"

namespace omaha {

typedef int AssertResponse;
#define IGNORE_ALWAYS 0
#define IGNORE_ONCE   0

extern int g_assert_count;

// A class with no methods which will, once acquired, cause asserts to
// be routed through the given function; once released, they will return
// to the previous handler.
class UseAssertFunction {
 public:
  explicit UseAssertFunction(DebugAssertFunctionType *function) {
    function;
    old_function_ = REPLACE_ASSERT_FUNCTION(function);
  }

  ~UseAssertFunction() {
    REPLACE_ASSERT_FUNCTION(old_function_);
  }

  bool asserts_enabled() {
    return old_function_ != NULL;  // "!= NULL" fixes the perf warning C4800
  }

 private:
  DebugAssertFunctionType *old_function_;
  DISALLOW_EVIL_CONSTRUCTORS(UseAssertFunction);
};


// A class with no methods which will send asserts to GTest as failures;
// once released, it restores the assert handler to the previous handler.
// Example usage:
//  int main() {
//    // Report asserts to GTest, not a messagebox.
//    FailOnAssert a;
//    return RUN_ALL_TESTS();
//  }
class FailOnAssert {
 public:
  FailOnAssert() : inner_(AssertHandler) {
  }

  static AssertResponse AssertHandler(const char *expression,
      const char *message, const char *file, int line) {
    ADD_FAILURE() << "ASSERT in " << file << "(" << line << "): "
                  << expression << "; \"" << message << "\"";
    return IGNORE_ALWAYS;
  }

 private:
  UseAssertFunction inner_;
  DISALLOW_EVIL_CONSTRUCTORS(FailOnAssert);
};

// A class with no methods which will cause asserts to be ignored;
// once released, it restores the assert handler to the previous handler.
// Example usage:
//  test1() {
//    IngoreAssert a;
//    TestSomethingWhichMayOrMayNotAssert();
//  }
class IgnoreAsserts {
 public:
  IgnoreAsserts() : inner_(AssertHandler) {
  }

  static AssertResponse AssertHandler(const char *,
                                      const char *,
                                      const char *,
                                      int) {
    return IGNORE_ALWAYS;
  }

 private:
  UseAssertFunction inner_;
  DISALLOW_EVIL_CONSTRUCTORS(IgnoreAsserts);
};

// A class with no methods which will cause asserts to be counted but otherwise
// ignored; once released, a GTest assert will be run to see if any asserts
// were fired, and the assert handler will be restored to the previous handler.
// Example usage:
//  test2() {
//    ExpectAsserts a;
//    TestSomethingWhichIsKnownToAssert();
//  }
class ExpectAsserts {
 public:
  ExpectAsserts() : inner_(AssertHandler),
    old_assert_count_(g_assert_count) {
  }

  ~ExpectAsserts() {
    DeInit();
  }

  static AssertResponse AssertHandler(const char *,
                                      const char *,
                                      const char *,
                                      int) {
    ++g_assert_count;
    return IGNORE_ONCE;
  }
 private:
  void DeInit() {
    if (inner_.asserts_enabled()) {
      ASSERT_GT(g_assert_count, old_assert_count_)
          << "This test was expected to trigger at least one assert.";
    }
  }

  int old_assert_count_;
  UseAssertFunction inner_;
  DISALLOW_EVIL_CONSTRUCTORS(ExpectAsserts);
};

}  // namespace omaha

#endif  // OMAHA_TESTING_UNITTEST_DEBUG_HELPER_H__
