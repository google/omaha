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

//
// Tests Get over http and https, using direct connection and wpad proxy.
//
// TODO(omaha): missing Post unit tests

#include <windows.h>
#include <winhttp.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/error.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/common/ping_event_download_metrics.h"
#include "omaha/net/network_config.h"
#include "omaha/net/simple_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR* kBigFileUrl =
    _T("http://dl.google.com/dl/edgedl/update2/UpdateData_10M.bin");

DWORD WINAPI PauseAndResumeThreadProc(void* parameter) {
  SimpleRequest* simple_request = reinterpret_cast<SimpleRequest*>(parameter);
  ASSERT1(simple_request);

  bool request_paused = false;

  // Loop even times so the download thread won't be blocked by Pause() after
  // loop exit.
  for (int i = 0; i < 10; ++i) {
    ::Sleep(100);

    if (request_paused) {
      simple_request->Resume();
    } else {
      simple_request->Pause();
    }

    request_paused = !request_paused;
  }

  return 0;
}

DWORD WINAPI CancelRequestThreadProc(void* parameter) {
  SimpleRequest* simple_request = reinterpret_cast<SimpleRequest*>(parameter);
  ASSERT1(simple_request);

  // Wait a short period of time so the download can start. Assumes the file
  // is large enough so that download will not complete within the sleep time.
  ::Sleep(100);

  simple_request->Cancel();
  return 0;
}

bool IsWpadFailureError(HRESULT hr) {
  // Google corpnet currently has a semi-functional WPAD server that does
  // not support WinHTTP (or IE6/IE7 for that matter).  If we get an error
  // message indicating that we found a WPAD server but could not fetch the
  // PAC script, treat this as a skipped test.
  if (IsEnvironmentVariableSet(_T("OMAHA_TEST_DISABLE_WPAD_ERROR_HIDING"))) {
    return false;
  }

  return hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT);
}

void PrintWpadFailureWarning() {
  std::wcout << _T("\tAborted; WPAD server is non-functional.") << std::endl;
}

class SimpleRequestTest : public testing::Test {
 protected:
  SimpleRequestTest() {}

  void SimpleGet(const CString& url, const ProxyConfig& config);
  void SimpleDownloadFile(const CString& url,
                          const CString& filename,
                          const ProxyConfig& config);
  void SimpleDownloadFilePauseAndResume(const CString& url,
                                        const CString& filename,
                                        const ProxyConfig& config);
  // Returns the result of the Send call.
  HRESULT SimpleDownloadFileCancellation(const CString& filename,
                                         bool do_cancel);
  void SimpleGetHostNotFound(const CString& url, const ProxyConfig& config);

  void SimpleGetFileNotFound(const CString& url, const ProxyConfig& config);

  void SimpleGetRedirect(const CString& url, const ProxyConfig& config);

  void PrepareRequest(const CString& url,
                      const ProxyConfig& config,
                      SimpleRequest* simple_request);
};

void SimpleRequestTest::PrepareRequest(const CString& url,
                                       const ProxyConfig& config,
                                       SimpleRequest* simple_request) {
  ASSERT_TRUE(simple_request);
  NetworkConfig* network_config = NULL;
  EXPECT_HRESULT_SUCCEEDED(
      NetworkConfigManager::Instance().GetUserNetworkConfig(&network_config));

  HINTERNET handle = network_config->session().session_handle;
  simple_request->set_session_handle(handle);
  simple_request->set_url(url);
  simple_request->set_proxy_configuration(config);

  CString user_agent_header;
  user_agent_header.Format(_T("User-Agent: %s\r\n"),
                           simple_request->user_agent());
  simple_request->set_additional_headers(user_agent_header);
}

