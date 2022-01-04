// Copyright 2010 Google Inc.
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

#include <string.h>
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/ping.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class PingTest : public testing::Test {
 protected:
  virtual void SetUp() {
    RegKey::DeleteKey(USER_REG_UPDATE _T("\\PersistedPings"));
  }

  virtual void TearDown() {
    RegKey::DeleteKey(USER_REG_UPDATE _T("\\PersistedPings"));
  }
};

class PersistedPingsTest : public AppTestBase {
 protected:
  PersistedPingsTest()
      : AppTestBase(false,  // is_machine
                    true),  // use_strict_mock
        app_(NULL) {}

  virtual void SetUp() {
    AppTestBase::SetUp();

    RegKey::DeleteKey(USER_REG_UPDATE _T("\\PersistedPings"));

    const TCHAR* const kAppId1 = _T("{DDE97E2B-A82C-4790-A630-FCA02F64E8BE}");
    EXPECT_SUCCEEDED(
        app_bundle_->createApp(CComBSTR(kAppId1), &app_));
    ASSERT_TRUE(app_);
  }

  const CString request_id() {
    return app_bundle_->request_id_;
  }

  App* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistedPingsTest);
};

TEST_F(PingTest, BuildOmahaPing) {
  PingEventPtr ping_event1(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    10,
                    20));

  PingEventPtr ping_event2(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    30,
                    40));

  CommandLineExtraArgs command_line_extra_args;
  StringToGuidSafe(_T("{DE06587E-E5AB-4364-A46B-F3AC733007B3}"),
                   &command_line_extra_args.installation_id);
  command_line_extra_args.brand_code = _T("GGLS");
  command_line_extra_args.client_id  = _T("a client id");
  command_line_extra_args.language   = _T("en");

  File::Remove(goopdate_utils::BuildGoogleUpdateExePath(false));

  // User ping, missing shell.
  Ping install_ping_no_shell(false, _T("session"), _T("taggedmi"));
  install_ping_no_shell.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping_no_shell.BuildOmahaPing(_T("1.0.0.0"),
                                       _T("2.0.0.0"),
                                       ping_event1,
                                       ping_event2);

  CString expected_shell_version_substring;
  expected_shell_version_substring.Format(_T(" shell_version=\"%s\" "),
                                          GetShellVersionString());
  CString expected_ping_request_substring;
  expected_ping_request_substring.Format(_T("<app appid=\"") GOOPDATE_APP_ID _T("\" version=\"1.0.0.0\" nextversion=\"2.0.0.0\" lang=\"en\" brand=\"GGLS\" client=\"a client id\" iid=\"{DE06587E-E5AB-4364-A46B-F3AC733007B3}\"><event eventtype=\"2\" eventresult=\"1\" errorcode=\"10\" extracode1=\"20\"/><event eventtype=\"2\" eventresult=\"1\" errorcode=\"30\" extracode1=\"40\"/></app>"));  // NOLINT

  CString actual_ping_request;
  install_ping_no_shell.BuildRequestString(&actual_ping_request);

  // The ping_request_string contains some data that depends on the machine
  // environment, such as operating system version. Look for partial matches in
  // the string corresponding to the shell_version and the <app> elements.
  EXPECT_NE(-1, actual_ping_request.Find(expected_shell_version_substring));
  EXPECT_NE(-1, actual_ping_request.Find(expected_ping_request_substring));

  CPath shell_path_1_2_183_21(app_util::GetCurrentModuleDirectory());
  shell_path_1_2_183_21.Append(_T("unittest_support\\omaha_1.3.x\\"));
  shell_path_1_2_183_21.Append(kOmahaShellFileName);
  EXPECT_SUCCEEDED(File::Copy(shell_path_1_2_183_21,
                              goopdate_utils::BuildGoogleUpdateExePath(false),
                              true));

  // User ping, 1.2.183.21 shell.
  Ping install_ping_1_2_183_21(false, _T("session"), _T("taggedmi"));
  install_ping_1_2_183_21.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping_1_2_183_21.BuildOmahaPing(_T("1.0.0.0"),
                                         _T("2.0.0.0"),
                                         ping_event1,
                                         ping_event2);

  expected_shell_version_substring.Format(_T(" shell_version=\"1.2.183.21\" "));

  actual_ping_request.Empty();
  install_ping_1_2_183_21.BuildRequestString(&actual_ping_request);

  EXPECT_NE(-1, actual_ping_request.Find(expected_shell_version_substring));
  EXPECT_NE(-1, actual_ping_request.Find(expected_ping_request_substring));

  // Clean up after ourselves, since we copied the test exe into this path
  // earlier in this test.
  EXPECT_SUCCEEDED(File::Remove(goopdate_utils::BuildGoogleUpdateExePath(false)));
}

