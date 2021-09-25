// Copyright 2014 Google Inc.
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

#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/common/ping.h"
#include "omaha/common/ping_event_download_metrics.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const CString kPv = _T("1.3.99.0");
const CString kLang = _T("en");
const CString kBrandCode = _T("GOOG");
const CString kClientId = _T("testclientid");
const CString kIid = _T("{7C0B6E56-B24B-436b-A960-A6EA201E886D}");

}  // namespace

class PingEventDownloadMetricsTest : public testing::Test {
 protected:
  void SetUpRegistry() {
    const TCHAR* const kOmahaUserClientStatePath =
        _T("HKCU\\Software\\") PATH_COMPANY_NAME
        _T("\\") PRODUCT_NAME
        _T("\\ClientState\\") GOOPDATE_APP_ID;

    RegKey::DeleteKey(kOmahaUserClientStatePath);
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                              kRegValueProductVersion,
                                              kPv));
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                              kRegValueLanguage,
                                              kLang));
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                              kRegValueBrandCode,
                                              kBrandCode));
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                              kRegValueClientId,
                                              kClientId));
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                              kRegValueInstallationId,
                                              kIid));
  }
};

TEST_F(PingEventDownloadMetricsTest, BuildPing) {
  SetUpRegistry();

  DownloadMetrics download_metrics;
  download_metrics.url = _T("http:\\\\host\\path");
  download_metrics.downloader = DownloadMetrics::kWinHttp;
  download_metrics.error = 5;
  download_metrics.downloaded_bytes = 10;
  download_metrics.total_bytes = 0x100000000;
  download_metrics.download_time_ms = 1000;

  PingEventPtr ping_event(
      new PingEventDownloadMetrics(true,
                                   PingEvent::EVENT_RESULT_ERROR,
                                   download_metrics));

  Ping ping(false, _T("unittest"), _T("InstallSource_Foo"));
  std::vector<CString> apps;
  apps.push_back(GOOPDATE_APP_ID);
  ping.LoadAppDataFromRegistry(apps);
  ping.BuildAppsPing(ping_event);

  CString expected_ping_request_substring;
  expected_ping_request_substring =
      _T("<app appid=\"") GOOPDATE_APP_ID _T("\" ")
      _T("version=\"1.3.99.0\" nextversion=\"\" lang=\"en\" brand=\"GOOG\" ")
      _T("client=\"testclientid\" ")
      _T("iid=\"{7C0B6E56-B24B-436b-A960-A6EA201E886D}\">")
      _T("<event eventtype=\"14\" eventresult=\"0\" errorcode=\"5\" ")
      _T("extracode1=\"0\" downloader=\"winhttp\" url=\"http:\\\\host\\path\" ")
      _T("downloaded=\"10\" total=\"4294967296\" download_time_ms=\"1000\"/>")
      _T("</app>");

  CString actual_ping_request;
  ping.BuildRequestString(&actual_ping_request);
  EXPECT_NE(-1, actual_ping_request.Find(expected_ping_request_substring))
    << actual_ping_request.GetString()
    << _T("\n\r\n\r")
    << expected_ping_request_substring.GetString();
}

}  // namespace omaha
