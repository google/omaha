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

#ifndef OMAHA_COMMON_TYPE_UTILS_H_
#define OMAHA_COMMON_TYPE_UTILS_H_

namespace omaha {

//
// Detecting convertibility and inheritence at compile time
// (Extracted from: Modern C++ Design)
//

// Evaluates true if U inherites from T publically, or if T and U are same type
#define SUPERSUBCLASS(T, U) \
  (ConversionUtil<const U*, const T*>::exists && \
  !ConversionUtil<const T*, const void*>::same_type)

// Evaluates true only if U inherites from T publically
#define SUPERSUBCLASS_STRICT(T, U) \
  (SUPERSUBCLASS(T, U) && \
  !ConversionUtil<const T, const U>::same_type)

// Perform type test
template <class T, class U>
class ConversionUtil {
 private:
  typedef char Small;
  class Big {
    char dummy[2];
  };
  static Small Test(U);
  static Big Test(...);
  static T MakeT();

 public:
  // Tell whether there is ConversionUtil from T to U
  enum { exists = sizeof(Test(MakeT())) == sizeof(Small) };

  // Tells whether there are ConversionUtils between T and U in both directions
  enum { exists_2way = exists && ConversionUtil<U, T>::exists };

  // Tells whether there are same type
  enum { same_type = false };
};

// Perform same type test through partial template specialization
template<class T>
class ConversionUtil<T, T> {
 public:
  enum { exists = 1, exists_2way = 1, same_type = 1 };
};

}  // namespace omaha

#endif  // OMAHA_COMMON_TYPE_UTILS_H_
