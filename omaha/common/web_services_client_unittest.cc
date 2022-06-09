// Copyright 2008-2010 Google Inc.
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

#include "omaha/common/web_services_client.h"

#include "omaha/base/const_addresses.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/net/network_request.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

using ::testing::_;

namespace omaha {

// TODO(omaha): test the machine case.

// This test is parameterized for foreground/background boolean.
class WebServicesClientTest : public testing::Test,
                              public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    EXPECT_HRESULT_SUCCEEDED(
        ConfigManager::Instance()->GetUpdateCheckUrl(&update_check_url_));

    web_service_client_.reset(new WebServicesClient(false));

    update_request_.reset(xml::UpdateRequest::Create(false,
                                                     _T("unittest_sessionid"),
                                                     _T("unittest_instsource"),
                                                     CString()));
    update_response_.reset(xml::UpdateResponse::Create());
  }

  void TearDown() override {
    web_service_client_.reset();
  }

  NetworkRequest* GetNetworkRequest() const {
    return web_service_client_->network_request_.get();
  }

  CString FindHttpHeaderValue(const CString& all_headers,
                              const CString& search_name) {
    return WebServicesClient::FindHttpHeaderValue(all_headers, search_name);
  }

  void CaptureCustomHeaderValues() {
    return web_service_client_->CaptureCustomHeaderValues();
  }

  CString update_check_url_;

  std::unique_ptr<WebServicesClient> web_service_client_;
  std::unique_ptr<xml::UpdateRequest> update_request_;
  std::unique_ptr<xml::UpdateResponse> update_response_;
  bool reg_value_enable_ecdsa_exists_;
  DWORD ecdsa_enabled_;
};


INSTANTIATE_TEST_CASE_P(IsForeground, WebServicesClientTest, ::testing::Bool());

TEST_F(WebServicesClientTest, Send) {
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Initialize(update_check_url_,
                                                           HeadersVector(),
                                                           false));

  // Test sending a user update check request.
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Send(false,
                                                     update_request_.get(),
                                                     update_response_.get()));
  EXPECT_TRUE(web_service_client_->is_http_success());

  xml::response::Response response(update_response_->response());
  EXPECT_STREQ(_T("3.0"), response.protocol);

  NetworkRequest* network_request = GetNetworkRequest();

  CString cookie;
  EXPECT_HRESULT_FAILED(network_request->QueryHeadersString(
      WINHTTP_QUERY_FLAG_REQUEST_HEADERS | WINHTTP_QUERY_COOKIE,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &cookie));
  EXPECT_TRUE(cookie.IsEmpty());

  CString etag;
  EXPECT_HRESULT_FAILED(network_request->QueryHeadersString(
      WINHTTP_QUERY_ETAG, WINHTTP_HEADER_NAME_BY_INDEX, &etag));
  EXPECT_TRUE(etag.IsEmpty());
}

TEST_P(WebServicesClientTest, SendUsingCup) {
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Initialize(update_check_url_,
                                                           HeadersVector(),
                                                           true));

  // Check the initial value of the custom headers.
  EXPECT_EQ(-1, web_service_client_->http_xdaystart_header_value());
  EXPECT_EQ(-1, web_service_client_->http_xdaynum_header_value());

  // Test sending a user update check request.
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Send(GetParam(),
                                                     update_request_.get(),
                                                     update_response_.get()));
  EXPECT_TRUE(web_service_client_->is_http_success());

  xml::response::Response response(update_response_->response());
  EXPECT_STREQ(_T("3.0"), response.protocol);

  NetworkRequest* network_request = GetNetworkRequest();

  CString no_request_age_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      _T("X-RequestAge"),
      &no_request_age_header);

  EXPECT_STREQ(_T(""), no_request_age_header);

  CString interactive_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      kHeaderXInteractive,
      &interactive_header);

  EXPECT_STREQ(GetParam() ? _T("fg") : _T("bg"), interactive_header);

  CString app_ids_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      kHeaderXAppId,
      &app_ids_header);

  EXPECT_STREQ(_T(""), app_ids_header);

  CString updater_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      kHeaderXUpdater,
      &updater_header);

  CString expected_updater_header;
  SafeCStringAppendFormat(&expected_updater_header, _T("Omaha-%s"),
                                                    GetVersionString());
  EXPECT_STREQ(expected_updater_header, updater_header);

  // Check the custom headers after the response has been received.
  EXPECT_LT(0, web_service_client_->http_xdaystart_header_value());
  EXPECT_LT(0, web_service_client_->http_xdaynum_header_value());
}

