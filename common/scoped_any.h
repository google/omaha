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

// smartany library uses a poor compile time assert. It is actually generating
// runtime code to call the constructor of the assert class. Most likely to
// improve performance, the library uses a static instance of the compile time
// assert object. In theory the code is not thread-safe. In practice, since the
// compile time assert is an empty class, there should be no problems. The long
// term solution requires changing smartany to use a better compile time assert
// which is completely evaluated at compile time and it has no effects
// whatsoever at runtime. Short term, the code should include these wrappers
// to silence the compiler warning.

#ifndef OMAHA_COMMON_SCOPED_ANY__
#define OMAHA_COMMON_SCOPED_ANY__

#pragma warning(push)
// C4640: construction of local static object is not thread-safe
#pragma warning(disable : 4640)
#include "omaha/third_party/smartany/scoped_any.h"
#pragma warning(pop)

#endif  // OMAHA_COMMON_SCOPED_ANY__

