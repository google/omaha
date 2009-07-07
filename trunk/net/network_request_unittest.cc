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
#include <vector>
#include "base/scoped_ptr.h"
#include "base/basictypes.h"
#include "omaha/common/app_util.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/constants.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/queue_timer.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/utils.h"
#include "omaha/common/vista_utils.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/browser_request.h"
#include "omaha/net/cup_request.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/net/urlmon_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class NetworkRequestTest
    : public testing::Test,
      public NetworkRequestCallback {
 protected:
  NetworkRequestTest() {}

  static void SetUpTestCase() {
    // Initialize the detection chain: GoogleProxy, FireFox if it is the
    // default browser, and IE.
    NetworkConfig& network_config(NetworkConfig::Instance());
    network_config.Clear();
    network_config.Add(new GoogleProxyDetector(MACHINE_REG_UPDATE_DEV));
    BrowserType browser_type(BROWSER_UNKNOWN);
    GetDefaultBrowserType(&browser_type);
    if (browser_type == BROWSER_FIREFOX) {
      network_config.Add(new FirefoxProxyDetector());
    }
    network_config.Add(new IEProxyDetector());
    network_config.Add(new DefaultProxyDetector);

    vista::GetLoggedOnUserToken(&token_);
  }

  static void TearDownTestCase() {
    if (token_) {
      ::CloseHandle(token_);
    }
    NetworkConfig::Instance().Clear();
  }

  virtual void SetUp() {
    const NetworkConfig::Session& session(NetworkConfig::Instance().session());
    network_request_.reset(new NetworkRequest(session));
  }

  virtual void TearDown() {}

  virtual void OnProgress(int bytes, int bytes_total, int, const TCHAR*) {
    UNREFERENCED_PARAMETER(bytes);
    UNREFERENCED_PARAMETER(bytes_total);
    NET_LOG(L3, (_T("[downloading %d of %d]"), bytes, bytes_total));
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
    EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);
  }

  // https get.
  void HttpsGetHelper() {
    std::vector<uint8> response;
    CString url = _T("https://www.google.com/robots.txt");
    network_request_->set_num_retries(2);
    EXPECT_HRESULT_SUCCEEDED(network_request_->Get(url, &response));
    EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);
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
    EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);

    // Post an UTF8 string.
    CStringA utf8_request(reinterpret_cast<const char*>(request));
    EXPECT_HRESULT_SUCCEEDED(network_request_->PostUtf8String(url,
                                                              utf8_request,
                                                              &response));
    EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);

    // Post a Unicode string.
    CString unicode_request(reinterpret_cast<const char*>(request));
    EXPECT_HRESULT_SUCCEEDED(network_request_->PostString(url,
                                                          unicode_request,
                                                          &response));
    EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);
  }

  // Download http file.
  void DownloadHelper() {
    CString url = _T("http://dl.google.com/update2/UpdateData.bin");

    CString temp_dir = app_util::GetTempDir();
    CString temp_file;
    EXPECT_TRUE(::GetTempFileName(temp_dir, _T("tmp"), 0,
                                  CStrBuf(temp_file, MAX_PATH)));
    network_request_->set_num_retries(2);
    network_request_->set_low_priority(true);
    network_request_->set_callback(this);
    EXPECT_HRESULT_SUCCEEDED(network_request_->DownloadFile(url, temp_file));
    EXPECT_TRUE(::DeleteFile(temp_file));
    EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);
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
      EXPECT_EQ(network_request_->http_status_code(), HTTP_STATUS_OK);
    }
  }

  void PostRequestHelper() {
    CString response;
    CString url = _T("http://tools.google.com/service/update2");
    CString request = _T("<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" testsource=\"dev\"/>");  // NOLINT
    EXPECT_HRESULT_SUCCEEDED(PostRequest(network_request_.get(),
                                         true,
                                         url,
                                         request,
                                         &response));
  }

  void PostRequestNegativeTestHelper() {
    CString response;
    CString url = _T("http://no_such_host.google.com/service/update2");
    CString request = _T("<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" testsource=\"dev\"/>");  // NOLINT
    EXPECT_HRESULT_FAILED(PostRequest(network_request_.get(),
                                      true,
                                      url,
                                      request,
                                      &response));
  }

  void RetriesNegativeTestHelper() {
    // Try a direct connection to a non-existent host and keep retrying until
    // the retries are used up. Urlmon request is using IE's settings.
    // Therefore, it is possible a proxy is used. In this case, the http
    // response is '503 Service Unavailable'.
    Config config;
    network_request_->set_network_configuration(&config);
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
    Config config;
    network_request_->set_network_configuration(&config);
    network_request_->set_num_retries(10);
    network_request_->set_time_between_retries(10);  // 10 miliseconds.
    std::vector<uint8> response;

    CString url = _T("http://nohost/nofile");

    EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED,
              network_request_->Get(url, &response));

    EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED,
              network_request_->Get(url, &response));
  }

  scoped_ptr<NetworkRequest> network_request_;
  static HANDLE token_;
};

