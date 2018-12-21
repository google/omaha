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

// Redefining the macro RegisterEventSource to evaluate to NULL so that
// CAtlServiceModuleT::LogEvent() code does not log to the event log. Doing this
// avoids duplicating the CAtlServiceModuleT code.
#undef RegisterEventSource
#define RegisterEventSource(x, ...) NULL

#include <winioctl.h>
#include <wtypes.h>
#include <tchar.h>
#include <strsafe.h>

#include "omaha/base/atlassert.h"   // Redefines ATLASSERT.

// C4265: class has virtual functions, but destructor is not virtual
// C4302: 'reinterpret_cast' : truncation from 'type_1' to 'type_2'
// C4350: behavior change: 'member1' called instead of 'member2'
// C4365: conversion from 'type_1' to 'type_2', signed/unsigned mismatch
// C4548: expression before comma has no effect
// C4702: unreachable code
// C4838: conversion requires a narrowing conversion
// C4839: non-standard use of class as an argument to a variadic function.
// C4986: exception specification does not match previous declaration
// C5038: data member will be initialized after data member
#pragma warning(disable : 4265 4302 4350 4365 4548 4702 4838 4839 4986 5038)
#include <atlbase.h>
#include <atlstr.h>     // Needs to be in front of atlapp.h
#include <atlapp.h>
#include <atlcoll.h>
#include <atlcom.h>
#include <atlcomtime.h>
#include <atlctl.h>
#include <atlgdi.h>
#include <atlhost.h>
#include <atlrx.h>
#include <atlsecurity.h>
#if (_MSC_VER < 1800)
#include <atlsecurity.inl>
#endif
#include <atltime.h>
#include <atltypes.h>
#include <atluser.h>
#include <atlwin.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>
#pragma warning(pop)

#if (_MSC_VER < 1400)
// TODO(omaha): fix the atlconv for VC8.
#include "omaha/base/atlconvfix.h"
#endif

#pragma warning(push)
// C4310: cast truncates constant value
#pragma warning(disable : 4310)
#include "base/basictypes.h"
#pragma warning(pop)

#include "gtest/gtest_prod.h"

#endif  // OMAHA_PRECOMPILE_PRECOMPILE_H__
