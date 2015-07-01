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

#include "omaha/common/ping.h"
#include "base/scoped_ptr.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/goopdate/app.h"
#include "omaha/goopdate/app_bundle.h"
#include "omaha/goopdate/update_request_utils.h"
#include "omaha/goopdate/update_response_utils.h"

namespace omaha {

const TCHAR* const Ping::kRegKeyPing = _T("Pings");
const time64 Ping::kPingExpiry100ns  = 10 * kDaysTo100ns;  // 10 days.

// Minimum compatible Omaha version that understands the /ping command line.
// 1.3.0.0.
const ULONGLONG kMinOmahaVersionForPingOOP = 0x0001000300000000;

Ping::Ping(bool is_machine,
           const CString& session_id,
           const CString& install_source)
    : is_machine_(is_machine),
      ping_request_(xml::UpdateRequest::Create(is_machine,
                                               session_id,
                                               install_source,
                                               CString())) {
  CString shell_version_string =
      goopdate_utils::GetInstalledShellVersion(is_machine);
  if (!shell_version_string.IsEmpty()) {
    ping_request_->set_omaha_shell_version(shell_version_string);
  }
}

Ping::~Ping() {
}

void Ping::BuildRequest(const App* app, bool is_update_check) {
  update_request_utils::BuildRequest(app, is_update_check, ping_request_.get());
}

void Ping::LoadAppDataFromExtraArgs(const CommandLineExtraArgs& extra_args) {
  const CString installation_id = GuidToString(extra_args.installation_id);
  for (size_t i = 0; i != extra_args.apps.size(); ++i) {
    AppData app_data;
    app_data.app_id = GuidToString(extra_args.apps[i].app_guid);
    app_data.language = extra_args.language;
    app_data.brand_code = extra_args.brand_code;
    app_data.client_id = extra_args.client_id;
    app_data.installation_id = installation_id;
    app_data.experiment_labels = extra_args.apps[i].experiment_labels;
    apps_data_.push_back(app_data);
  }

  omaha_data_.app_id = kGoogleUpdateAppId;
  omaha_data_.language = extra_args.language;
  omaha_data_.brand_code = extra_args.brand_code;
  omaha_data_.client_id = extra_args.client_id;
  omaha_data_.installation_id = installation_id;
  omaha_data_.experiment_labels = extra_args.experiment_labels;
}

void Ping::LoadOmahaDataFromRegistry() {
  omaha_data_.app_id = kGoogleUpdateAppId;
  app_registry_utils::GetClientStateData(
      is_machine_,
      kGoogleUpdateAppId,
      NULL,
      NULL,         // ap is not used yet.
      &omaha_data_.language,
      &omaha_data_.brand_code,
      &omaha_data_.client_id,
      &omaha_data_.installation_id,
      &omaha_data_.experiment_labels,
      &omaha_data_.install_time_diff_sec,
      &omaha_data_.day_of_install);
}

void Ping::LoadAppDataFromRegistry(const std::vector<CString>& app_ids) {
  for (size_t i = 0; i != app_ids.size(); ++i) {
    AppData app_data;
    app_data.app_id = app_ids[i];
    app_registry_utils::GetClientStateData(
        is_machine_,
        app_data.app_id,
        &app_data.pv,
        NULL,         // ap is not used yet.
        &app_data.language,
        &app_data.brand_code,
        &app_data.client_id,
        &app_data.installation_id,
        &app_data.experiment_labels,
        &app_data.install_time_diff_sec,
        &app_data.day_of_install);
    apps_data_.push_back(app_data);
  }

  LoadOmahaDataFromRegistry();
}

void Ping::AddExtraOmahaLabel(const CString& label_set) {
  CORE_LOG(L3, (_T("[Ping::AddExtraOmahaLabel][%s]"), label_set));
  if (label_set.IsEmpty()) {
    return;
  }

  CString new_labels;
  if (ExperimentLabels::MergeLabelSets(omaha_data_.experiment_labels,
                                       label_set,
                                       &new_labels)) {
    omaha_data_.experiment_labels = new_labels;
  }
}

HRESULT Ping::Send(bool is_fire_and_forget) {
  CORE_LOG(L3, (_T("[Ping::Send]")));

  ASSERT1(ConfigManager::Instance()->CanUseNetwork(is_machine_));

  if (ping_request_->IsEmpty()) {
    CORE_LOG(L3, (_T("[Ping::Send did not send empty ping]")));
    return S_FALSE;
  }

  CString request_string;
  HRESULT hr = BuildRequestString(&request_string);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[BuildRequestString failed][0x%08x]"), hr));
    return hr;
  }

