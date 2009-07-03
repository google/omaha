// Copyright 2008-2009 Google Inc.
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

#include <shlobj.h>
#include "omaha/common/vistautil.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace vista_util {

TEST(VistaUtilTest, IsUserAdmin) {
  bool is_admin = !!::IsUserAnAdmin();
  EXPECT_EQ(is_admin, vista_util::IsUserAdmin());
}

// Tests the code returns false if not Vista or later.
TEST(VistaUtilTest, IsUACDisabled) {
  bool is_uac_disabled = IsUACDisabled();
  bool is_vista_or_later = vista_util::IsVistaOrLater();
  if (!is_vista_or_later) {
    EXPECT_FALSE(is_uac_disabled);
  }
}

}  // namespace vista_util

}  // namespace omaha

