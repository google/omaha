// Copyright 2013 Google Inc.
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
#include "omaha/common/ping_event.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const CString kPv = _T("1.3.99.0");
const CString kLang = _T("en");
const CString kBrandCode = _T("GOOG");
const CString kClientId = _T("testclientid");
const CString kIid = _T("{7C0B6E56-B24B-436b-A960-A6EA201E886D}");

}  // namespace

class PingEventTest : public testing::Test {
 protected:
  void SetUpRegistry() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
    OverrideRegistryHives(kRegistryHiveOverrideRoot);

    const TCHAR* const kOmahaUserClientStatePath =
        _T("HKCU\\Software\\") PATH_COMPANY_NAME
        _T("\\") PRODUCT_NAME
        _T("\\ClientState\\") GOOPDATE_APP_ID;

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

  virtual void CleanUpRegistry() {
    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }
};

TEST_F(PingEventTest, BuildDownloadCompletePing) {
  const int error_code = 34;
  const int extra_code1 = 3333;
  const int source_url_index = 5;
  const int update_check_time_ms = 1234;
  const int download_time_ms = 15000;
  const uint64 num_bytes_downloaded = 4000000;
  const uint64 app_packages_total_size = 8000000;
  const int install_time_ms = 7788;

  SetUpRegistry();
  PingEventPtr ping_event(new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                                        PingEvent::EVENT_RESULT_SUCCESS,
                                        error_code,
                                        extra_code1,
                                        source_url_index,
                                        update_check_time_ms,
                                        download_time_ms,
                                        num_bytes_downloaded,
                                        app_packages_total_size,
                                        install_time_ms));

  Ping ping(false, _T("unittest"), _T("InstallSource_Foo"));
  std::vector<CString> apps;
  apps.push_back(GOOPDATE_APP_ID);
  ping.LoadAppDataFromRegistry(apps);
  ping.BuildAppsPing(ping_event);
  CleanUpRegistry();

  CString expected_ping_request_substring;
  expected_ping_request_substring.Format(
      _T("<app appid=\"%s\" version=\"%s\" nextversion=\"\" lang=\"%s\" ")
      _T("brand=\"%s\" client=\"%s\" iid=\"%s\">")
      _T("<event eventtype=\"%d\" eventresult=\"%d\" ")
      _T("errorcode=\"%d\" extracode1=\"%d\" ")
      _T("source_url_index=\"%d\" ")
      _T("update_check_time_ms=\"%d\" ")
      _T("download_time_ms=\"%d\" downloaded=\"%I64u\" total=\"%I64u\" ")
      _T("install_time_ms=\"%d\"/>")
      _T("</app>"),
      GOOPDATE_APP_ID, kPv, kLang, kBrandCode, kClientId, kIid,
      PingEvent::EVENT_INSTALL_COMPLETE, PingEvent::EVENT_RESULT_SUCCESS,
      error_code, extra_code1, source_url_index,
      update_check_time_ms, download_time_ms, num_bytes_downloaded,
      app_packages_total_size, install_time_ms);

  CString actual_ping_request;
  ping.BuildRequestString(&actual_ping_request);
  EXPECT_NE(-1, actual_ping_request.Find(expected_ping_request_substring));
}

TEST_F(PingEventTest, BuildDownloadCompletePing_Cached) {
  const int error_code = 888;
  const int extra_code1 = 0;
  const int source_url_index = -1;
  const int update_check_time_ms = 0;
  const int download_time_ms = 15;
  const uint64 num_bytes_downloaded = 4000000;
  const uint64 app_packages_total_size = 4000000;
  const int install_time_ms = 0;

  SetUpRegistry();
  PingEventPtr ping_event(new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                                        PingEvent::EVENT_RESULT_SUCCESS,
                                        error_code,
                                        extra_code1,
                                        source_url_index,
                                        update_check_time_ms,
                                        download_time_ms,
                                        num_bytes_downloaded,
                                        app_packages_total_size,
                                        install_time_ms));

  Ping ping(false, _T("unittest"), _T("InstallSource_Foo"));
  std::vector<CString> apps;
  apps.push_back(GOOPDATE_APP_ID);
  ping.LoadAppDataFromRegistry(apps);
  ping.BuildAppsPing(ping_event);
  CleanUpRegistry();

  CString expected_ping_request_substring;
  expected_ping_request_substring.Format(
      _T("<app appid=\"%s\" version=\"%s\" nextversion=\"\" lang=\"%s\" ")
      _T("brand=\"%s\" client=\"%s\" iid=\"%s\">")
      _T("<event eventtype=\"%d\" eventresult=\"%d\" ")
      _T("errorcode=\"%d\" extracode1=\"%d\" ")
      _T("download_time_ms=\"15\" downloaded=\"%I64u\" total=\"%I64u\"/>")
      _T("</app>"),
      GOOPDATE_APP_ID, kPv, kLang, kBrandCode, kClientId, kIid,
      PingEvent::EVENT_INSTALL_COMPLETE, PingEvent::EVENT_RESULT_SUCCESS,
      error_code, extra_code1, app_packages_total_size,
      app_packages_total_size);

  CString actual_ping_request;
  ping.BuildRequestString(&actual_ping_request);
  EXPECT_NE(-1, actual_ping_request.Find(expected_ping_request_substring));
}

}  // namespace omaha