  const DWORD wait_timeout_ms = is_fire_and_forget ? 0 : INFINITE;
  hr = SendUsingGoogleUpdate(request_string, wait_timeout_ms);
  if (SUCCEEDED(hr)) {
    return hr;
  }

  CORE_LOG(LE, (_T("[Ping::SendUsingGoogleUpdate failed][0x%x]"), hr));

  hr = SendInProcess(request_string);
  if (SUCCEEDED(hr)) {
    return hr;
  }

  CORE_LOG(LE, (_T("[Ping::SendInProcess failed][0x%x]"), hr));

  return PersistPing(is_machine_, request_string);
}

void Ping::BuildOmahaPing(const CString& version,
                          const CString& next_version,
                          const PingEventPtr& ping_event) {
  xml::request::App app(BuildOmahaApp(version, next_version));
  app.ping_events.push_back(ping_event);
  ping_request_->AddApp(app);
}

void Ping::BuildOmahaPing(const CString& version,
                          const CString& next_version,
                          const PingEventPtr& ping_event1,
                          const PingEventPtr& ping_event2) {
  xml::request::App app(BuildOmahaApp(version, next_version));
  app.ping_events.push_back(ping_event1);
  app.ping_events.push_back(ping_event2);
  ping_request_->AddApp(app);
}

xml::request::App Ping::BuildOmahaApp(const CString& version,
                                      const CString& next_version) const {
  xml::request::App app;

  app.app_id                = omaha_data_.app_id;
  app.lang                  = omaha_data_.language;
  app.brand_code            = omaha_data_.brand_code;
  app.client_id             = omaha_data_.client_id;
  app.experiments           = omaha_data_.experiment_labels;
  app.iid                   = omaha_data_.installation_id;
  app.install_time_diff_sec = omaha_data_.install_time_diff_sec;
  app.day_of_install        = omaha_data_.day_of_install;

  app.version               = version;
  app.next_version          = next_version;

  return app;
}

void Ping::BuildAppPing(const CString& app_id,
                        const CString& version,
                        const CString& next_version,
                        const CString& client_id,
                        const PingEventPtr& ping_event) {
  xml::request::App app;

  app.app_id                = app_id;
  app.version               = version;
  app.next_version          = next_version;
  app.client_id             = client_id;

  app.ping_events.push_back(ping_event);
  ping_request_->AddApp(app);
}

void Ping::BuildAppsPing(const PingEventPtr& ping_event) {
  for (size_t i = 0; i != apps_data_.size(); ++i) {
    xml::request::App app;

    app.version               = apps_data_[i].pv;
    app.app_id                = apps_data_[i].app_id;
    app.lang                  = apps_data_[i].language;
    app.brand_code            = apps_data_[i].brand_code;
    app.client_id             = apps_data_[i].client_id;
    app.experiments           = apps_data_[i].experiment_labels;
    app.iid                   = apps_data_[i].installation_id;
    app.install_time_diff_sec = apps_data_[i].install_time_diff_sec;
    app.day_of_install        = apps_data_[i].day_of_install;

    app.ping_events.push_back(ping_event);
    ping_request_->AddApp(app);
  }
}

