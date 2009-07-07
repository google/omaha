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

// Since the net code is linked in as a lib, force the registration code to
// be a dependency, otherwise the linker is optimizing in out.
#pragma comment(linker, "/INCLUDE:_kRegisterWinHttp")

#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/omaha_version.h"
#include "omaha/net/http_client.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Test fixture, perhaps we need it later.
class HttpClientTest : public testing::Test {
 protected:
  HttpClientTest() {}

  static void SetUpTestCase() {}

  virtual void SetUp() {
    http_client_.reset(
        HttpClient::GetFactory().CreateObject(HttpClient::WINHTTP));
    ASSERT_TRUE(http_client_.get());
    ASSERT_SUCCEEDED(http_client_->Initialize());
  }

  virtual void TearDown() {
    http_client_.reset();
  }

  void GetUrl(const TCHAR* url, bool use_proxy);

  scoped_ptr<HttpClient> http_client_;
};

CString BuildUserAgent() {
  CString user_agent;
  user_agent.Format(_T("HttpClientTest version %s"), omaha::GetVersionString());
  return user_agent;
}

const TCHAR kTestUrlGet[]       = _T("http://www.google.com/robots.txt");
const TCHAR kTestSecureUrlGet[] = _T("https://www.google.com/robots.txt");

// If using a proxy is specified, the function does WPAD detection to get the
// name of the proxy to be used. The request goes direct if the WPAD fails.
void HttpClientTest::GetUrl(const TCHAR* url, bool use_proxy) {
  ASSERT_TRUE(url);

  CString server, path;
  int port = 0;
  ASSERT_SUCCEEDED(http_client_->CrackUrl(url,
                                          ICU_DECODE,
                                          NULL,
                                          &server,
                                          &port,
                                          &path,
                                          NULL));
  ASSERT_STREQ(server, _T("www.google.com"));
  ASSERT_STREQ(path, _T("/robots.txt"));

  HINTERNET session_handle = NULL;
  ASSERT_SUCCEEDED(http_client_->Open(BuildUserAgent(),
                                      HttpClient::kAccessTypeNoProxy,
                                      NULL,
                                      NULL,
                                      &session_handle));
  if (use_proxy) {
    HttpClient::AutoProxyOptions autoproxy_options = {0};
    autoproxy_options.flags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    autoproxy_options.auto_detect_flags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
                                          WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    autoproxy_options.auto_logon_if_challenged = true;
    HttpClient::ProxyInfo proxy_info = {0};
    http_client_->GetProxyForUrl(session_handle,
                                 url,
                                 &autoproxy_options,
                                 &proxy_info);
    if (proxy_info.proxy && wcslen(proxy_info.proxy)) {
      proxy_info.access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
      EXPECT_SUCCEEDED(http_client_->SetOption(session_handle,
                                               WINHTTP_OPTION_PROXY,
                                               &proxy_info,
                                               sizeof(proxy_info)));
    }
    ::GlobalFree(const_cast<wchar_t*>(proxy_info.proxy));
    ::GlobalFree(const_cast<wchar_t*>(proxy_info.proxy_bypass));
  }

  HINTERNET connection_handle = NULL;
  ASSERT_SUCCEEDED(http_client_->Connect(session_handle,
                                         server,
                                         port,
                                         &connection_handle));
  uint32 flags = port == 443 ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET request_handle = NULL;
  ASSERT_SUCCEEDED(http_client_->OpenRequest(connection_handle,
                                             _T("GET"),
                                             path,
                                             NULL,     // HTTP 1.1
                                             NULL,     // Referrer.
                                             NULL,     // Default accept types.
                                             flags,
                                             &request_handle));
  ASSERT_SUCCEEDED(http_client_->SendRequest(request_handle,
                                             NULL, 0, NULL, 0, 0));
  ASSERT_SUCCEEDED(http_client_->ReceiveResponse(request_handle));
  CString content_type;
  EXPECT_SUCCEEDED(http_client_->QueryHeadersString(request_handle,
                                                    WINHTTP_QUERY_CONTENT_TYPE,
                                                    NULL,
                                                    &content_type,
                                                    NULL));
  EXPECT_STREQ(content_type, _T("text/plain"));
  CString server_header;
  EXPECT_SUCCEEDED(http_client_->QueryHeadersString(request_handle,
                                                    WINHTTP_QUERY_SERVER,
                                                    NULL,
                                                    &server_header,
                                                    NULL));
  CString response;
  DWORD size = 0;
  do {
    // Use ReadData to determine when a response has been completely read.
    // Always allocate a buffer for ReadData even though QueryDataAvailable
    // might return 0 bytes available.
    ASSERT_SUCCEEDED(http_client_->QueryDataAvailable(request_handle, &size));
    std::vector<uint8> buf(size + 1);
    ASSERT_SUCCEEDED(http_client_->ReadData(request_handle,
                                            &buf.front(),
                                            buf.size(),
                                            &size));
    buf.resize(size);
    if (size) {
      response += CString(reinterpret_cast<char*>(&buf.front()), buf.size());
    }
  } while (size > 0);

  // Compare a little bit of the body.
  response.Truncate(10);
  ASSERT_STREQ(response, _T("User-agent"));

  ASSERT_SUCCEEDED(http_client_->Close(request_handle));
  ASSERT_SUCCEEDED(http_client_->Close(connection_handle));
  ASSERT_SUCCEEDED(http_client_->Close(session_handle));
}

