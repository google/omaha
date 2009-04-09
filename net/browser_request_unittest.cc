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
#include <winhttp.h>
#include "omaha/common/app_util.h"
#include "omaha/common/string.h"
#include "omaha/net/browser_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class BrowserRequestTest : public testing::Test {
 protected:
  BrowserRequestTest() : are_browser_objects_available(false) {
    BrowserRequest request;
    are_browser_objects_available = !request.objects_.empty();
  }

  void BrowserGet(const CString& url);
  void BrowserGet204(const CString& url);
  void BrowserDownload(const CString& url);

  bool are_browser_objects_available;
};

void BrowserRequestTest::BrowserGet(const CString& url) {
  if (!are_browser_objects_available) {
    return;
  }

  BrowserRequest browser_request;
  browser_request.set_url(url);
  EXPECT_HRESULT_SUCCEEDED(browser_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, browser_request.GetHttpStatusCode());

  CString response(Utf8BufferToWideChar(browser_request.GetResponse()));
  EXPECT_NE(-1, response.Find(_T("User-agent: *")));
  CString content_type;
  browser_request.QueryHeadersString(WINHTTP_QUERY_CONTENT_TYPE,
                                     NULL, &content_type);
  EXPECT_STREQ(_T("text/plain"), content_type);
  EXPECT_FALSE(browser_request.GetResponseHeaders().IsEmpty());
}

void BrowserRequestTest::BrowserGet204(const CString& url) {
  if (!are_browser_objects_available) {
    return;
  }

  BrowserRequest browser_request;
  browser_request.set_url(url);
  EXPECT_HRESULT_SUCCEEDED(browser_request.Send());
  EXPECT_EQ(HTTP_STATUS_NO_CONTENT, browser_request.GetHttpStatusCode());
  CString response(Utf8BufferToWideChar(browser_request.GetResponse()));
  EXPECT_EQ(-1, response.Find(_T("User-agent: *")));
  CString content_type;
  browser_request.QueryHeadersString(WINHTTP_QUERY_CONTENT_TYPE,
                                     NULL, &content_type);
  EXPECT_STREQ(_T("text/html"), content_type);
  EXPECT_FALSE(browser_request.GetResponseHeaders().IsEmpty());
}

void BrowserRequestTest::BrowserDownload(const CString& url) {
  if (!are_browser_objects_available) {
    return;
  }

  BrowserRequest browser_request;
  CString temp_dir = app_util::GetTempDir();
  CString temp_file;
  EXPECT_TRUE(::GetTempFileName(temp_dir, _T("tmp"), 0,
                                CStrBuf(temp_file, MAX_PATH)));
  browser_request.set_filename(temp_file);
  browser_request.set_url(url);

  // First request.
  ASSERT_HRESULT_SUCCEEDED(browser_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, browser_request.GetHttpStatusCode());
  browser_request.Close();

  // Second request.
  ASSERT_HRESULT_SUCCEEDED(browser_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, browser_request.GetHttpStatusCode());

  EXPECT_TRUE(::DeleteFile(temp_file));
}

// http get.
TEST_F(BrowserRequestTest, HttpGet) {
  BrowserGet(_T("http://www.google.com/robots.txt"));
}

// https get.
TEST_F(BrowserRequestTest, HttpsGet) {
  BrowserGet(_T("https://www.google.com/robots.txt"));
}

TEST_F(BrowserRequestTest, Download) {
  BrowserDownload(_T("http://dl.google.com/update2/UpdateData.bin"));
}

// http get 204.
TEST_F(BrowserRequestTest, HttpGet204) {
  BrowserGet204(_T("http://tools.google.com/service/check2"));
}

}   // namespace omaha

