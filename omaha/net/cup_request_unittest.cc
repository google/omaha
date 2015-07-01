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

// This unit test is hardcoded to run against production servers only.

#include <iostream>
#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/base/constants.h"
#include "omaha/base/vistautil.h"
#include "omaha/net/cup_request.h"
#include "omaha/net/network_config.h"
#include "omaha/net/simple_request.h"
#include "omaha/net/urlmon_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR* const kGetUrl =
    _T("http://tools.google.com/service/update2/id");
const TCHAR* const kGetUrlNoResponseBody =
    _T("http://tools.google.com/service/update2/nil");
const TCHAR* const kPostUrl =
    _T("http://tools.google.com/service/update2");
const uint8 kRequestBuffer[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><o:gupdate xmlns:o=\"http://www.google.com/update2/request\" protocol=\"2.0\" version=\"1.2.1.0\" ismachine=\"1\" testsource=\"dev\"><o:os platform=\"win\" version=\"5.1\" sp=\"Service Pack 2\"/><o:app appid=\"{52820187-5605-4C18-AA51-8BD0A1209C8C}\" version=\"1.1.1.3\" lang=\"abc\" client=\"{0AF52D61-9958-4fea-9B29-CDD9DCDBB145}\" iid=\"{F723495F-8ACF-4746-8240-643741C797B5}\"><o:event eventtype=\"1\" eventresult=\"1\" errorcode=\"0\" previousversion=\"1.0.0.0\"/></o:app></o:gupdate>";  // NOLINT

class CupRequestTest : public testing::Test {
 protected:
  CupRequestTest() {}

  static void SetUpTestCase() {
    NetworkConfig* network_config = NULL;
    EXPECT_HRESULT_SUCCEEDED(
        NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));
    network_config->SetCupCredentials(NULL);

    // For debugging purposes, try FF configuration first.
    network_config->Clear();
    network_config->Add(new FirefoxProxyDetector());
    EXPECT_HRESULT_SUCCEEDED(network_config->Detect());
  }

  static void TearDownTestCase() {
    NetworkConfig* network_config = NULL;
    EXPECT_HRESULT_SUCCEEDED(
        NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));
    network_config->SetCupCredentials(NULL);
    network_config->Clear();
  }

  void DoRequest(HttpRequestInterface* contained_request,
                 const CString& url,
                 const uint8* request_buffer,
                 size_t request_buffer_lenght) {
    scoped_ptr<CupRequest> http_request(new CupRequest(contained_request));

    NetworkConfig* network_config = NULL;
    EXPECT_HRESULT_SUCCEEDED(
        NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));
    network_config->SetCupCredentials(NULL);

    // For debugging purposes, try FF configuration first.
    network_config->Clear();
    network_config->Add(new FirefoxProxyDetector());
    EXPECT_HRESULT_SUCCEEDED(network_config->Detect());

    // This will use Firefox or direct connection if FF is not installed.
    std::vector<ProxyConfig> proxy_configurations(
        network_config->GetConfigurations());
    proxy_configurations.push_back(ProxyConfig());

    // Clear CUP credentials.
    network_config->SetCupCredentials(NULL);

    HINTERNET handle = network_config->session().session_handle;
    http_request->set_session_handle(handle);
    http_request->set_proxy_configuration(proxy_configurations[0]);
    http_request->set_url(url);
    if (request_buffer) {
      http_request->set_request_buffer(request_buffer, request_buffer_lenght);
    }

    // First request goes with a fresh set of client credentials.
    EXPECT_HRESULT_SUCCEEDED(http_request->Send());
    int http_status = http_request->GetHttpStatusCode();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);
    std::vector<uint8> response(http_request->GetResponse());

    // Second request goes with cached client credentials.
    EXPECT_HRESULT_SUCCEEDED(http_request->Send());
    http_status = http_request->GetHttpStatusCode();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);
    response = http_request->GetResponse();

    // After each test run we should have some {sk, c} persisted in the
    // registry. The CUP credentials are written back when the CUP request
    // that has created them is destroyed.
    http_request.reset();
    CupCredentials cup_creds;
    EXPECT_HRESULT_SUCCEEDED(network_config->GetCupCredentials(&cup_creds));
    EXPECT_FALSE(cup_creds.sk.empty());
    EXPECT_FALSE(cup_creds.c.IsEmpty());

    network_config->SetCupCredentials(NULL);
    EXPECT_HRESULT_FAILED(network_config->GetCupCredentials(&cup_creds));
  }
};

TEST_F(CupRequestTest, GetSimpleRequest) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  DoRequest(new SimpleRequest, kGetUrl, NULL, 0);
  DoRequest(new SimpleRequest, kGetUrlNoResponseBody, NULL, 0);
}

TEST_F(CupRequestTest, GetUrlmonRequest) {
  DoRequest(new UrlmonRequest, kGetUrl, NULL, 0);
  DoRequest(new UrlmonRequest, kGetUrlNoResponseBody, NULL, 0);
}

TEST_F(CupRequestTest, PostSimpleRequest) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  DoRequest(new SimpleRequest, kPostUrl,
            kRequestBuffer, arraysize(kRequestBuffer) - 1);
}

TEST_F(CupRequestTest, PostUrlmonRequest) {
  DoRequest(new UrlmonRequest, kPostUrl,
            kRequestBuffer, arraysize(kRequestBuffer) - 1);
}

}   // namespace omaha