TEST_F(WebServicesClientTest, SendForcingHttps) {
  // Skips the test if the update check URL is not https.
  if (!String_StartsWith(update_check_url_, kHttpsProtoScheme, true)) {
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Initialize(update_check_url_,
                                                           HeadersVector(),
                                                           true));

  EXPECT_TRUE(update_request_->IsEmpty());

  // Adds an application with non-empty tt_token to the update request.
  // This should prevent the network stack from replacing https with
  // CUP protocol.
  xml::request::App app;
  app.app_id = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
  app.iid    = _T("{00000000-0000-0000-0000-000000000000}");
  app.update_check.is_valid = true;
  app.update_check.tt_token = _T("Test TT token");
  update_request_->AddApp(app);
  app.app_id = _T("{E608D3AC-AA44-4754-A391-DA830AE78EA4}");
  app.iid    = _T("{00000000-0000-0000-0000-000000000000}");
  app.update_check.is_valid = true;
  app.update_check.tt_token = _T("");
  update_request_->AddApp(app);

  EXPECT_FALSE(update_request_->IsEmpty());
  EXPECT_TRUE(update_request_->has_tt_token());

  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Send(false,
                                                     update_request_.get(),
                                                     update_response_.get()));
  EXPECT_TRUE(web_service_client_->is_http_success());

  NetworkRequest* network_request = GetNetworkRequest();

  CString app_ids_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      kHeaderXAppId,
      &app_ids_header);

  EXPECT_STREQ(_T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2},")
               _T("{E608D3AC-AA44-4754-A391-DA830AE78EA4}"),
               app_ids_header);

  // Do a couple of checks on the parsing of the response.
  xml::response::Response response(update_response_->response());
  EXPECT_STREQ(_T("3.0"), response.protocol);
  ASSERT_EQ(2, response.apps.size());
  EXPECT_STREQ(_T("error-unknownApplication"), response.apps[0].status);
  EXPECT_STREQ(_T("error-unknownApplication"), response.apps[1].status);
}

TEST_F(WebServicesClientTest, SendWithCustomHeader) {
  HeadersVector headers;
  headers.push_back(std::make_pair(_T("X-RequestAge"), _T("200")));

  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Initialize(update_check_url_,
                                                           headers,
                                                           true));

  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Send(false,
                                                     update_request_.get(),
                                                     update_response_.get()));
  EXPECT_TRUE(web_service_client_->is_http_success());

  xml::response::Response response(update_response_->response());
  EXPECT_STREQ(_T("3.0"), response.protocol);

  NetworkRequest* network_request = GetNetworkRequest();

  CString request_age_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      _T("X-RequestAge"),
      &request_age_header);

  EXPECT_STREQ(_T("200"), request_age_header);
}

TEST_P(WebServicesClientTest, SendString) {
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Initialize(update_check_url_,
                                                           HeadersVector(),
                                                           false));

  // Test sending a user update check request.
  CString request_string =
    _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
    _T("<request protocol=\"3.0\" testsource=\"dev\"></request>");
  std::unique_ptr<xml::UpdateResponse> response(xml::UpdateResponse::Create());
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->SendString(GetParam(),
                                                           &request_string,
                                                           response.get()));
  EXPECT_TRUE(web_service_client_->is_http_success());

  NetworkRequest* network_request = GetNetworkRequest();

  CString interactive_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      kHeaderXInteractive,
      &interactive_header);
  EXPECT_STREQ(GetParam() ? _T("fg") : _T("bg"), interactive_header);

  CString updater_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      kHeaderXUpdater,
      &updater_header);

  CString expected_updater_header;
  SafeCStringAppendFormat(&expected_updater_header, _T("Omaha-%s"),
                                                    GetVersionString());
  EXPECT_STREQ(expected_updater_header, updater_header);
}

TEST_F(WebServicesClientTest, SendStringWithCustomHeader) {
  HeadersVector headers;
  headers.push_back(std::make_pair(_T("X-FooBar"), _T("424")));

  EXPECT_HRESULT_SUCCEEDED(web_service_client_->Initialize(update_check_url_,
                                                           headers,
                                                           false));

  // Test sending a user update check request.
  CString request_string =
    _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
    _T("<request protocol=\"3.0\" testsource=\"dev\"></request>");
  std::unique_ptr<xml::UpdateResponse> response(xml::UpdateResponse::Create());
  EXPECT_HRESULT_SUCCEEDED(web_service_client_->SendString(false,
                                                           &request_string,
                                                           response.get()));
  EXPECT_TRUE(web_service_client_->is_http_success());

  NetworkRequest* network_request = GetNetworkRequest();

  CString foobar_header;
  network_request->QueryHeadersString(
      WINHTTP_QUERY_CUSTOM | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      _T("X-FooBar"),
      &foobar_header);

  EXPECT_STREQ(_T("424"), foobar_header);
}

TEST_F(WebServicesClientTest, FindHttpHeaderValue) {
  const CString headers(_T("HTTP/1.0 200 OK\r\n")
                        _T("Date: Thu, 09 Aug 2012 19:27:58 GMT\r\n")
                        _T("Pragma: no-cache\r\n")
                        _T("Content-Type: text/xml; charset=UTF-8\r\n")
                        _T("X-Frame-Options: SAMEORIGIN\r\n")
                        _T("TestNumber: 12345\r\n")
                        _T("\r\n"));
  // Finding header values is case-insensitive.
  EXPECT_STREQ(_T(""), FindHttpHeaderValue(_T(""), _T("NoInput")));
  EXPECT_STREQ(_T(""), FindHttpHeaderValue(headers, _T("NoSuchKey")));
  EXPECT_STREQ(_T(""), FindHttpHeaderValue(headers, _T("HTTP")));
  EXPECT_STREQ(_T("12345"), FindHttpHeaderValue(headers, _T("TestNumber")));
  EXPECT_STREQ(_T("12345"), FindHttpHeaderValue(headers, _T("Testnumber")));
  EXPECT_STREQ(_T("no-cache"), FindHttpHeaderValue(headers, _T("Pragma")));
}

}  // namespace omaha

