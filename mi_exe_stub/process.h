// Copyright 2006-2009 Google Inc.
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


#ifndef OMAHA_MI_EXE_STUB_PROCESS_H_
#define OMAHA_MI_EXE_STUB_PROCESS_H_

#pragma warning(push)
#pragma warning(disable : 4548)
// C4548: expression before comma has no effect
#include <atlbase.h>
#include <atlstr.h>
#pragma warning(pop)

// Convenience function to launch a process. Returns true if we successfully
// launched it.
bool Run(const CString &command_line);

// Convenience function to launch a process, wait for it to finish,
// and return the result.
bool RunAndWait(const CString &command_line, DWORD *exit_code);

// Convenience function to launch a process with the initial window hidden,
// wait for it to finish and return the result.
bool RunAndWaitHidden(const CString &command_line, DWORD *exit_code);

#endif  // OMAHA_MI_EXE_STUB_PROCESS_H_