HRESULT Ping::SendUsingGoogleUpdate(const CString& request_string,
                                    DWORD wait_timeout_ms) const {
  CString pv;
  app_registry_utils::GetAppVersion(is_machine_, kGoogleUpdateAppId, &pv);
  if (VersionFromString(pv) < kMinOmahaVersionForPingOOP) {
    // Older versions could display a dialog box if they are run with /ping.
    return E_NOTIMPL;
  }

  CStringA request_string_utf8(WideToUtf8(request_string));
  CStringA ping_string_utf8;
  WebSafeBase64Escape(request_string_utf8, &ping_string_utf8);

  CommandLineBuilder builder(COMMANDLINE_MODE_PING);
  builder.set_ping_string(Utf8ToWideChar(ping_string_utf8,
                                         ping_string_utf8.GetLength()));
  CString args = builder.GetCommandLineArgs();

  scoped_process ping_process;
  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(is_machine_,
                                                         args,
                                                         address(ping_process));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to start ping process][0x%08x]"), hr));
    return hr;
  }

  if (wait_timeout_ms) {
    DWORD result = ::WaitForSingleObject(get(ping_process), wait_timeout_ms);
    DWORD exit_code(0);
    if (result == WAIT_OBJECT_0 &&
        ::GetExitCodeProcess(get(ping_process), &exit_code)) {
      ASSERT1(exit_code == 0 || FAILED(exit_code));
      return (exit_code == 0) ? S_OK : exit_code;
    } else {
      if (result == WAIT_TIMEOUT) {
        CORE_LOG(LW, (_T("[ping process did not finish in time][pid=%u]"),
            ::GetProcessId(get(ping_process))));
        VERIFY1(::TerminateProcess(get(ping_process), UINT_MAX));
      }
      return E_FAIL;
    }
  }

  return S_OK;
}

HRESULT Ping::SendInProcess(const CString& request_string) const {
  HRESULT hr = SendString(is_machine_, HeadersVector(), request_string);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[SendString failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT Ping::BuildRequestString(CString* request_string) const {
  ASSERT1(request_string);
  return ping_request_->Serialize(request_string);
}

CString Ping::GetPingRegPath(bool is_machine) {
  CString ping_reg_path = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  return AppendRegKeyPath(ping_reg_path, kRegKeyPing);
}

HRESULT Ping::LoadPersistedPings(bool is_machine, PingsVector* pings) {
  ASSERT1(pings);

  RegKey ping_reg_key;
  HRESULT hr = ping_reg_key.Open(GetPingRegPath(is_machine), KEY_READ);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[Unable to open Ping regkey][0x%x]"), hr));
    return hr;
  }

  int num_pings = ping_reg_key.GetValueCount();
  for (int i = 0; i < num_pings; ++i) {
    CString persisted_time_string;
    hr = ping_reg_key.GetValueNameAt(i, &persisted_time_string, NULL);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[GetValueNameAt failed][%d]"), i));
      continue;
    }

    time64 persisted_time = _tcstoui64(persisted_time_string, NULL, 10);
    if (persisted_time == 0 || persisted_time == _UI64_MAX) {
      CORE_LOG(LW, (_T("[Incorrect time value][%s]"), persisted_time_string));
      continue;
    }

    CString ping_string;
    hr = ping_reg_key.GetValue(persisted_time_string, &ping_string);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[GetValue failed][%s]"), persisted_time_string));
      continue;
    }

    pings->push_back(std::make_pair(persisted_time, ping_string));
  }

  return S_OK;
}

bool Ping::IsPingExpired(time64 persisted_time) {
  const time64 now = GetCurrent100NSTime();

  if (now < persisted_time) {
    CORE_LOG(LW, (_T("[Incorrect clock time][%I64u][%I64u]"),
                  now, persisted_time));
    return true;
  }

  const time64 time_difference = now - persisted_time;
  CORE_LOG(L3, (_T("[%I64u][%I64u][%I64u]"),
                now, persisted_time, time_difference));

  const bool result = time_difference >= kPingExpiry100ns;
  CORE_LOG(L3, (_T("[IsPingExpired][%d]"), result));
  return result;
}

HRESULT Ping::DeletePersistedPing(bool is_machine, time64 persisted_time) {
  CString persisted_time_string;
  persisted_time_string.Format(_T("%I64u"), persisted_time);
  CORE_LOG(L3, (_T("[Ping::DeletePersistedPing][%s]"), persisted_time_string));

  CString ping_reg_path(GetPingRegPath(is_machine));
  HRESULT hr = RegKey::DeleteValue(ping_reg_path, persisted_time_string);

  if (RegKey::IsKeyEmpty(ping_reg_path)) {
    VERIFY1(SUCCEEDED(RegKey::DeleteKey(ping_reg_path)));
  }

  return hr;
}

