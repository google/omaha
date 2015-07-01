// Copyright 2011 Google Inc.
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

// The CrashHandler is a long-lived Omaha process. It runs one instance for the
// machine and one instance for each user session, including console and TS
// sessions. If the user has turned off crash reporting, this process will exit
// shortly after startup.

#include <windows.h>
#include <tchar.h>
#include "omaha/base/omaha_version.h"
#include "omaha/base/utils.h"
#include "omaha/crashhandler/crash_handler.h"

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int) {
  bool is_system_process = false;

  omaha::InitializeVersionFromModule(NULL);
  HRESULT hr = omaha::IsSystemProcess(&is_system_process);
  if (FAILED(hr)) {
    return hr;
  }

  return omaha::CrashHandler().Main(is_system_process);
}
