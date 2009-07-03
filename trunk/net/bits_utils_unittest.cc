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
#include <winhttp.h>
#include <atlbase.h>
#include "omaha/net/bits_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(BitsUtilsTest, GetBitsManager) {
  CComPtr<IBackgroundCopyManager> bits_manager;
  ASSERT_HRESULT_SUCCEEDED(GetBitsManager(&bits_manager));
}

TEST(BitsUtilsTest, GetHttpStatusFromBitsError) {
  EXPECT_EQ(GetHttpStatusFromBitsError(BG_E_HTTP_ERROR_400),
            HTTP_STATUS_BAD_REQUEST);
  EXPECT_EQ(GetHttpStatusFromBitsError(BG_E_HTTP_ERROR_407),
            HTTP_STATUS_PROXY_AUTH_REQ);
  EXPECT_EQ(GetHttpStatusFromBitsError(BG_E_HTTP_ERROR_100),
            HTTP_STATUS_CONTINUE);
  EXPECT_EQ(GetHttpStatusFromBitsError(BG_E_HTTP_ERROR_505),
            HTTP_STATUS_VERSION_NOT_SUP);

  EXPECT_EQ(GetHttpStatusFromBitsError(-1), 0);
  EXPECT_EQ(GetHttpStatusFromBitsError(0), 0);
  EXPECT_EQ(GetHttpStatusFromBitsError(99), 0);
  EXPECT_EQ(GetHttpStatusFromBitsError(506), 0);
}

}   // namespace omaha

