// Copyright 2003-2009 Google Inc.
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
// Constants used for different timeouts in Common Installer

#ifndef OMAHA_COMMON_CONST_TIMEOUTS_H__
#define OMAHA_COMMON_CONST_TIMEOUTS_H__

namespace omaha {

// Timeout for tearing down thread
const int kMaxThreadDestructionTimeMs = 2000;

const int kRegisterExeTimeoutMs =                  120000;      // 2 minutes.

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_TIMEOUTS_H__

