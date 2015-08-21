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

#include "omaha/base/debug.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Test what happens when we hit an ATLASSERT within ATL code.
// The & operator of CComPtr class expects the parameter to be NULL, otherwise
// the object will leak.
TEST(AtlAssertTest, AtlAssert) {
  ExpectAsserts expect_asserts;
  CComPtr<IMalloc> p;
  EXPECT_HRESULT_SUCCEEDED(::CoGetMalloc(1, &p));
  EXPECT_TRUE(p != NULL);
  ::CoGetMalloc(1, &p);     // This line is expected to assert.
}

}  // namespace omaha
