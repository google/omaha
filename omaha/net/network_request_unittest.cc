// Copyright 2007-2010 Google Inc.
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
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/queue_timer.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/cup_ecdsa_request.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class NetworkRequestTest
    : public testing::Test,
      public NetworkRequestCallback {
 protected:
  NetworkRequestTest() {}

  static void SetUpTestCase() {
    // Initialize the detection chain: GoogleProxy, FireFox if it is the
    // default browser, and IE.
    NetworkConfig* network_config = NULL;
    EXPECT_HRESULT_SUCCEEDED(
      NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));

    network_config->Clear();
    network_config->Add(new UpdateDevProxyDetector);
    network_config->Add(new IEWPADProxyDetector);
    network_config->Add(new IEPACProxyDetector);
    network_config->Add(new IENamedProxyDetector);
    network_config->Add(new DefaultProxyDetector);

    vista::GetLoggedOnUserToken(&token_);
  }

  static void TearDownTestCase() {
    if (token_) {
      ::CloseHandle(token_);
    }

    NetworkConfig* network_config = NULL;
    EXPECT_HRESULT_SUCCEEDED(
      NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));

    network_config->Clear();
  }

  virtual void SetUp() {
    NetworkConfig* network_config = NULL;
    EXPECT_HRESULT_SUCCEEDED(
      NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));

    network_request_.reset(new NetworkRequest(network_config->session()));
    network_request_->set_retry_delay_jitter(0);
  }

  virtual void TearDown() {}

  virtual void OnProgress(int bytes, int bytes_total, int, const TCHAR*) {
    UNREFERENCED_PARAMETER(bytes);
    UNREFERENCED_PARAMETER(bytes_total);
    NET_LOG(L3, (_T("[downloading %d of %d]"), bytes, bytes_total));
  }

  virtual void OnRequestBegin() {
    NET_LOG(L3, (_T("[download starts]")));
  }

  virtual void OnRequestRetryScheduled(time64 next_retry_time) {
    UNREFERENCED_PARAMETER(next_retry_time);

    time64 now = GetCurrent100NSTime();
    ASSERT1(next_retry_time > now);

    NET_LOG(L3, (_T("\n[Download will retry in %d seconds]\n"),
                 CeilingDivide(next_retry_time - now, kSecsTo100ns)));
  }

  static void CancelCallback(QueueTimer* queue_timer) {
    ASSERT_TRUE(queue_timer);
    ASSERT_TRUE(queue_timer->ctx());
    void* ctx = queue_timer->ctx();
    NetworkRequestTest* test = static_cast<NetworkRequestTest*>(ctx);

    const TCHAR* msg = _T("CancelCallback");
    ASSERT_HRESULT_SUCCEEDED(test->network_request_->Cancel());
  }

  // http get.
  void HttpGetHelper() {
    std::vector<uint8> response;
    CString url = _T("http://www.google.com/robots.txt");
    network_request_->set_num_retries(2);
    EXPECT_HRESULT_SUCCEEDED(network_request_->Get(url, &response));

    int http_status = network_request_->http_status_code();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);
  }

  // https get.
  void HttpsGetHelper() {
    std::vector<uint8> response;
    CString url = _T("https://www.google.com/robots.txt");
    network_request_->set_num_retries(2);
    EXPECT_HRESULT_SUCCEEDED(network_request_->Get(url, &response));

    int http_status = network_request_->http_status_code();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);
  }

  // http post.
  void HttpPostHelper() {
    std::vector<uint8> response;

    CString url = _T("http://tools.google.com/service/update2");
    // Post a buffer.
    const uint8 request[] = "<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" testsource=\"dev\"/>";  // NOLINT
    network_request_->set_num_retries(2);
    EXPECT_HRESULT_SUCCEEDED(network_request_->Post(url,
                                                    request,
                                                    arraysize(request) - 1,
                                                    &response));

    int http_status = network_request_->http_status_code();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);

    // Post an UTF8 string.
    CStringA utf8_request(reinterpret_cast<const char*>(request));
    EXPECT_HRESULT_SUCCEEDED(network_request_->PostUtf8String(url,
                                                              utf8_request,
                                                              &response));
    http_status = network_request_->http_status_code();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);

    // Post a Unicode string.
    CString unicode_request(reinterpret_cast<const char*>(request));
    EXPECT_HRESULT_SUCCEEDED(network_request_->PostString(url,
                                                          unicode_request,
                                                          &response));
    http_status = network_request_->http_status_code();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);
  }

  // Download http file.
  void DownloadHelper() {
    CString url = _T("http://dl.google.com/update2/UpdateData.bin");

    CString temp_file = GetTempFilename(_T("tmp"));
    ASSERT_FALSE(temp_file.IsEmpty());

    network_request_->set_num_retries(2);
    network_request_->set_low_priority(true);
    network_request_->set_callback(this);
    EXPECT_HRESULT_SUCCEEDED(network_request_->DownloadFile(url, temp_file));
    EXPECT_TRUE(::DeleteFile(temp_file));

    int http_status = network_request_->http_status_code();
    EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                http_status == HTTP_STATUS_PARTIAL_CONTENT);
  }

  void MultipleRequestsHelper() {
    std::vector<uint8> response;

    CString url = _T("http://tools.google.com/service/update2");
    const uint8 request[] = "<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" testsource=\"dev\"/>";  // NOLINT
    for (size_t i = 0; i != 3; ++i) {
      EXPECT_HRESULT_SUCCEEDED(network_request_->Post(url,
                                                      request,
                                                      arraysize(request) - 1,
                                                      &response));

      int http_status = network_request_->http_status_code();
      EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
                  http_status == HTTP_STATUS_PARTIAL_CONTENT);
    }
  }

  void PostRequestHelper() {
    std::vector<uint8> response;
    CString url = _T("http://tools.google.com/service/update2");
    CString request = _T("<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" testsource=\"dev\"/>");  // NOLINT
    EXPECT_HRESULT_SUCCEEDED(network_request_->PostString(url,
                                                          request,
                                                          &response));
  }

  void PostRequestNegativeTestHelper() {
    std::vector<uint8> response;
    CString url = _T("http://no_such_host.google.com/service/update2");
    CString request = _T("<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" testsource=\"dev\"/>");  // NOLINT
    EXPECT_HRESULT_FAILED(network_request_->PostString(url,
                                                       request,
                                                       &response));
  }

  void RetriesNegativeTestHelper() {
    // Try a direct connection to a non-existent host and keep retrying until
    // the retries are used up. Urlmon request is using IE's settings.
    // Therefore, it is possible a proxy is used. In this case, the http
    // response is '503 Service Unavailable'.
    ProxyConfig config;
    network_request_->set_proxy_configuration(&config);
    network_request_->set_num_retries(2);
    network_request_->set_time_between_retries(10);   // 10 miliseconds.
    std::vector<uint8> response;

    CString url = _T("http://nohost/nofile");

    // One request plus 2 retries after 10 and 20 miliseconds respectively.
    HRESULT hr = network_request_->Get(url, &response);
    EXPECT_TRUE(hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_NAME_NOT_RESOLVED) ||
                hr == INET_E_RESOURCE_NOT_FOUND ||
                hr == HRESULTFromHttpStatusCode(503));
  }

  void CancelTest_GetHelper() {
    HANDLE timer_queue = ::CreateTimerQueue();
    ASSERT_TRUE(timer_queue);
    ON_SCOPE_EXIT(::DeleteTimerQueueEx, timer_queue, INVALID_HANDLE_VALUE);

    QueueTimer queue_timer(timer_queue,
                           &NetworkRequestTest::CancelCallback,
                           this);
    ASSERT_HRESULT_SUCCEEDED(queue_timer.Start(200, 0, WT_EXECUTEONLYONCE));

    // Try a direct connection to a non-existent host and keep retrying until
    // canceled by the timer.
    ProxyConfig config;
    network_request_->set_proxy_configuration(&config);
    network_request_->set_num_retries(10);
    network_request_->set_time_between_retries(10);  // 10 miliseconds.
    std::vector<uint8> response;

    CString url = _T("http://nohost/nofile");

    EXPECT_EQ(GOOPDATE_E_CANCELLED,
              network_request_->Get(url, &response));

    EXPECT_EQ(GOOPDATE_E_CANCELLED,
              network_request_->Get(url, &response));
  }

  std::unique_ptr<NetworkRequest> network_request_;
  static HANDLE token_;
};

