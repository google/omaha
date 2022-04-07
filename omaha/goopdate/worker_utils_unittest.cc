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

#include <windows.h>
#include <atlstr.h>
#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/goopdate/worker_utils.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace worker_utils {

TEST(WorkerUtilsTest, FormatMessageForNetworkError) {
  const TCHAR* const kEnglish = _T("en");
  EXPECT_SUCCEEDED(ResourceManager::Create(
      false, app_util::GetCurrentModuleDirectory(), kEnglish));

  const TCHAR* const kTestAppName = _T("Test App");
  CString message;
  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NO_NETWORK,
                                               kEnglish,
                                               &message));
  EXPECT_STREQ(
      _T("Unable to connect to the Internet. If you use a firewall, please ")
      _T("allowlist ") MAIN_EXE_BASE_NAME _T(".exe."),
      message);

  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NETWORK_UNAUTHORIZED,
                                               kEnglish,
                                               &message));
  EXPECT_STREQ(
      _T("Unable to connect to the Internet. HTTP 401 Unauthorized. Please ")
      _T("check your proxy configuration."),
      message);

  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NETWORK_FORBIDDEN,
                                               kEnglish,
                                               &message));
  EXPECT_STREQ(
      _T("Unable to connect to the Internet. HTTP 403 Forbidden. Please check ")
      _T("your proxy configuration."),
      message);

  EXPECT_EQ(true,
            FormatMessageForNetworkError(GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED,
                                         kEnglish,
                                         &message));
  EXPECT_STREQ(
      _T("Unable to connect to the Internet. Proxy server requires ")
      _T("authentication."),
      message);

  EXPECT_EQ(false, FormatMessageForNetworkError(E_FAIL, kEnglish, &message));
  EXPECT_STREQ(
      _T("Unable to connect to the Internet. If you use a firewall, please ")
      _T("allowlist ") MAIN_EXE_BASE_NAME _T(".exe."),
      message);

  ResourceManager::Delete();
}

}  // namespace job_controller_utils

}  // namespace omaha
