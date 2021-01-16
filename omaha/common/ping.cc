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

#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
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
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

const TCHAR* const Ping::kRegKeyPersistedPings = _T("PersistedPings");
const TCHAR* const Ping::kRegValuePersistedPingTime = _T("PersistedPingTime");
const TCHAR* const Ping::kRegValuePersistedPingString =
    _T("PersistedPingString");
const time64 Ping::kPersistedPingExpiry100ns  = 10 * kDaysTo100ns;  // 10 days.

// Minimum compatible Omaha version that understands the /ping command line.
// 1.3.0.0.
const ULONGLONG kMinOmahaVersionForPingOOP = 0x0001000300000000;

Ping::Ping(bool is_machine,
           const CString& session_id,
           const CString& install_source,
           const CString& request_id) {
  Initialize(is_machine, session_id, install_source, request_id);
}

Ping::Ping(bool is_machine,
           const CString& session_id,
           const CString& install_source) {
  CString request_id;
  VERIFY_SUCCEEDED(GetGuid(&request_id));
  Initialize(is_machine, session_id, install_source, request_id);
}

void Ping::Initialize(bool is_machine,
                      const CString& session_id,
                      const CString& install_source,
                      const CString& request_id) {
  is_machine_ = is_machine;
  request_id_ = request_id;

  ping_request_.reset(xml::UpdateRequest::Create(is_machine,
                                                 session_id,
                                                 install_source,
                                                 CString(),
                                                 request_id));
  CString shell_version_string =
      goopdate_utils::GetInstalledShellVersion(is_machine);
  if (!shell_version_string.IsEmpty()) {
    ping_request_->set_omaha_shell_version(shell_version_string);
  }
}

Ping::~Ping() {
}

void Ping::BuildRequest(const App* app) {
  update_request_utils::BuildRequest(app, false, ping_request_.get());
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
      &omaha_data_.cohort,
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
        &app_data.cohort,
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

  // When the ping is sent successfully, the ScopeGuard ensures that the
  // corresponding persisted ping is deleted. In failure cases, the persisted
  // ping is retained to be sent in a subsequent UA run.
  HRESULT hr = E_FAIL;
  ON_SCOPE_EXIT_OBJ(*this,
                    &Ping::DeletePersistedPingOnSuccess,
                    ByRef(hr));

  if (ping_request_->IsEmpty()) {
    CORE_LOG(L3, (_T("[Ping::Send did not send empty ping]")));
    hr = S_FALSE;
    return hr;
  }

  CString request_string;
  hr = BuildRequestString(&request_string);
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

  return hr;
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
  app.experiments           =
      ExperimentLabels::RemoveTimestamps(omaha_data_.experiment_labels);
  app.iid                   = omaha_data_.installation_id;
  app.install_time_diff_sec = omaha_data_.install_time_diff_sec;
  app.day_of_install        = omaha_data_.day_of_install;
  app.cohort                = omaha_data_.cohort.cohort;
  app.cohort_hint           = omaha_data_.cohort.hint;
  app.cohort_name           = omaha_data_.cohort.name;

  app.version               = version;
  app.next_version          = next_version;

  return app;
}

