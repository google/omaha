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

#include <windows.h>
#include <shellapi.h>

#include "omaha/testing/omaha_unittest.h"

// The entry point for the Omaha unit tests.
// We use main instead of _tmain since _tmain doesn't like being part of a
// static library...
int main(int unused_argc, char** unused_argv) {
  UNREFERENCED_PARAMETER(unused_argc);
  UNREFERENCED_PARAMETER(unused_argv);

  int argc = 0;
  WCHAR** argv = ::CommandLineToArgvW(::GetCommandLine(), &argc);
  return omaha::RunTests(false,  // is_medium_or_large_test.
                         false,  // load_resources.
                         argc,
                         argv);
}

namespace omaha {

// The network is not needed for small tests.

int InitializeNetwork() {
  return 0;
}

int DeinitializeNetwork() {
  return 0;
}

}  // namespace omaha