TEST_F(PingTest, BuildAppsPing) {
  const TCHAR* const kOmahaUserClientStatePath =
      _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
      _T("\\ClientState\\") GOOPDATE_APP_ID;

  const CString expected_pv           = _T("1.3.99.0");
  const CString expected_lang         = _T("en");
  const CString expected_brand_code   = _T("GGLS");
  const CString expected_client_id    = _T("someclientid");
  const CString expected_iid          =
      _T("{7C0B6E56-B24B-436b-A960-A6EA201E886F}");
  const CString experiment_labels =
    _T("a=a|Wed, 14 Mar 2029 23:36:18 GMT;b=a|Fri, 14 Aug 2015 16:13:03 GMT");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueProductVersion,
                                            expected_pv));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueLanguage,
                                            expected_lang));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueBrandCode,
                                            expected_brand_code));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueClientId,
                                            expected_client_id));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueInstallationId,
                                            expected_iid));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueExperimentLabels,
                                            experiment_labels));

  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  const DWORD two_days_back = now - (2 * kSecondsPerDay);
  ASSERT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                    kRegValueInstallTimeSec,
                                    two_days_back));

  // The actual installdate in ping will be floor(9090/7)*7 = 9086.
  const DWORD day_of_install = 9090;
  ASSERT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                    kRegValueDayOfInstall,
                                    day_of_install));

  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    34,
                    6));

  Ping apps_ping(false, _T("unittest"), _T("InstallSource_Foo"));
  std::vector<CString> apps;
  apps.push_back(GOOPDATE_APP_ID);
  apps_ping.LoadAppDataFromRegistry(apps);
  apps_ping.BuildAppsPing(ping_event);

  CString expected_ping_request_substring;
  expected_ping_request_substring.Format(_T("<app appid=\"") GOOPDATE_APP_ID _T("\" version=\"1.3.99.0\" nextversion=\"\" lang=\"en\" brand=\"GGLS\" client=\"someclientid\" experiments=\"a=a\" installage=\"2\" installdate=\"9086\" iid=\"{7C0B6E56-B24B-436b-A960-A6EA201E886F}\"><event eventtype=\"2\" eventresult=\"1\" errorcode=\"34\" extracode1=\"6\"/></app>"));  // NOLINT

  CString actual_ping_request;
  apps_ping.BuildRequestString(&actual_ping_request);

  EXPECT_NE(-1, actual_ping_request.Find(expected_ping_request_substring)) <<
      actual_ping_request.GetString();
}

TEST_F(PingTest, DISABLED_SendString) {
  CString request_string = _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?><request protocol=\"3.0\" version=\"1.3.99.0\" ismachine=\"1\" sessionid=\"unittest\" installsource=\"taggedmi\" testsource=\"dev\" requestid=\"{EC821C33-E4EE-4E75-BC85-7E9DFC3652F5}\" periodoverridesec=\"7407360\"><os platform=\"win\" version=\"6.0\" sp=\"Service Pack 1\"/><app appid=\"") GOOPDATE_APP_ID _T("\" version=\"1.0.0.0\" nextversion=\"2.0.0.0\" lang=\"en\" brand=\"GGLS\" client=\"a client id\" iid=\"{DE06587E-E5AB-4364-A46B-F3AC733007B3}\"><event eventtype=\"10\" eventresult=\"1\" errorcode=\"0\" extracode1=\"0\"/></app></request>");   // NOLINT
  EXPECT_HRESULT_SUCCEEDED(Ping::SendString(false,
                                            HeadersVector(),
                                            request_string));

  // 400 Bad Request returned by the server.
  EXPECT_EQ(0x80042190, Ping::SendString(false, HeadersVector(), _T("")));
}

