// Copyright 2009 Google Inc.
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
// Use this header instead of using WTL's atlapp.h directly.
//
// WTL80 uses deprecated _vswprintf calls which may be unsafe. Disables the
// deprecation warning until WTL is fully compatible with VC 9.0.
//
// WTL80 uses min and max macros, which are disabled in the build. The work
// around is to use std::min and std::max.

#include <algorithm>
using std::min;
using std::max;

#pragma warning(push)
#pragma warning(disable : 4265 4996)
// 4265: class has virtual functions, but destructor is not virtual
// 4996: 'function' was declared deprecated

#include <atlapp.h>

#pragma warning(pop)