HANDLE NetworkRequestTest::token_ = NULL;

// http get.
TEST_F(NetworkRequestTest, HttpGet) {
  network_request_->AddHttpRequest(new SimpleRequest);
  HttpGetHelper();
}

// https get.
TEST_F(NetworkRequestTest, HttpsGet) {
  network_request_->AddHttpRequest(new SimpleRequest);
  HttpsGetHelper();
}

// http post.
TEST_F(NetworkRequestTest, HttpPost) {
  network_request_->AddHttpRequest(new CupEcdsaRequest(new SimpleRequest));
  network_request_->AddHttpRequest(new SimpleRequest);
  HttpPostHelper();
}

// Download http file.
TEST_F(NetworkRequestTest, Download) {
  BitsRequest* bits_request(new BitsRequest);
  // Bits specific settings.
  //
  // Hardcode for now the min value, just to see how it works.
  // TODO(omaha): expose properties to NetworkRequest.
  bits_request->set_minimum_retry_delay(60);
  bits_request->set_no_progress_timeout(5);

  network_request_->AddHttpRequest(bits_request);
  network_request_->AddHttpRequest(new SimpleRequest);
  DownloadHelper();
}

TEST_F(NetworkRequestTest, MultipleRequests) {
  network_request_->AddHttpRequest(new CupEcdsaRequest(new SimpleRequest));
  MultipleRequestsHelper();
}