TEST_F(PingTest, DISABLED_HandlePing) {
  CString request_string = _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?><request protocol=\"3.0\" version=\"1.3.99.0\" ismachine=\"1\" sessionid=\"unittest\" installsource=\"taggedmi\" testsource=\"dev\" requestid=\"{EC821C33-E4EE-4E75-BC85-7E9DFC3652F5}\" periodoverridesec=\"7407360\"><os platform=\"win\" version=\"6.0\" sp=\"Service Pack 1\"/><app appid=\"") GOOPDATE_APP_ID _T("\" version=\"1.0.0.0\" nextversion=\"2.0.0.0\" lang=\"en\" brand=\"GGLS\" client=\"a client id\" iid=\"{DE06587E-E5AB-4364-A46B-F3AC733007B3}\"><event eventtype=\"10\" eventresult=\"1\" errorcode=\"0\" extracode1=\"0\"/></app></request>");   // NOLINT

  CStringA request_string_utf8(WideToUtf8(request_string));
  CStringA ping_string_utf8;
  WebSafeBase64Escape(request_string_utf8, &ping_string_utf8);

  EXPECT_HRESULT_SUCCEEDED(
      Ping::HandlePing(false, Utf8ToWideChar(ping_string_utf8,
                                             ping_string_utf8.GetLength())));

  // 400 Bad Request returned by the server.
  EXPECT_EQ(0x80042190, Ping::HandlePing(false, _T("")));
}

TEST_F(PingTest, SendInProcess) {
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    0,
                    0));

  CommandLineExtraArgs command_line_extra_args;
  StringToGuidSafe(_T("{DE06587E-E5AB-4364-A46B-F3AC733007B3}"),
                   &command_line_extra_args.installation_id);
  command_line_extra_args.brand_code = _T("GGLS");
  command_line_extra_args.client_id  = _T("a client id");
  command_line_extra_args.language   = _T("en");

  // User ping.
  Ping install_ping(false, _T("unittest"), _T("taggedmi"));
  install_ping.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping.BuildOmahaPing(_T("1.0.0.0"), _T("2.0.0.0"), ping_event);

  CString request_string;
  EXPECT_HRESULT_SUCCEEDED(install_ping.BuildRequestString(&request_string));
  EXPECT_HRESULT_SUCCEEDED(install_ping.SendInProcess(request_string));
}

TEST_F(PingTest, IsPingExpired_PastTime) {
  const time64 time = GetCurrent100NSTime() -
                      (Ping::kPersistedPingExpiry100ns + 1);
  EXPECT_TRUE(Ping::IsPingExpired(time));
}

TEST_F(PingTest, IsPingExpired_CurrentTime) {
  const time64 time = GetCurrent100NSTime();
  EXPECT_FALSE(Ping::IsPingExpired(time));
}

TEST_F(PingTest, IsPingExpired_FutureTime) {
  const time64 time = GetCurrent100NSTime() + 10;
  EXPECT_TRUE(Ping::IsPingExpired(time));
}

TEST_F(PingTest, LoadPersistedPings_NoPersistedPings) {
  Ping::PingsVector persisted_pings;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            Ping::LoadPersistedPings(false, &persisted_pings));
  EXPECT_EQ(0, persisted_pings.size());
}

TEST_F(PingTest, LoadAndDeletePersistedPings) {
  CString pings_reg_path(Ping::GetPersistedPingsRegPath(false));

  for (size_t i = 0; i < 3; ++i) {
    CString i_str(String_DigitToChar(i + 1));
    CString ping_reg_path(AppendRegKeyPath(pings_reg_path,
                                           _T("Test Key ") + i_str));
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(ping_reg_path,
                                              Ping::kRegValuePersistedPingTime,
                                              i_str));
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
        ping_reg_path,
        Ping::kRegValuePersistedPingString,
        _T("Test Ping ") + i_str));
  }

  Ping::PingsVector persisted_pings;
  EXPECT_HRESULT_SUCCEEDED(Ping::LoadPersistedPings(false, &persisted_pings));
  EXPECT_EQ(3, persisted_pings.size());

  for (size_t i = 0; i < persisted_pings.size(); ++i) {
    CString i_str(String_DigitToChar(i + 1));
    EXPECT_EQ(i + 1, persisted_pings[i].second.first);
    EXPECT_STREQ(_T("Test Ping ") + i_str, persisted_pings[i].second.second);

    EXPECT_HRESULT_SUCCEEDED(
        Ping::DeletePersistedPing(false, _T("Test Key ") + i_str));
  }

  RegKey pings_reg_key;
  pings_reg_key.Open(pings_reg_path, KEY_READ);
  EXPECT_EQ(0, pings_reg_key.GetSubkeyCount());
}

