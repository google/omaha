// Copyright 2007-2013 Google Inc.
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

#include <vector>
#include "base/scoped_ptr.h"

#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/environment_utils.h"
#include "omaha/base/error.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(EnvironmentBlockModifierTest, EBM_None) {
  EnvironmentBlockModifier eb_mod_none;
  EXPECT_TRUE(eb_mod_none.IsEmpty());

  std::vector<TCHAR> blk;
  eb_mod_none.Create(_T(""), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(), _T("")));

  blk.clear();
  eb_mod_none.Create(_T("a=1\0z=9\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(), _T("a=1\0z=9\0")));
}

TEST(EnvironmentBlockModifierTest, EBM_Create) {
  EnvironmentBlockModifier eb_mod;
  eb_mod.SetVar(_T("test"), _T("314"));
  eb_mod.SetVar(_T("MOD"), _T("gets overwritten by mod"));
  eb_mod.SetVar(_T("mod"), _T("has stuff"));
  EXPECT_FALSE(eb_mod.IsEmpty());

  std::vector<TCHAR> blk;
  eb_mod.Create(_T(""), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(),
                  _T("mod=has stuff\0test=314\0")));

  blk.clear();
  eb_mod.Create(_T("tEsT=overwritten\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(),
                  _T("mod=has stuff\0test=314\0")));

  blk.clear();
  eb_mod.Create(_T("a=1\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(),
                  _T("a=1\0mod=has stuff\0test=314\0")));

  blk.clear();
  eb_mod.Create(_T("z=0\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(),
                  _T("mod=has stuff\0test=314\0z=0\0")));

  blk.clear();
  eb_mod.Create(_T("mo=w\0MOD=3\0mOdE=auto\0MOdel=4\0Test1=z\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(),
                  _T("mo=w\0mod=has stuff\0mOdE=auto\0MOdel=4\0")
                  _T("test=314\0Test1=z\0")));

  blk.clear();
  eb_mod.Create(_T("=C:=C:\\\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(),
                  _T("=C:=C:\\\0mod=has stuff\0test=314\0")));
}

TEST(EnvironmentBlockModifierTest, EBM_Erase) {
  EnvironmentBlockModifier eb_mod_erase;
  eb_mod_erase.SetVar(_T("test"), _T(""));
  eb_mod_erase.SetVar(_T("mod"), _T(""));
  EXPECT_FALSE(eb_mod_erase.IsEmpty());

  std::vector<TCHAR> blk;
  eb_mod_erase.Create(_T(""), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(), _T("")));

  blk.clear();
  eb_mod_erase.Create(_T("mod=old value\0test=314\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(), _T("")));

  blk.clear();
  eb_mod_erase.Create(_T("a=3\0tESt=314\0z=last\0"), &blk);
  EXPECT_TRUE(CompareEnvironmentBlock(&blk.front(), _T("a=3\0z=last\0")));
}

TEST(EnvironmentBlockModifierTest, EBM_Real) {
  EnvironmentBlockModifier eb_mod;
  eb_mod.SetVar(_T("test"), _T("314"));
  eb_mod.SetVar(_T("mod"), _T("has stuff"));
  eb_mod.SetVar(_T("path"), _T(""));

  // The purpose of this test is to use "real life" environment block values,
  // and ensure smooth execution.
  std::vector<TCHAR> blk;
  EXPECT_TRUE(eb_mod.CreateForCurrentUser(&blk));
  EXPECT_GE(GetEnvironmentBlockLengthInTchar(&blk.front()),
            static_cast<size_t>(23));
}

}  // namespace omaha
