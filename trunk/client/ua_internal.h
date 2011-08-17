// Copyright 2008-2010 Google Inc.
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

#ifndef OMAHA_CLIENT_UA_INTERNAL_H_
#define OMAHA_CLIENT_UA_INTERNAL_H_

#include <windows.h>

namespace omaha {

class ProgramInstance;

namespace internal {

bool EnsureSingleUAProcess(bool is_machine, ProgramInstance** instance);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_CLIENT_UA_INTERNAL_H_