TEST_F(PingTest, PersistAndSendPersistedPings) {
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    S_OK,
                    0));

  CommandLineExtraArgs command_line_extra_args;
  StringToGuidSafe(_T("{DE06587E-E5AB-4364-A46B-F3AC733007B3}"),
                   &command_line_extra_args.installation_id);
  command_line_extra_args.brand_code = _T("GGLS");
  command_line_extra_args.client_id  = _T("a client id");
  command_line_extra_args.language   = _T("en");

  // User ping.
  Ping install_ping(false, _T("unittest"), _T("taggedmi"));
  install_ping.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping.BuildOmahaPing(_T("1.0.0.0"), _T("2.0.0.0"), ping_event);

  time64 past(GetCurrent100NSTime());
  EXPECT_HRESULT_SUCCEEDED(install_ping.PersistPing());

  CString reg_path(Ping::GetPersistedPingsRegPath(false));
  CString ping_subkey_path(AppendRegKeyPath(reg_path,
                                            install_ping.request_id_));
  EXPECT_TRUE(RegKey::HasKey(ping_subkey_path));

  CString persisted_time_string;
  EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(ping_subkey_path,
                                            Ping::kRegValuePersistedPingTime,
                                            &persisted_time_string));
  time64 persisted_time = _tcstoui64(persisted_time_string, NULL, 10);
  EXPECT_LE(past, persisted_time);
  EXPECT_GE(GetCurrent100NSTime(), persisted_time);

  CString persisted_ping;
  EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(ping_subkey_path,
                                            Ping::kRegValuePersistedPingString,
                                            &persisted_ping));
  EXPECT_NE(-1, persisted_ping.Find(_T("sessionid=\"unittest\"")));
  EXPECT_NE(-1, persisted_ping.Find(_T("<app appid=\"") GOOPDATE_APP_ID _T("\" version=\"1.0.0.0\" nextversion=\"2.0.0.0\" lang=\"en\" brand=\"GGLS\" client=\"a client id\" iid=\"{DE06587E-E5AB-4364-A46B-F3AC733007B3}\"><event eventtype=\"2\" eventresult=\"1\" errorcode=\"0\" extracode1=\"0\"/></app>")));  // NOLINT

  EXPECT_HRESULT_SUCCEEDED(Ping::SendPersistedPings(false));

  RegKey pings_reg_key;
  pings_reg_key.Open(reg_path, KEY_READ);
  EXPECT_EQ(0, pings_reg_key.GetSubkeyCount());
}

// The tests below rely on the out-of-process mechanism to send install pings.
// Enable the test to debug the sending code.
TEST_F(PingTest, DISABLED_SendUsingGoogleUpdate) {
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    0,
                    0));

  CommandLineExtraArgs command_line_extra_args;
  StringToGuidSafe(_T("{DE06587E-E5AB-4364-A46B-F3AC733007B3}"),
                   &command_line_extra_args.installation_id);
  command_line_extra_args.brand_code = _T("GGLS");
  command_line_extra_args.client_id  = _T("a client id");
  command_line_extra_args.language   = _T("en");

  // User ping and wait for completion.
  Ping install_ping(false, _T("unittest"), _T("taggedmi"));
  install_ping.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping.BuildOmahaPing(_T("1.0.0.0"), _T("2.0.0.0"), ping_event);

  const int kWaitForPingProcessToCompleteMs = 60000;
  CString request_string;
  EXPECT_HRESULT_SUCCEEDED(install_ping.BuildRequestString(&request_string));
  EXPECT_HRESULT_SUCCEEDED(install_ping.SendUsingGoogleUpdate(
      request_string, kWaitForPingProcessToCompleteMs));
}

TEST_F(PingTest, Send_Empty) {
  CommandLineExtraArgs command_line_extra_args;
  Ping install_ping(false, _T("unittest"), _T("taggedmi"));
  EXPECT_EQ(S_FALSE, install_ping.Send(false));
}