void SimpleRequestTest::SimpleGet(const CString& url,
                                  const ProxyConfig& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);

  HRESULT hr = simple_request.Send();
  if (IsWpadFailureError(hr)) {
    PrintWpadFailureWarning();
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(HTTP_STATUS_OK, simple_request.GetHttpStatusCode());
  CString response = Utf8BufferToWideChar(simple_request.GetResponse());

  // robots.txt response contains "User-agent: *". This is not the "User-Agent"
  // http header.
  EXPECT_NE(-1, response.Find(_T("User-agent: *")));
  CString content_type;
  simple_request.QueryHeadersString(WINHTTP_QUERY_CONTENT_TYPE,
                                    NULL, &content_type);
  EXPECT_STREQ(_T("text/plain"), content_type);

  // Uses custom query for content type and it should match what we just got.
  CString content_type_from_custom_query;
  simple_request.QueryHeadersString(WINHTTP_QUERY_CUSTOM,
                                    _T("Content-Type"),
                                    &content_type_from_custom_query);
  EXPECT_STREQ(content_type, content_type_from_custom_query);

  EXPECT_FALSE(simple_request.GetResponseHeaders().IsEmpty());

  // Check the user agent went out with the request.
  CString user_agent;
  simple_request.QueryHeadersString(
      WINHTTP_QUERY_FLAG_REQUEST_HEADERS | WINHTTP_QUERY_USER_AGENT,
      WINHTTP_HEADER_NAME_BY_INDEX,
      &user_agent);
  EXPECT_STREQ(simple_request.user_agent(), user_agent);

  // Check of some of the download metrics.
  DownloadMetrics dm;
  EXPECT_TRUE(simple_request.download_metrics(&dm));
  EXPECT_STREQ(url, dm.url);
  EXPECT_EQ(DownloadMetrics::kWinHttp, dm.downloader);
  EXPECT_EQ(0, dm.error);
}

void SimpleRequestTest::SimpleDownloadFile(const CString& url,
                                           const CString& filename,
                                           const ProxyConfig& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);
  simple_request.set_filename(filename);

  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());

  int http_status = simple_request.GetHttpStatusCode();
  EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
              http_status == HTTP_STATUS_PARTIAL_CONTENT);
}

void SimpleRequestTest::SimpleDownloadFilePauseAndResume(
    const CString& url,
    const CString& filename,
    const ProxyConfig& config) {
  const int kNumThreads = 3;
  const int kMaxWaitTimeMs = 10000;
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);
  simple_request.set_filename(filename);

  // Testing call Pause before Send.
  simple_request.Pause();
  simple_request.Resume();
  HANDLE pause_and_resume_threads[kNumThreads] = { NULL };

  // Now create some threads to run Pause/Resume in the middle of sending.
  for (int i = 0; i < kNumThreads; ++i) {
    pause_and_resume_threads[i] = ::CreateThread(NULL,
                                                 0,
                                                 PauseAndResumeThreadProc,
                                                 &simple_request,
                                                 0,
                                                 NULL);
    EXPECT_TRUE(pause_and_resume_threads[i] != NULL);
  }

  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());

  int http_status = simple_request.GetHttpStatusCode();
  EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
              http_status == HTTP_STATUS_PARTIAL_CONTENT);

  DWORD result = ::WaitForMultipleObjects(arraysize(pause_and_resume_threads),
                                          pause_and_resume_threads,
                                          true,   // Wait all threads to exit.
                                          kMaxWaitTimeMs);
  EXPECT_NE(result, WAIT_FAILED);
  EXPECT_NE(result, WAIT_TIMEOUT);

  for (int i = 0; i < kNumThreads; ++i) {
    ::CloseHandle(pause_and_resume_threads[i]);
  }
}

HRESULT SimpleRequestTest::SimpleDownloadFileCancellation(
    const CString& filename, bool do_cancel) {
  SimpleRequest simple_request;
  PrepareRequest(kBigFileUrl, ProxyConfig(), &simple_request);
  simple_request.set_filename(filename);

  scoped_handle cancel_thread_handle;
  if (do_cancel) {
    reset(cancel_thread_handle, ::CreateThread(NULL,
                                               0,
                                               CancelRequestThreadProc,
                                               &simple_request,
                                               0, NULL));
  }

  HRESULT hr = simple_request.Send();
  if (do_cancel) {
    EXPECT_TRUE(hr == GOOPDATE_E_CANCELLED || SUCCEEDED(hr));
    ::WaitForSingleObject(get(cancel_thread_handle), INFINITE);
  } else {
    EXPECT_HRESULT_SUCCEEDED(hr);
  }

  return hr;
}

void SimpleRequestTest::SimpleGetHostNotFound(const CString& url,
                                              const ProxyConfig& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);

  HRESULT hr = simple_request.Send();
  int status_code = simple_request.GetHttpStatusCode();

  if (config.auto_detect) {
    // Either a direct connection or a proxy will be used in the case of
    // proxy auto detect.
    //
    // When a proxy is detected, the proxy server can return some html content
    // indicating an error has occurred. The status codes will be different,
    // depending on how each proxy is configured. This may be a flaky unit test.
    if (FAILED(hr)) {
      if (IsWpadFailureError(hr)) {
        PrintWpadFailureWarning();
      } else {
        EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_WINHTTP_NAME_NOT_RESOLVED), hr);
      }
    } else {
      if (String_StartsWith(url, kHttpsProtoScheme, true)) {
        EXPECT_EQ(HTTP_STATUS_NOT_FOUND, status_code);
      } else {
        EXPECT_EQ(HTTP_STATUS_SERVICE_UNAVAIL, status_code);
      }
    }
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_WINHTTP_NAME_NOT_RESOLVED), hr);
    EXPECT_EQ(0, status_code);
  }
}

