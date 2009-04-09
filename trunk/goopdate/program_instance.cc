// Copyright 2005-2009 Google Inc.
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

#include "omaha/goopdate/program_instance.h"
#include "omaha/common/debug.h"

namespace omaha {

// The manner in which we prevent multiple instances of
// goopdate running in the user session is to
// use a local name for the singleton mutex, and
// grab the mutex till the end of our lifetime.
bool ProgramInstance::EnsureSingleInstance() {
  return CheckSingleInstance();
}

bool ProgramInstance::CheckSingleInstance() {
  ASSERT1(!mutex_name_.IsEmpty());

  reset(mutex_, ::CreateMutex(NULL, false, mutex_name_));
  if (!mutex_) {
    // We were not able to create the mutex instance for some reason.
    return false;
  }
  DWORD error = ::GetLastError();
  if (error == ERROR_ALREADY_EXISTS) {
    // The program instance is already running since the mutex already exists.
    return false;
  }
  return true;
}

}  // namespace omaha

