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

#ifndef OMAHA_CRASHHANDLER_CRASH_DUMP_UTIL_H_
#define OMAHA_CRASHHANDLER_CRASH_DUMP_UTIL_H_

#include <windows.h>
#include <memory>

#include "base/basictypes.h"
#include "omaha/base/string.h"
#include "omaha/crashhandler/crash_analyzer.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

extern const TCHAR* const kLaunchedForMinidump;

namespace omaha {

class EnvironmentBlockModifier;

// Checks whether current process is created to handle a particular crash dump.
bool IsRunningAsMinidumpHandler();

// Assigns given event handle and client info to environment variables so child
// process can inherit and use.
HRESULT SetCrashInfoToEnvironmentBlock(
    EnvironmentBlockModifier* eb_mod,
    HANDLE crash_processed_event,
    HANDLE mini_dump_handle,
    HANDLE full_dump_handle,
    HANDLE custom_info_handle,
    const google_breakpad::ClientInfo& client_info);

// Gets notification event handle and client info value from environment
// variables. Note the pointer values are in crashed process memory space,
// do not dereference directly.
// Caller is responsible to free client_info object.
HRESULT GetCrashInfoFromEnvironmentVariables(
    HANDLE* crash_processed_event,
    HANDLE* mini_dump_handle,
    HANDLE* full_dump_handle,
    HANDLE* custom_info_handle,
    std::unique_ptr<google_breakpad::ClientInfo>* client_info);

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASH_DUMP_UTIL_H_