TEST_F(PingTest, DISABLED_Send) {
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    0,
                    0));

  CommandLineExtraArgs command_line_extra_args;
  StringToGuidSafe(_T("{DE06587E-E5AB-4364-A46B-F3AC733007B3}"),
                   &command_line_extra_args.installation_id);
  command_line_extra_args.brand_code = _T("GGLS");
  command_line_extra_args.client_id  = _T("a client id");
  command_line_extra_args.language   = _T("en");

  // User ping and wait for completion.
  Ping install_ping(false, _T("unittest"), _T("taggedmi"));
  install_ping.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping.BuildOmahaPing(_T("1.0.0.0"), _T("2.0.0.0"), ping_event);

  EXPECT_HRESULT_SUCCEEDED(install_ping.PersistPing());
  EXPECT_HRESULT_SUCCEEDED(install_ping.Send(false));
}

TEST_F(PingTest, DISABLED_SendFireAndForget) {
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    0,
                    0));

  CommandLineExtraArgs command_line_extra_args;
  StringToGuidSafe(_T("{DE06587E-E5AB-4364-A46B-F3AC733007B3}"),
                   &command_line_extra_args.installation_id);
  command_line_extra_args.brand_code = _T("GGLS");
  command_line_extra_args.client_id  = _T("a client id");
  command_line_extra_args.language   = _T("en");

  // User ping and do not wait for completion.
  Ping install_ping(false, _T("unittest"), _T("taggedmi"));
  install_ping.LoadAppDataFromExtraArgs(command_line_extra_args);
  install_ping.BuildOmahaPing(_T("1.0.0.0"), _T("2.0.0.0"), ping_event);

  EXPECT_HRESULT_SUCCEEDED(install_ping.PersistPing());
  EXPECT_HRESULT_SUCCEEDED(install_ping.Send(true));
}

TEST_F(PersistedPingsTest, AddPingEvents) {
  time64 past(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  struct PE {
    const PingEvent::Types event_type_;
    const PingEvent::Results event_result_;
    const int error_code_;
    const int extra_code1_;
  } pe[] = {
    {PingEvent::EVENT_INSTALL_DOWNLOAD_START, PingEvent::EVENT_RESULT_SUCCESS,
     S_OK, 0},
    {PingEvent::EVENT_INSTALL_INSTALLER_START, PingEvent::EVENT_RESULT_SUCCESS,
     S_OK, 0},
    {PingEvent::EVENT_INSTALL_COMPLETE, PingEvent::EVENT_RESULT_ERROR,
     E_FAIL, 0},
  };

  for (size_t i = 0; i < arraysize(pe); ++i) {
    PingEventPtr ping_event(new PingEvent(pe[i].event_type_,
                                          pe[i].event_result_,
                                          pe[i].error_code_,
                                          pe[i].extra_code1_));
    app_->AddPingEvent(ping_event);
  }

  Ping::PingsVector persisted_pings;
  EXPECT_HRESULT_SUCCEEDED(Ping::LoadPersistedPings(false, &persisted_pings));
  EXPECT_EQ(1, persisted_pings.size());

  for (size_t i = 0; i < persisted_pings.size(); ++i) {
    time64 persisted_time = persisted_pings[i].second.first;
    EXPECT_LE(past, persisted_time);
    EXPECT_GE(GetCurrent100NSTime(), persisted_time);

    const CString persisted_ping(persisted_pings[i].second.second);
    CString expected_requestid_substring;
    expected_requestid_substring.Format(_T("requestid=\"%s\""), request_id());
    EXPECT_NE(-1, persisted_ping.Find(expected_requestid_substring));

    for (size_t j = 0; j < arraysize(pe); ++j) {
      CString expected_ping_event_substring;
      expected_ping_event_substring.Format(
          _T("<event eventtype=\"%d\" eventresult=\"%d\" ")
          _T("errorcode=\"%d\" extracode1=\"%d\"/>"),
          pe[j].event_type_, pe[j].event_result_,
          pe[j].error_code_, pe[j].extra_code1_);
      EXPECT_NE(-1, persisted_ping.Find(expected_ping_event_substring));
    }
  }
}

}  // namespace omaha

