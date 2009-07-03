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

#ifndef OMAHA_PRECOMPILE_PRECOMPILE_H__
#define OMAHA_PRECOMPILE_PRECOMPILE_H__

// 'function': name was marked as #pragma deprecated
#pragma warning(disable : 4995)

#pragma warning(push)
// C4201: nonstandard extension used : nameless struct/union
#pragma warning(disable : 4201)
#include <windows.h>
#include <winioctl.h>
#include <wtypes.h>
#include <tchar.h>
#include <strsafe.h>

#include "omaha/common/atlassert.h"   // Redefines ATLASSERT.

// C4061: enumerate is not explicitly handled by a case label
// C4265: class has virtual functions, but destructor is not virtual
// C4510: default constructor could not be generated
// C4548: expression before comma has no effect
// C4610: struct can never be instantiated - user defined constructor required
// C4826: conversion from 'type1 ' to 'type_2' is sign-extended
#pragma warning(disable : 4061 4265 4510 4610 4548 4826)
#include <atlbase.h>
#include <atlstr.h>
#include <atlcoll.h>
#include <atlcom.h>
#include <atlhost.h>
#include <atlrx.h>
#include <atlsecurity.h>
#include <atltypes.h>
#include <atlwin.h>
#include <algorithm>
#include <cstdlib>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>
#pragma warning(pop)

#if (_MSC_VER < 1400)
// TODO(omaha): fix the atlconv for VC8.
#include "omaha/common/atlconvfix.h"
#endif

#pragma warning(push)
// C4310: cast truncates constant value
#pragma warning(disable : 4310)
#include "base/basictypes.h"
#pragma warning(pop)

#ifdef UNITTEST
#include "third_party/gtest/include/gtest/gtest.h"
#endif  // UNITTEST

#endif  // OMAHA_PRECOMPILE_PRECOMPILE_H__
