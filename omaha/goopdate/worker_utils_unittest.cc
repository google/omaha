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
      _T("The installer could not connect to the Internet. Ensure that your ")
      _T("computer is connected to the Internet and your firewall allows ")
      _T("GoogleUpdate.exe to connect then try again."),
      message);

  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NETWORK_UNAUTHORIZED,
                                               kEnglish,
                                               &message));
  EXPECT_STREQ(
      _T("The installer could not connect to the Internet because of ")
      _T("an HTTP 401 Unauthorized response. This is likely a proxy ")
      _T("configuration issue. Please configure the proxy server to allow ")
      _T("network access and try again or contact your network administrator."),
      message);

  EXPECT_EQ(true, FormatMessageForNetworkError(GOOPDATE_E_NETWORK_FORBIDDEN,
                                               kEnglish,
                                               &message));
  EXPECT_STREQ(
      _T("The installer could not connect to the Internet because of ")
      _T("an HTTP 403 Forbidden response. This is likely a proxy ")
      _T("configuration issue. Please configure the proxy server to allow ")
      _T("network access and try again or contact your network administrator."),
      message);

  EXPECT_EQ(true,
            FormatMessageForNetworkError(GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED,
                                         kEnglish,
                                         &message));
  EXPECT_STREQ(
      _T("The installer could not connect to the Internet because a ")
      _T("proxy server required user authentication. Please configure the ")
      _T("proxy server to allow network access and try again or contact your ")
      _T("network administrator."),
      message);

  EXPECT_EQ(false, FormatMessageForNetworkError(E_FAIL, kEnglish, &message));
  EXPECT_STREQ(
      _T("The installer could not connect to the Internet. Ensure that your ")
      _T("computer is connected to the Internet and your firewall allows ")
      _T("GoogleUpdate.exe to connect then try again."),
      message);

  ResourceManager::Delete();
}

}  // namespace job_controller_utils

}  // namespace omaha
