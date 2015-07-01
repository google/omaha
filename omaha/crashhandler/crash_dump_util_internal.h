// Copyright 2013 Google Inc.
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

#ifndef OMAHA_CRASHHANDLER_CRASH_DUMP_UTIL_INTERNAL_H_
#define OMAHA_CRASHHANDLER_CRASH_DUMP_UTIL_INTERNAL_H_

#include <windows.h>

#include "base/basictypes.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

namespace omaha {

class EnvironmentBlockModifier;

namespace internal {

// A flattened memory piece for data transmission. All pointers are valid in the
// crashed process address space only. So do *NOT* dereference them. Call
// ReadProcessMemory() to get real contents.
struct CrashInfo {
  HANDLE notification_event;
  DWORD crash_id;
  DWORD crash_process_id;
  DWORD* crash_thread_id;
  EXCEPTION_POINTERS** ex_info;
  MDRawAssertionInfo* assert_info;
  MINIDUMP_TYPE dump_type;
  google_breakpad::CustomClientInfo custom_info;
  HANDLE mini_dump_handle;
  HANDLE full_dump_handle;
  HANDLE custom_info_handle;
};

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASH_DUMP_UTIL_INTERNAL_H_
