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
// static_assert.h
//
// Compile-time asserts
//
// Based on the one from boost:
// http://www.boost.org/boost/static_assert.hpp
//
// Usage:
//
// kStaticAssert(constant_boolean_expression);
//
// This can appear at file scope or within a block (anyplace a typedef
// is OK).
//
// TODO(omaha): deprecate and replace with the static assert in base/basictypes
#ifndef OMAHA_COMMON_STATIC_ASSERT_H_
#define OMAHA_COMMON_STATIC_ASSERT_H_

template <bool> struct STATIC_ASSERTION_FAILURE;

template <> struct STATIC_ASSERTION_FAILURE<true> { enum { value = 1 }; };

template<int> struct static_assert_test{};

#define STATIC_ASSERT( B ) \
typedef static_assert_test<\
  sizeof(STATIC_ASSERTION_FAILURE< (bool)( B ) >)>\
    static_assert_typedef_ ##  __LINE__

#endif  // OMAHA_COMMON_STATIC_ASSERT_H_