HRESULT Ping::PersistPing(bool is_machine, const CString& ping_string) {
  CString time_now_str;
  time_now_str.Format(_T("%I64u"), GetCurrent100NSTime());
  CORE_LOG(L3, (_T("[Ping::PersistPing][%s][%s]"), time_now_str, ping_string));

  return RegKey::SetValue(GetPingRegPath(is_machine),
                          time_now_str,
                          ping_string);
}

HRESULT Ping::SendPersistedPings(bool is_machine) {
  PingsVector pings;
  HRESULT hr = LoadPersistedPings(is_machine, &pings);
  if (FAILED(hr) && (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))) {
    return hr;
  }

  for (size_t i = 0; i != pings.size(); ++i) {
    time64 persisted_time = pings[i].first;
    int32 request_age = Time64ToInt32(GetCurrent100NSTime()) -
                        Time64ToInt32(persisted_time);
    const CString& ping_string(pings[i].second);

    CORE_LOG(L3, (_T("[Resending ping][%I64u][%d][%s]"),
                  persisted_time, request_age, ping_string));

    CString request_age_string;
    request_age_string.Format(_T("%d"), request_age);
    HeadersVector headers;
    headers.push_back(std::make_pair(kHeaderXRequestAge, request_age_string));

    hr = SendString(is_machine, headers, ping_string);

    if (SUCCEEDED(hr) || IsPingExpired(persisted_time)) {
      CORE_LOG(L3, (_T("[Deleting ping][0x%x]"), hr));
      VERIFY1(SUCCEEDED(DeletePersistedPing(is_machine, persisted_time)));
    }
  }

  return S_OK;
}

// TODO(omaha): Ping support for authenticated proxies.
HRESULT Ping::SendString(bool is_machine,
                         const HeadersVector& headers,
                         const CString& request_string) {
  ASSERT1(ConfigManager::Instance()->CanUseNetwork(is_machine));

  CORE_LOG(L3, (_T("[ping request string][%s]"), request_string));

  CString url;
  ConfigManager::Instance()->GetPingUrl(&url);

  // Impersonate the user if the caller is machine, running as local system,
  // and a user is logged on to the system.
  scoped_handle impersonation_token(
      goopdate_utils::GetImpersonationTokenForMachineProcess(is_machine));
  scoped_impersonation impersonate_user(get(impersonation_token));

  WebServicesClient web_service_client(is_machine);
  HRESULT hr(web_service_client.Initialize(url, headers, false));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[WebServicesClient::Initialize failed][0x%08x]"), hr));
    return hr;
  }

  scoped_ptr<xml::UpdateResponse> response(xml::UpdateResponse::Create());
  hr = web_service_client.SendString(&request_string, response.get());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[WebServicesClient::SendString failed][0x%08x]"), hr));
    return hr;
  }

  // If a ping is sent but the response is corrupted in some way (or we can't
  // persist the labels for some reason), returning a failure code would result
  // in the ping being persisted and re-sent later.  For this reason, we always
  // return a success code if we sent the ping, even if following actions fail.
  VERIFY1(SUCCEEDED(update_response_utils::ApplyExperimentLabelDeltas(
      is_machine,
      response.get())));

  return S_OK;
}

HRESULT Ping::HandlePing(bool is_machine, const CString& ping_string) {
  CORE_LOG(L3, (_T("[Ping::HandlePing][%s]"), ping_string));

  // |ping_string| is web safe base64 encoded. It must be decoded before
  // sending it.
  const CStringA ping_string_utf8(WideToUtf8(ping_string));

  CStringA request_string_utf8;
  const int out_buffer_length = ping_string_utf8.GetLength();
  char* out_buffer = request_string_utf8.GetBufferSetLength(out_buffer_length);

  const int num_chars = WebSafeBase64Unescape(ping_string_utf8,
                                              ping_string_utf8.GetLength(),
                                              out_buffer,
                                              out_buffer_length);
  if (num_chars < 0) {
    return E_FAIL;
  }
  request_string_utf8.ReleaseBufferSetLength(num_chars);

  return Ping::SendString(
      is_machine,
      HeadersVector(),
      Utf8ToWideChar(request_string_utf8, request_string_utf8.GetLength()));
}

}  // namespace omaha

