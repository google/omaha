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

// Helper functions to enable a useful pattern of using scoped pointers in
// which the ownership of the memory is transferred out to an empty
// scoped_ptr or scoped_array.
//
// The usage pattern is as follows:
//    void foo(Bar** b);
//
//    scoped_ptr<B> p;
//    foo(address(p));
//
// To receive the ownwership of the resource the scoped pointer must be
// empty, otherwise it will leak.
//
// As an implementation detail, the scoped pointers in "base/scoped_ptr.h" do
// not offer support for this idiom. The code below may break if the
// implementation of the scoped_ptr changes. The code works with the vast
// majority of the scoped_ptr implementations though.
//
// TODO(omaha): add unit tests.

#ifndef OMAHA_COMMON_SCOPED_PTR_ADDRESS__
#define OMAHA_COMMON_SCOPED_PTR_ADDRESS__

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"

template <typename T>
inline T** address(const scoped_ptr<T>& t) {
  COMPILE_ASSERT(sizeof(T*) == sizeof(scoped_ptr<T>), types_do_not_match);
  ASSERT1(!t.get());
  return reinterpret_cast<T**>(&const_cast<scoped_ptr<T>&>(t));
}

template <typename T>
inline T** address(const scoped_array<T>& t) {
  COMPILE_ASSERT(sizeof(T*) == sizeof(scoped_ptr<T>), types_do_not_match);
  ASSERT1(!t.get());
  return reinterpret_cast<T**>(&const_cast<scoped_array<T>&>(t));
}

#endif // OMAHA_COMMON_SCOPED_PTR_ADDRESS__