HANDLE NetworkRequestTest::token_ = NULL;

// http get.
TEST_F(NetworkRequestTest, HttpGet) {
  network_request_->AddHttpRequest(new SimpleRequest);
  network_request_->AddHttpRequest(new BrowserRequest);
  HttpGetHelper();
}

// http get.
TEST_F(NetworkRequestTest, HttpGetUrlmon) {
  network_request_->AddHttpRequest(new UrlmonRequest);
  network_request_->AddHttpRequest(new BrowserRequest);
  HttpGetHelper();
}

// https get.
TEST_F(NetworkRequestTest, HttpsGet) {
  network_request_->AddHttpRequest(new SimpleRequest);
  network_request_->AddHttpRequest(new BrowserRequest);
  HttpsGetHelper();
}

// https get.
TEST_F(NetworkRequestTest, HttpsGetUrlmon) {
  network_request_->AddHttpRequest(new UrlmonRequest);
  network_request_->AddHttpRequest(new BrowserRequest);
  HttpsGetHelper();
}

// http post.
TEST_F(NetworkRequestTest, HttpPost) {
  network_request_->AddHttpRequest(new CupRequest(new SimpleRequest));
  network_request_->AddHttpRequest(new SimpleRequest);
  network_request_->AddHttpRequest(new CupRequest(new BrowserRequest));
  HttpPostHelper();
}

// http post.
TEST_F(NetworkRequestTest, HttpPostUrlmon) {
  network_request_->AddHttpRequest(new CupRequest(new UrlmonRequest));
  network_request_->AddHttpRequest(new UrlmonRequest);
  network_request_->AddHttpRequest(new CupRequest(new BrowserRequest));
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
  network_request_->AddHttpRequest(new BrowserRequest);
  DownloadHelper();
}

// Download http file.
TEST_F(NetworkRequestTest, DownloadUrlmon) {
  BitsRequest* bits_request(new BitsRequest);
  // Bits specific settings.
  bits_request->set_minimum_retry_delay(60);
  bits_request->set_no_progress_timeout(5);

  network_request_->AddHttpRequest(bits_request);
  network_request_->AddHttpRequest(new UrlmonRequest);
  network_request_->AddHttpRequest(new BrowserRequest);
  DownloadHelper();
}

TEST_F(NetworkRequestTest, MultipleRequests) {
  network_request_->AddHttpRequest(new CupRequest(new SimpleRequest));
  MultipleRequestsHelper();
}

TEST_F(NetworkRequestTest, MultipleRequestsUrlmon) {
  network_request_->AddHttpRequest(new CupRequest(new UrlmonRequest));
  MultipleRequestsHelper();
}

TEST_F(NetworkRequestTest, PostRequest) {
  network_request_->AddHttpRequest(new SimpleRequest);
  PostRequestHelper();
}

TEST_F(NetworkRequestTest, PostRequestUrlmon) {
  network_request_->AddHttpRequest(new UrlmonRequest);
  PostRequestHelper();
}

TEST_F(NetworkRequestTest, PostRequestNegativeTest) {
  network_request_->AddHttpRequest(new SimpleRequest);
  PostRequestNegativeTestHelper();
}

TEST_F(NetworkRequestTest, PostRequestNegativeTestUrlmon) {
  network_request_->AddHttpRequest(new UrlmonRequest);
  PostRequestNegativeTestHelper();
}

TEST_F(NetworkRequestTest, RetriesNegativeTest) {
  network_request_->AddHttpRequest(new SimpleRequest);
  RetriesNegativeTestHelper();
}

TEST_F(NetworkRequestTest, RetriesNegativeTestUrlmon) {
  network_request_->AddHttpRequest(new UrlmonRequest);
  RetriesNegativeTestHelper();
}

// Network request can't be reused once canceled.
TEST_F(NetworkRequestTest, CancelTest_CannotReuse) {
  network_request_->Cancel();
  std::vector<uint8> response;

  CString url = _T("https://www.google.com/robots.txt");
  EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED,
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
  Config config;
  network_request_->set_network_configuration(&config);

  BitsRequest* bits_request(new BitsRequest);
  bits_request->set_minimum_retry_delay(60);
  bits_request->set_no_progress_timeout(5);
  network_request_->AddHttpRequest(bits_request);
  network_request_->set_num_retries(10);
  network_request_->set_time_between_retries(10);  // 10 miliseconds.
  std::vector<uint8> response;

  CString url = _T("http://nohost/nofile");

  EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED,
            network_request_->DownloadFile(url, _T("c:\\foo")));

  EXPECT_EQ(OMAHA_NET_E_REQUEST_CANCELLED,
            network_request_->DownloadFile(url, _T("c:\\foo")));
}

TEST_F(NetworkRequestTest, CancelTest_Get) {
  network_request_->AddHttpRequest(new SimpleRequest);
  CancelTest_GetHelper();
}

TEST_F(NetworkRequestTest, CancelTest_GetUrlmon) {
  network_request_->AddHttpRequest(new UrlmonRequest);
  CancelTest_GetHelper();
}

}  // namespace omaha

