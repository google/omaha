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

#include "omaha/net/winhttp_vtable.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(WinHttpVTableTest, WinHttpVTable) {
  WinHttpVTable winhttp;
  if (winhttp.Load()) {
    // Check to see if the winhttp module has already been loaded.
    bool was_loaded = ::GetModuleHandle(_T("winhttp")) ||
                      ::GetModuleHandle(_T("winhttp5"));
    EXPECT_TRUE(winhttp.IsLoaded());
    EXPECT_TRUE(winhttp.Load());
    EXPECT_TRUE(::GetModuleHandle(_T("winhttp")) ||
                ::GetModuleHandle(_T("winhttp5")));
    EXPECT_TRUE(winhttp.WinHttpCheckPlatform());
    winhttp.Unload();
    EXPECT_FALSE(winhttp.IsLoaded());
    if (!was_loaded) {
      // If the module was not loaded at the beginning of the test then it
      // should not be loaded now either.
      EXPECT_FALSE(::GetModuleHandle(_T("winhttp")) ||
                   ::GetModuleHandle(_T("winhttp5")));
    }
  }
}

}   // namespace omaha

