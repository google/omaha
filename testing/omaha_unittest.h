// Copyright 2010 Google Inc.
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

#ifndef OMAHA_TESTING_OMAHA_UNITTEST_H_
#define OMAHA_TESTING_OMAHA_UNITTEST_H_

#include <windows.h>

namespace omaha {

int RunTests(bool is_medium_or_large_test,
             bool load_resources,
             int argc,
             TCHAR** argv);

// These functions must be implemented by users of RunTests and linked into the
// test binary.

// Initializes the network if necessary for the test.
int InitializeNetwork();

// Performs and teardown of the network if necessary.
int DeinitializeNetwork();

}  // namespace omaha

#endif  // OMAHA_TESTING_OMAHA_UNITTEST_H_
