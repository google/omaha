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

#include "omaha/base/app_util.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace vista {

// Exercises RunAsUser() with explorer token. For Vista, the call to
// StartProcessWithTokenOfProcess() will succeed only if the caller is SYSTEM.
TEST(VistaUtilsTest, StartProcessWithExplorerTokenTest) {
  CString path = ConcatenatePath(app_util::GetSystemDir(), _T("cmd.exe"));
  EnclosePath(&path);
  path += _T(" /c exit 702");
  uint32 pid(0);
  EXPECT_SUCCEEDED(GetExplorerPidForCurrentUserOrSession(&pid));

  HRESULT hr = StartProcessWithTokenOfProcess(pid, path);
  if (!vista_util::IsVistaOrLater()) {
    EXPECT_SUCCEEDED(hr);
    return;
  }

  bool is_system = false;
  EXPECT_SUCCEEDED(IsSystemProcess(&is_system));
  if (is_system) {
    EXPECT_SUCCEEDED(hr);
    return;
  }

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PRIVILEGE_NOT_HELD), hr);
}

// Exercises RunAsUser() with current user token.
TEST(VistaUtilsTest, RunAsCurrentUserTest) {
  CString path = ConcatenatePath(app_util::GetSystemDir(), _T("cmd.exe"));
  EnclosePath(&path);
  path += _T(" /c exit 702");
  EXPECT_SUCCEEDED(vista::RunAsCurrentUser(path));
}

}  // namespace vista

}  // namespace omaha

