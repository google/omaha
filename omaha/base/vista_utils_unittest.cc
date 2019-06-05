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

#include <windows.h>
#include "omaha/base/app_util.h"
#include "omaha/base/path.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace vista {

// Exercises RunAsUser() with current user token.
TEST(VistaUtilsTest, RunAsCurrentUserTest) {
  CString cmd_path = ConcatenatePath(app_util::GetSystemDir(), _T("cmd.exe"));
  EnclosePath(&cmd_path);
  CString exit_path = cmd_path + _T(" /c exit 702");
  EXPECT_SUCCEEDED(vista::RunAsCurrentUser(exit_path, NULL, NULL));

  scoped_process process;
  EXPECT_SUCCEEDED(
      vista::RunAsCurrentUser(exit_path, NULL, address(process)));
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(get(process), 16 * kMsPerSec));
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(get(process), &exit_code));
  ASSERT_EQ(702, exit_code);
  reset(process);

  scoped_handle stdout_pipe;
  CString echo_path = cmd_path + _T(" /c \"echo Hello World\"");
  EXPECT_SUCCEEDED(vista::RunAsCurrentUser(echo_path,
                                           address(stdout_pipe),
                                           address(process)));
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(get(process), 16 * kMsPerSec));
  char buffer[32] = {0};
  DWORD bytes_read = 0;
  ASSERT_TRUE(::ReadFile(get(stdout_pipe),
                         buffer,
                         arraysize(buffer) - 1,
                         &bytes_read,
                         NULL));
  ASSERT_EQ(13, bytes_read);
  ASSERT_EQ(CString(_T("Hello World\r\n")),
            AnsiToWideString(buffer, bytes_read + 1));
}

}  // namespace vista

}  // namespace omaha

