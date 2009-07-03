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
// dynamic link kernel 32 unittest

#include "omaha/common/dynamic_link_kernel32.h"
#include "omaha/common/system_info.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(DynamicLinkKernel32Test, DynamicLinkKernel32) {
  BOOL is64(0);
  ASSERT_TRUE(Kernel32::IsWow64Process(GetCurrentProcess(), &is64));
  ASSERT_EQ(static_cast<BOOL>(SystemInfo::IsRunningOn64Bit()), is64);
}

}  // namespace omaha