void Ping::BuildAppsPing(const PingEventPtr& ping_event) {
  for (size_t i = 0; i != apps_data_.size(); ++i) {
    xml::request::App app;

    app.version               = apps_data_[i].pv;
    app.app_id                = apps_data_[i].app_id;
    app.lang                  = apps_data_[i].language;
    app.brand_code            = apps_data_[i].brand_code;
    app.client_id             = apps_data_[i].client_id;
    app.experiments           =
        ExperimentLabels::RemoveTimestamps(apps_data_[i].experiment_labels);
    app.iid                   = apps_data_[i].installation_id;
    app.install_time_diff_sec = apps_data_[i].install_time_diff_sec;
    app.day_of_install        = apps_data_[i].day_of_install;
    app.cohort                = apps_data_[i].cohort.cohort;
    app.cohort_hint           = apps_data_[i].cohort.hint;
    app.cohort_name           = apps_data_[i].cohort.name;

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
                                                         StartMode::kBackground,
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

CString Ping::GetPersistedPingsRegPath(bool is_machine) {
  CString persisted_pings_reg_path = is_machine ? MACHINE_REG_UPDATE :
                                                  USER_REG_UPDATE;
  return AppendRegKeyPath(persisted_pings_reg_path, kRegKeyPersistedPings);
}

HRESULT Ping::LoadPersistedPings(bool is_machine,
                                 PingsVector* persisted_pings) {
  ASSERT1(persisted_pings);

  RegKey persisted_pings_reg_key;
  CString persisted_pings_reg_path(GetPersistedPingsRegPath(is_machine));
  HRESULT hr = persisted_pings_reg_key.Open(persisted_pings_reg_path, KEY_READ);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[Unable to open Persisted pings regkey][%#x]"), hr));
    return hr;
  }

  int num_pings = persisted_pings_reg_key.GetSubkeyCount();
  for (int i = 0; i < num_pings; ++i) {
    CString persisted_subkey_name;
    hr = persisted_pings_reg_key.GetSubkeyNameAt(i, &persisted_subkey_name);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[GetSubkeyNameAt failed][%d]"), i));
      continue;
    }

    RegKey persisted_ping_reg_key;
    CString persisted_ping_reg_path(
        AppendRegKeyPath(persisted_pings_reg_path, persisted_subkey_name));
    hr = persisted_ping_reg_key.Open(persisted_ping_reg_path, KEY_READ);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[Unable to open Persisted Ping subkey][%s][%#x]"),
                    persisted_subkey_name, hr));
      continue;
    }

    CString persisted_time_string;
    hr = persisted_ping_reg_key.GetValue(kRegValuePersistedPingTime,
                                         &persisted_time_string);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[GetValue kRegValuePersistedPingTime failed][%#x]"),
                    hr));
      continue;
    }

    time64 persisted_time = _tcstoui64(persisted_time_string, NULL, 10);
    if (persisted_time == 0 || persisted_time == _UI64_MAX) {
      CORE_LOG(LW, (_T("[Incorrect time value][%s]"), persisted_time_string));
      continue;
    }

    CString persisted_ping_string;
    hr = persisted_ping_reg_key.GetValue(kRegValuePersistedPingString,
                                         &persisted_ping_string);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[GetValue kRegValuePersistedPingString failed][%#x]"),
                    hr));
      continue;
    }

    persisted_pings->push_back(std::make_pair(
        persisted_subkey_name,
        std::make_pair(persisted_time, persisted_ping_string)));
    CORE_LOG(L6, (_T("[Persisted ping][%s][%s][%s]"),
                  persisted_subkey_name,
                  persisted_time_string,
                  persisted_ping_string));
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

  const bool result = time_difference >= kPersistedPingExpiry100ns;
  CORE_LOG(L3, (_T("[IsPingExpired][%d]"), result));
  return result;
}

HRESULT Ping::DeletePersistedPing(bool is_machine,
                                  const CString& persisted_subkey_name) {
  CORE_LOG(L3, (_T("[Ping::DeletePersistedPing][%s]"), persisted_subkey_name));

  CString persisted_pings_reg_path(GetPersistedPingsRegPath(is_machine));
  CString persisted_ping_reg_path = AppendRegKeyPath(persisted_pings_reg_path,
                                                     persisted_subkey_name);

  if (!RegKey::HasKey(persisted_ping_reg_path)) {
    return S_OK;
  }

  // Registry writes to HKLM need admin.
  scoped_revert_to_self revert_to_self;

  return RegKey::DeleteKey(persisted_ping_reg_path);
}