void SimpleRequestTest::SimpleGetFileNotFound(const CString& url,
                                              const ProxyConfig& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);

  HRESULT hr = simple_request.Send();
  if (IsWpadFailureError(hr)) {
    PrintWpadFailureWarning();
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());
  EXPECT_EQ(HTTP_STATUS_NOT_FOUND, simple_request.GetHttpStatusCode());
}

void SimpleRequestTest::SimpleGetRedirect(const CString& url,
                                          const ProxyConfig& config) {
  SimpleRequest simple_request;
  PrepareRequest(url, config, &simple_request);
  EXPECT_HRESULT_SUCCEEDED(simple_request.Send());

  int http_status = simple_request.GetHttpStatusCode();
  EXPECT_TRUE(http_status == HTTP_STATUS_OK ||
              http_status == HTTP_STATUS_PARTIAL_CONTENT);
}

//
// http tests.
//
// http get, direct connection.
TEST_F(SimpleRequestTest, HttpGetDirect) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  SimpleGet(_T("http://www.google.com/robots.txt"), ProxyConfig());
}

// http get, direct connection, download to file
TEST_F(SimpleRequestTest, HttpDownloadDirect) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  CString temp_file = GetTempFilenameAt(app_util::GetModuleDirectory(NULL),
                                        _T("SRT"));
  ASSERT_FALSE(temp_file.IsEmpty());

  ScopeGuard guard = MakeGuard(::DeleteFile, temp_file);

  SimpleDownloadFile(
      _T("http://dl.google.com/update2/UpdateData.bin"),
      temp_file, ProxyConfig());
}

// http get, direct connection, download to file, pause/resume
// Download same file twice with and without pause/resume. The downloaded
// files should be same.
TEST_F(SimpleRequestTest, DISABLED_HttpDownloadDirectPauseAndResume) {
  // Download the same URL with and without pause/resume.
  CString temp_file1 = GetTempFilenameAt(app_util::GetModuleDirectory(NULL),
                                         _T("SRT"));
  ASSERT_FALSE(temp_file1.IsEmpty());
  ScopeGuard guard = MakeGuard(::DeleteFile, temp_file1);

  SimpleDownloadFilePauseAndResume(kBigFileUrl, temp_file1, ProxyConfig());

  CString temp_file2 = GetTempFilenameAt(app_util::GetModuleDirectory(NULL),
                                         _T("SRT"));
  ASSERT_FALSE(temp_file2.IsEmpty());
  ScopeGuard guard2 = MakeGuard(::DeleteFile, temp_file2);

  SimpleDownloadFile(kBigFileUrl, temp_file2, ProxyConfig());

  // Compares that the downloaded files are equal
  scoped_hfile file_handle1(::CreateFile(temp_file1, GENERIC_READ,
      FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  EXPECT_NE(get(file_handle1), INVALID_HANDLE_VALUE);

  scoped_hfile file_handle2(::CreateFile(temp_file2, GENERIC_READ,
      FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));

  // Compare file size
  DWORD file_size = GetFileSize(get(file_handle1), NULL);
  EXPECT_EQ(file_size, GetFileSize(get(file_handle2), NULL));

  // Compare file contents
  const int kBufferLength = 1024;
  BYTE buffer1[kBufferLength] = {0};
  BYTE buffer2[kBufferLength] = {0};

  for (int i = static_cast<int>(file_size); i > 0; i -= kBufferLength) {
    int bytes_to_read = std::min(kBufferLength, i);
    DWORD num_bytes_got = 0;

    EXPECT_TRUE(ReadFile(get(file_handle1), buffer1,
                         bytes_to_read, &num_bytes_got, NULL));
    EXPECT_EQ(num_bytes_got, static_cast<DWORD>(bytes_to_read));
    EXPECT_TRUE(ReadFile(get(file_handle2), buffer2,
                         bytes_to_read, &num_bytes_got, NULL));
    EXPECT_EQ(num_bytes_got, static_cast<DWORD>(bytes_to_read));
    EXPECT_EQ(memcmp(buffer1, buffer2, bytes_to_read), 0);
  }

  // Make sure that file handles are closed first so that the delete guards
  // can delete the temp files.
  reset(file_handle1);
  reset(file_handle2);
}

// http get, direct connection, negative test.
TEST_F(SimpleRequestTest, HttpGetDirectHostNotFound) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  SimpleGetHostNotFound(_T("http://no_such_host.google.com/"), ProxyConfig());
}

// http get, direct connection, negative test.
TEST_F(SimpleRequestTest, HttpGetDirectFileNotFound) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  SimpleGetFileNotFound(_T("http://tools.google.com/no_such_file"),
                        ProxyConfig());
}

