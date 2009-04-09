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

//
// Tests Get over http and https, using direct connection and wpad proxy.
//
// TODO(omaha): missing Post unit tests

#include <windows.h>
#include <winhttp.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/error.h"
#include "omaha/common/string.h"
#include "omaha/net/network_config.h"
#include "omaha/net/simple_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class SimpleRequestTest : public testing::Test {
 protected:
  SimpleRequestTest() {}

  void SimpleGet(const CString& url, const Config& config);

  void SimpleGetHostNotFound(const CString& url, const Config& config);

  void SimpleGetFileNotFound(const CString& url, const Config& config);

  void SimpleGetRedirect(const CString& url, const Config& config);

  void PrepareRequest(const CString& url,
                      const Config& config,
                      SimpleRequest* simple_request);
};

void SimpleRequestTest::PrepareRequest(const CString& url,
                                       const Config& config,
                                       SimpleRequest* simple_request) {
  ASSERT_TRUE(simple_request);
  HINTERNET handle = NetworkConfig::Instance().session().session_handle;
  simple_request->set_session_handle(handle);
  simple_request->set_url(url);
  simple_request->set_network_configuration(config);
}

void SimpleRequestTest::SimpleGet(const CString& url, const Config& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);
  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, simple_request.GetHttpStatusCode());
  CString response = Utf8BufferToWideChar(simple_request.GetResponse());

  // robots.txt response contains "User-agent: *". This is not the "User-Agent"
  // http header.
  EXPECT_NE(-1, response.Find(_T("User-agent: *")));
  CString content_type;
  simple_request.QueryHeadersString(WINHTTP_QUERY_CONTENT_TYPE,
                                    NULL, &content_type);
  EXPECT_STREQ(_T("text/plain"), content_type);
  CString server;
  simple_request.QueryHeadersString(WINHTTP_QUERY_CUSTOM,
                                    _T("Server"), &server);
  EXPECT_STREQ(_T("gws"), server);
  EXPECT_FALSE(simple_request.GetResponseHeaders().IsEmpty());

  // Check the user agent went out with the request.
  CString user_agent;
  simple_request.QueryHeadersString(
      WINHTTP_QUERY_FLAG_REQUEST_HEADERS | WINHTTP_QUERY_USER_AGENT,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &user_agent);
  EXPECT_STREQ(simple_request.user_agent(), user_agent);
}

void SimpleRequestTest::SimpleGetHostNotFound(const CString& url,
                                              const Config& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);

  HRESULT hr = simple_request.Send();
  int status_code = simple_request.GetHttpStatusCode();

  if (config.auto_detect) {
    // When the http host is not found, the proxy server usually returns 503.
    // When the https host is not found, the proxy server returns 404. This may
    // result in a flaky unit test but it's the best it can be done for now.
    EXPECT_HRESULT_SUCCEEDED(hr);
    if (String_StartsWith(url, kHttpsProtoScheme, true)) {
      EXPECT_EQ(HTTP_STATUS_NOT_FOUND, status_code);
    } else {
      EXPECT_EQ(HTTP_STATUS_SERVICE_UNAVAIL, status_code);
    }
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_WINHTTP_NAME_NOT_RESOLVED), hr);
    EXPECT_EQ(0, status_code);
  }
}

void SimpleRequestTest::SimpleGetFileNotFound(const CString& url,
                                              const Config& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);
  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());
  EXPECT_EQ(HTTP_STATUS_NOT_FOUND, simple_request.GetHttpStatusCode());
}

void SimpleRequestTest::SimpleGetRedirect(const CString& url,
                                          const Config& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);
  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, simple_request.GetHttpStatusCode());
}

//
// http tests.
//
// http get, direct connection.
TEST_F(SimpleRequestTest, HttpGetDirect) {
  SimpleGet(_T("http://www.google.com/robots.txt"), Config());
}

// http get, direct connection, negative test.
TEST_F(SimpleRequestTest, HttpGetDirectHostNotFound) {
  SimpleGetHostNotFound(_T("http://no_such_host.google.com/"), Config());
}

// http get, direct connection, negative test.
TEST_F(SimpleRequestTest, HttpGetDirectFileNotFound) {
  SimpleGetFileNotFound(_T("http://tools.google.com/no_such_file"), Config());
}

// http get, proxy wpad.
TEST_F(SimpleRequestTest, HttpGetProxy) {
  Config config;
  config.auto_detect = true;
  SimpleGet(_T("http://www.google.com/robots.txt"), config);
}

// http get, proxy wpad, negative test.
TEST_F(SimpleRequestTest, HttpGetProxyHostNotFound) {
  Config config;
  config.auto_detect = true;
  SimpleGetHostNotFound(_T("http://no_such_host.google.com/"), config);
}

// http get, proxy wpad.
TEST_F(SimpleRequestTest, HttpGetProxyFileNotFound) {
  Config config;
  config.auto_detect = true;
  SimpleGetFileNotFound(_T("http://tools.google.com/no_such_file"), config);
}


//
// https tests.
//
// https get, direct.
TEST_F(SimpleRequestTest, HttpsGetDirect) {
  SimpleGet(_T("https://www.google.com/robots.txt"), Config());
}

// https get, direct, negative test.
TEST_F(SimpleRequestTest, HttpsGetDirectHostNotFound) {
  SimpleGetHostNotFound(_T("https://no_such_host.google.com/"), Config());
}

// https get, direct connection, negative test.
TEST_F(SimpleRequestTest, HttpsGetDirectFileNotFound) {
  SimpleGetFileNotFound(_T("https://tools.google.com/no_such_file"), Config());
}

// https get, proxy wpad.
TEST_F(SimpleRequestTest, HttpsGetProxy) {
  Config config;
  config.auto_detect = true;
  SimpleGet(_T("https://www.google.com/robots.txt"), config);
}

// https get, proxy wpad, negative test.
TEST_F(SimpleRequestTest, HttpsGetProxyHostNotFound) {
  Config config;
  config.auto_detect = true;
  SimpleGetHostNotFound(_T("https://no_such_host.google.com/"), config);
}

// https get, proxy wpad, negative test.
TEST_F(SimpleRequestTest, HttpsGetProxyFileNotFound) {
  Config config;
  config.auto_detect = true;
  SimpleGetFileNotFound(_T("https://tools.google.com/no_such_file"), config);
}

// Should not be able to reuse the object once canceled, even if closed.
TEST_F(SimpleRequestTest, Cancel_CannotReuse) {
  SimpleRequest simple_request;
  PrepareRequest(_T("http:\\foo\\"), Config(), &simple_request);
  EXPECT_HRESULT_SUCCEEDED(simple_request.Cancel());
  EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED, simple_request.Send());
  EXPECT_HRESULT_SUCCEEDED(simple_request.Close());
  EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED, simple_request.Send());
}

// Http get request should follow redirects. The url below redirects to
// https://www.google.com/service/update2/oneclick and then it returns
// 200 OK and some xml body.
TEST_F(SimpleRequestTest, HttpGet_Redirect) {
  SimpleGetRedirect(_T("http://www.google.com/service/update2/oneclick"),
                    Config());
}

}  // namespace omaha

