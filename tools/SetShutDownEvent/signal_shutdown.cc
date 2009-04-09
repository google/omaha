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
//
// Simple tool to signal the shutdownevent.

#include <Windows.h>
#include <stdio.h>

#include "omaha/common/const_object_names.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/utils.h"

int _tmain(int argc, TCHAR* argv[]) {
  if (argc != 2) {
    _tprintf(_T("Tool to set the Goopdate ShutdownEvent.\n"));
    _tprintf(_T("Usage: SetShutDownEvent <true>\n"));
    _tprintf(_T("If arg=true the machine shutdown event is signalled.\n"));
    return -1;
  }

  bool is_machine = false;
  if (_tcsncmp(_T("true"), argv[1], ARRAYSIZE(_T("true"))) == 0) {
    is_machine = true;
  }

  omaha::NamedObjectAttributes attr;
  GetNamedObjectAttributes(omaha::kShutdownEvent, is_machine, &attr);
  // Manual reset=true and signaled=false
  scoped_handle shutdown_event(::CreateEvent(&attr.sa,
                                             true,
                                             false,
                                             attr.name));
  if (!shutdown_event) {
    DWORD error = GetLastError();
    _tprintf(_T("CreateEvent failed. error = %d\n"), error);
    return error;
  }

  if (!::SetEvent(get(shutdown_event))) {
    DWORD error = GetLastError();
    _tprintf(_T("SetEvent failed. error = %d\n"), error);
    return error;
  }
  _tprintf(_T("Done\n"));
  return 0;
}