// http get, proxy wpad.
TEST_F(SimpleRequestTest, HttpGetProxy) {
  ProxyConfig config;
  config.auto_detect = true;
  SimpleGet(_T("http://www.google.com/robots.txt"), config);
}

// http get, proxy wpad, negative test.
TEST_F(SimpleRequestTest, HttpGetProxyHostNotFound) {
  ProxyConfig config;
  config.auto_detect = true;
  SimpleGetHostNotFound(_T("http://no_such_host.google.com/"), config);
}

// http get, proxy wpad.
TEST_F(SimpleRequestTest, HttpGetProxyFileNotFound) {
  ProxyConfig config;
  config.auto_detect = true;
  SimpleGetFileNotFound(_T("http://tools.google.com/no_such_file"), config);
}


//
// https tests.
//
// https get, direct.
TEST_F(SimpleRequestTest, HttpsGetDirect) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  SimpleGet(_T("https://www.google.com/robots.txt"), ProxyConfig());
}

// https get, direct, negative test.
TEST_F(SimpleRequestTest, HttpsGetDirectHostNotFound) {
  SimpleGetHostNotFound(_T("https://no_such_host.google.com/"), ProxyConfig());
}

// https get, direct connection, negative test.
TEST_F(SimpleRequestTest, HttpsGetDirectFileNotFound) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  SimpleGetFileNotFound(_T("https://tools.google.com/no_such_file"),
                        ProxyConfig());
}

// https get, proxy wpad.
TEST_F(SimpleRequestTest, HttpsGetProxy) {
  ProxyConfig config;
  config.auto_detect = true;
  SimpleGet(_T("https://www.google.com/robots.txt"), config);
}

// https get, proxy wpad, negative test.
TEST_F(SimpleRequestTest, HttpsGetProxyHostNotFound) {
  ProxyConfig config;
  config.auto_detect = true;
  SimpleGetHostNotFound(_T("https://no_such_host.google.com/"), config);
}

// https get, proxy wpad, negative test.
TEST_F(SimpleRequestTest, HttpsGetProxyFileNotFound) {
  ProxyConfig config;
  config.auto_detect = true;
  SimpleGetFileNotFound(_T("https://tools.google.com/no_such_file"), config);
}

// Should not be able to reuse the object once canceled, even if closed.
TEST_F(SimpleRequestTest, Cancel_CannotReuse) {
  SimpleRequest simple_request;
  PrepareRequest(_T("http:\\foo\\"), ProxyConfig(), &simple_request);
  EXPECT_HRESULT_SUCCEEDED(simple_request.Cancel());
  EXPECT_EQ(GOOPDATE_E_CANCELLED, simple_request.Send());
  EXPECT_HRESULT_SUCCEEDED(simple_request.Close());
  EXPECT_EQ(GOOPDATE_E_CANCELLED, simple_request.Send());
}

TEST_F(SimpleRequestTest, Cancel_ShouldDeleteTempFile) {
  CString temp_file = GetTempFilenameAt(app_util::GetModuleDirectory(NULL),
                                        _T("SRT"));
  ASSERT_FALSE(temp_file.IsEmpty());
  ScopeGuard guard = MakeGuard(::DeleteFile, temp_file);

  // Verify that cancellation should remove partially downloaded file.
  bool do_cancel = true;
  HRESULT hr = SimpleDownloadFileCancellation(temp_file, do_cancel);
  if (FAILED(hr)) {
    EXPECT_EQ(INVALID_FILE_ATTRIBUTES, ::GetFileAttributes(temp_file));
    EXPECT_EQ(ERROR_FILE_NOT_FOUND, ::GetLastError());
  }

  // Verify that target file is preserved if download is not cancelled.
  do_cancel = false;
  SimpleDownloadFileCancellation(temp_file, do_cancel);
  EXPECT_NE(INVALID_FILE_ATTRIBUTES, ::GetFileAttributes(temp_file));
}

TEST_F(SimpleRequestTest, HttpGet_Redirect) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  SimpleGetRedirect(_T("http://www.chrome.com/"), ProxyConfig());
}

}  // namespace omaha