TEST_F(HttpClientTest, Get) {
  GetUrl(kTestUrlGet, false);
}

TEST_F(HttpClientTest, SecureGet) {
  GetUrl(kTestSecureUrlGet, false);
}

TEST_F(HttpClientTest, ProxyGet) {
  GetUrl(kTestUrlGet, true);
}

TEST_F(HttpClientTest, ProxySecureGet) {
  GetUrl(kTestSecureUrlGet, true);
}

TEST_F(HttpClientTest, BuildRequestHeader) {
  ASSERT_STREQ(HttpClient::BuildRequestHeader(_T("foo"), _T("bar")),
               _T("foo: bar\r\n"));
}

TEST_F(HttpClientTest, GetStatusCodeClass) {
  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_CONTINUE),
            HttpClient::STATUS_CODE_INFORMATIONAL);

  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_OK),
            HttpClient::STATUS_CODE_SUCCESSFUL);

  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_PARTIAL_CONTENT),
            HttpClient::STATUS_CODE_SUCCESSFUL);

  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_AMBIGUOUS),
            HttpClient::STATUS_CODE_REDIRECTION);

  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_REDIRECT),
            HttpClient::STATUS_CODE_REDIRECTION);

  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_BAD_REQUEST),
            HttpClient::STATUS_CODE_CLIENT_ERROR);

  EXPECT_EQ(HttpClient::GetStatusCodeClass(HTTP_STATUS_SERVICE_UNAVAIL),
            HttpClient::STATUS_CODE_SERVER_ERROR);
}

TEST_F(HttpClientTest, CrackUrl) {
  CString scheme, server, path, query;
  int port = 0;
  ASSERT_SUCCEEDED(http_client_->CrackUrl(_T("http://host/path?query"),
                                          0,
                                          &scheme,
                                          &server,
                                          &port,
                                          &path,
                                          &query));
  ASSERT_STREQ(scheme, _T("http"));
  ASSERT_STREQ(server, _T("host"));
  ASSERT_EQ(port, INTERNET_DEFAULT_HTTP_PORT);
  ASSERT_STREQ(path, _T("/path"));
  ASSERT_STREQ(query, _T("?query"));

  ASSERT_SUCCEEDED(http_client_->CrackUrl(_T("http://host"),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &path,
                                          &query));
  ASSERT_STREQ(path, _T(""));
  ASSERT_STREQ(query, _T(""));
}

}  // namespace omaha

