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
// Provides functionality to get the command line of a Win32 process given the
// process ID.

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_PROCESS_COMMANDLINE_H__
#define OMAHA_TOOLS_SRC_GOOPDUMP_PROCESS_COMMANDLINE_H__

#include <windows.h>
#include <atlstr.h>

namespace omaha {

// Given a Win32 process ID, returns the command line for that process.
HRESULT GetProcessCommandLine(DWORD process_id, CString* command_line);

// Enables SE_DEBUG_NAME privilege for the process.
bool EnableDebugPrivilege();

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_PROCESS_COMMANDLINE_H__