TEST_F(NetworkRequestTest, PostRequest) {
  network_request_->AddHttpRequest(new SimpleRequest);
  PostRequestHelper();
}

TEST_F(NetworkRequestTest, PostRequestNegativeTest) {
  network_request_->AddHttpRequest(new SimpleRequest);
  PostRequestNegativeTestHelper();
}

TEST_F(NetworkRequestTest, RetriesNegativeTest) {
  network_request_->AddHttpRequest(new SimpleRequest);
  RetriesNegativeTestHelper();
}

// Network request can't be reused once canceled.
TEST_F(NetworkRequestTest, CancelTest_CannotReuse) {
  network_request_->Cancel();
  std::vector<uint8> response;

  CString url = _T("https://www.google.com/robots.txt");
  EXPECT_EQ(GOOPDATE_E_CANCELLED,
            network_request_->Get(url, &response));
}

TEST_F(NetworkRequestTest, CancelTest_DownloadFile) {
  HANDLE timer_queue = ::CreateTimerQueue();
  ASSERT_TRUE(timer_queue);
  ON_SCOPE_EXIT(::DeleteTimerQueueEx, timer_queue, INVALID_HANDLE_VALUE);

  QueueTimer queue_timer(timer_queue,
                         &NetworkRequestTest::CancelCallback,
                         this);
  ASSERT_HRESULT_SUCCEEDED(queue_timer.Start(200, 0, WT_EXECUTEONLYONCE));

  // Try a direct connection to a non-existent host and keep retrying until
  // canceled by the timer.
  ProxyConfig config;
  network_request_->set_proxy_configuration(&config);

  BitsRequest* bits_request(new BitsRequest);
  bits_request->set_minimum_retry_delay(60);
  bits_request->set_no_progress_timeout(5);
  network_request_->AddHttpRequest(bits_request);
  network_request_->set_num_retries(10);
  network_request_->set_time_between_retries(10);  // 10 miliseconds.
  std::vector<uint8> response;

  CString url = _T("http://nohost/nofile");

  EXPECT_EQ(GOOPDATE_E_CANCELLED,
            network_request_->DownloadFile(url, _T("c:\\foo")));

  EXPECT_EQ(GOOPDATE_E_CANCELLED,
            network_request_->DownloadFile(url, _T("c:\\foo")));
}

TEST_F(NetworkRequestTest, CancelTest_Get) {
  network_request_->AddHttpRequest(new SimpleRequest);
  CancelTest_GetHelper();
}

}  // namespace omaha