void Ping::DeletePersistedPingOnSuccess(const HRESULT& hr) {
  if (SUCCEEDED(hr)) {
    VERIFY_SUCCEEDED(DeletePersistedPing(is_machine_, request_id_));
  }
}

CString Ping::GetPersistedPingRegPath() {
  return AppendRegKeyPath(GetPersistedPingsRegPath(is_machine_), request_id_);
}

HRESULT Ping::PersistPing() {
  CString ping_string;
  HRESULT hr = BuildRequestString(&ping_string);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[PersistPing][BuildRequestString failed][%#x]"), hr));
    return hr;
  }

  CString time_now_str;
  SafeCStringFormat(&time_now_str, _T("%I64u"), GetCurrent100NSTime());
  CORE_LOG(L3, (_T("[Ping::PersistPing][%s][%s][%s]"),
                request_id_, time_now_str, ping_string));

  CString ping_reg_path(GetPersistedPingRegPath());

  // Registry writes to HKLM need admin.
  scoped_revert_to_self revert_to_self;
  ASSERT1(!is_machine_ || vista_util::IsUserAdmin());

  hr = RegKey::SetValue(ping_reg_path,
                        kRegValuePersistedPingString,
                        ping_string);
  if (FAILED(hr)) {
    return hr;
  }

  return RegKey::SetValue(ping_reg_path,
                          kRegValuePersistedPingTime,
                          time_now_str);
}

HRESULT Ping::SendPersistedPings(bool is_machine) {
  PingsVector persisted_pings;
  HRESULT hr = LoadPersistedPings(is_machine, &persisted_pings);
  if (FAILED(hr) && (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))) {
    return hr;
  }

  for (size_t i = 0; i != persisted_pings.size(); ++i) {
    const CString& persisted_subkey_name(persisted_pings[i].first);
    time64 persisted_time = persisted_pings[i].second.first;
    int32 request_age = Time64ToInt32(GetCurrent100NSTime()) -
                        Time64ToInt32(persisted_time);
    const CString& persisted_ping_string(persisted_pings[i].second.second);

    CORE_LOG(L3, (_T("[Resending persisted ping][%s][%I64u][%d][%s]"),
                  persisted_subkey_name,
                  persisted_time,
                  request_age,
                  persisted_ping_string));

    CString request_age_string;
    SafeCStringFormat(&request_age_string, _T("%d"), request_age);
    HeadersVector headers;
    headers.push_back(std::make_pair(kHeaderXRequestAge, request_age_string));

    hr = SendString(is_machine, headers, persisted_ping_string);

    if (SUCCEEDED(hr) || IsPingExpired(persisted_time)) {
      CORE_LOG(L3, (_T("[Deleting persisted ping][0x%x]"), hr));
      VERIFY_SUCCEEDED(DeletePersistedPing(is_machine,
                                            persisted_subkey_name));
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

  std::unique_ptr<xml::UpdateResponse> response(xml::UpdateResponse::Create());
  hr = web_service_client.SendString(false, &request_string, response.get());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[WebServicesClient::SendString failed][0x%08x]"), hr));
    return hr;
  }

  // If a ping is sent but the response is corrupted in some way (or we can't
  // persist the labels for some reason), returning a failure code would result
  // in the ping being persisted and re-sent later.  For this reason, we always
  // return a success code if we sent the ping, even if following actions fail.

  // Registry writes to HKLM need admin.
  scoped_revert_to_self revert_to_self;
  VERIFY_SUCCEEDED(update_response_utils::ApplyExperimentLabelDeltas(
      is_machine,
      response.get()));

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

HRESULT SendReliablePing(Ping* ping, bool is_fire_and_forget) {
  CORE_LOG(L6, (_T("[Ping::SendReliablePing]")));
  ASSERT1(ping);

  VERIFY_SUCCEEDED(ping->PersistPing());
  return ping->Send(is_fire_and_forget);
}

}  // namespace omaha

