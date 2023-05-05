// Copyright 2019 Google LLC.
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

#include "omaha/goopdate/dm_client.h"

#include <shlobj.h>
#include <memory>
#include <utility>

#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/url_utils.h"
#include "omaha/goopdate/dm_messages.h"
#include "omaha/goopdate/dm_storage.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"

namespace omaha {
namespace dm_client {

RegistrationState GetRegistrationState(DmStorage* dm_storage) {
  ASSERT1(dm_storage);

  if (!dm_storage->GetDmToken().IsEmpty()) {
    return kRegistered;
  }

  return dm_storage->GetEnrollmentToken().IsEmpty()
      ? kNotManaged
      : kRegistrationPending;
}

// Log categories are used within this function as follows:
// OPT: diagnostic messages to be included in the log file in release builds.
// REPORT: error messages to be included in the Windows Event Log.
HRESULT RegisterIfNeeded(DmStorage* dm_storage, bool is_foreground) {
  ASSERT1(dm_storage);
  OPT_LOG(L1, (_T("[DmClient::RegisterIfNeeded][%d]"), is_foreground));

  // No work to be done if the process is not running as an administrator, since
  // we will not be able to persist anything.
  if (!::IsUserAnAdmin()) {
    OPT_LOG(L1, (_T("[RegisterIfNeeded][Process not Admin, exiting early]")));
    return S_FALSE;
  }

  if (dm_storage->IsInvalidDMToken()) {
    OPT_LOG(L1, (_T("[Cannot register since DM Token is invalid]")));
    return E_FAIL;
  }

  // No work to be done if a DM token was found.
  CStringA dm_token = dm_storage->GetDmToken();
  if (!dm_token.IsEmpty()) {
    OPT_LOG(L1, (_T("[Device is already registered]")));
    return S_FALSE;
  }

  // No work to be done if no enrollment token was found.
  CString enrollment_token = dm_storage->GetEnrollmentToken();
  if (enrollment_token.IsEmpty()) {
    OPT_LOG(L1, (_T("[No enrollment token found]")));
    return S_FALSE;
  }

  // Cannot register if no device ID was found.
  CString device_id = dm_storage->GetDeviceId();
  if (device_id.IsEmpty()) {
    REPORT_LOG(LE, (_T("[Device ID not found]")));
    return E_FAIL;
  }

  if (!is_foreground) {
    // Waits a while before registration. We are reusing the AutoUpdateJitterMs
    // for simplicity.
    const int dm_jitter_ms(ConfigManager::Instance()->GetAutoUpdateJitterMs());
    if (dm_jitter_ms > 0) {
      OPT_LOG(L1, (_T("[Applying DM Registration jitter][%d]"), dm_jitter_ms));
      ::Sleep(dm_jitter_ms);
    }
  }

  // RegisterWithRequest owns the SimpleRequest being created here.
  HRESULT hr = internal::RegisterWithRequest(dm_storage,
                                             std::make_unique<SimpleRequest>(),
                                             enrollment_token, device_id);
  OPT_LOG(L1, (_T("[Registration complete]")));

  return S_OK;
}

HRESULT RefreshPolicies() {
  // No work to be done if the process is not running as an administrator, since
  // we will not be able to persist anything.
  if (!::IsUserAnAdmin()) {
    REPORT_LOG(L1, (_T("[RefreshPolicies][Process not Admin, exiting early]")));
    return S_FALSE;
  }

  DmStorage* const dm_storage = DmStorage::Instance();
  if (dm_storage->IsInvalidDMToken()) {
    REPORT_LOG(L1, (_T("[Skipping RefreshPolicies as DMToken is invalid]")));
    return E_FAIL;
  }

  const CString dm_token = CString(dm_storage->GetDmToken());
  if (dm_token.IsEmpty()) {
    REPORT_LOG(L1, (_T("[Skipping RefreshPolicies as there is no DMToken]")));
    return S_FALSE;
  }

  const CString device_id = dm_storage->GetDeviceId();
  if (device_id.IsEmpty()) {
    REPORT_LOG(LE, (_T("[Device ID not found]")));
    return E_FAIL;
  }

  CachedPolicyInfo info;
  HRESULT hr = dm_storage->ReadCachedPolicyInfoFile(&info);
  if (FAILED(hr)) {
    REPORT_LOG(LW, (_T("[ReadCachedPolicyInfoFile failed][%#x]"), hr));
    // Not fatal, continue.
  }

  PolicyResponses responses;
  hr = internal::FetchPolicies(dm_storage, std::make_unique<SimpleRequest>(),
                               dm_token, device_id, info, &responses);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[FetchPolicies failed][%#x]"), hr));
    return hr;
  }

  hr = dm_storage->PersistPolicies(responses);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[PersistPolicies failed][%#x]"), hr));
    return hr;
  }

  CachedOmahaPolicy dm_policy;
  hr = dm_storage->ReadCachedOmahaPolicy(&dm_policy);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[ReadCachedOmahaPolicy failed][%#x]"), hr));
  } else {
    ConfigManager::Instance()->SetOmahaDMPolicies(dm_policy);
  }

  REPORT_LOG(L1, (_T("[RefreshPolicies complete]")));

  return S_OK;
}

namespace internal {

HRESULT RegisterWithRequest(DmStorage* dm_storage,
                            std::unique_ptr<HttpRequestInterface> http_request,
                            const CString& enrollment_token,
                            const CString& device_id) {
  ASSERT1(dm_storage);
  ASSERT1(http_request.get());

  std::vector<std::pair<CString, CString>> query_params = {
    {_T("request"), _T("register_policy_agent")},
  };

  // Make the request payload.
  CStringA payload = SerializeRegisterBrowserRequest(
      WideToUtf8(app_util::GetHostName()),
      WideToUtf8(SystemInfo::GetSerialNumber()),
      CStringA("Windows"),
      internal::GetOsVersion());
  if (payload.IsEmpty()) {
    REPORT_LOG(LE, (_T("[SerializeRegisterBrowserRequest failed]")));
    return E_FAIL;
  }

  std::vector<uint8> response;
  HRESULT hr = SendDeviceManagementRequest(
      std::move(http_request), payload,
      internal::FormatEnrollmentTokenAuthorizationHeader(enrollment_token),
      device_id, std::move(query_params), &response);

  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[SendDeviceManagementRequest failed][%#x]"), hr));
    internal::HandleDMResponseError(dm_storage, hr, response);
    return hr;
  }

  CStringA dm_token;
  hr = ParseDeviceRegisterResponse(response, &dm_token);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[ParseDeviceRegisterResponse failed][%#x]"), hr));
    return hr;
  }

  hr = dm_storage->StoreDmToken(dm_token);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[StoreDmToken failed][%#x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT SendPolicyValidationResultReportIfNeeded(
    std::unique_ptr<HttpRequestInterface> http_request, const CString& dm_token,
    const CString& device_id, const PolicyValidationResult& validation_result) {
  const CStringA payload =
      SerializePolicyValidationReportRequest(validation_result);
  if (payload.IsEmpty()) return S_OK;

  std::vector<std::pair<CString, CString>> query_params = {
      {_T("request"), _T("policy_validation_report")},
  };

  std::vector<uint8> response;
  HRESULT hr = SendDeviceManagementRequest(
      std::move(http_request), payload,
      FormatDMTokenAuthorizationHeader(dm_token), device_id,
      std::move(query_params), &response);
  REPORT_LOG(L1, (_T("[SendPolicyValidationResultReportIfNeeded][%#x]"), hr));
  return hr;
}

HRESULT FetchPolicies(DmStorage* dm_storage,
                      std::unique_ptr<HttpRequestInterface> http_request,
                      const CString& dm_token, const CString& device_id,
                      const CachedPolicyInfo& info,
                      PolicyResponses* responses) {
  ASSERT1(dm_storage);
  ASSERT1(http_request.get());
  ASSERT1(!dm_token.IsEmpty());
  ASSERT1(responses);

  std::vector<std::pair<CString, CString>> query_params = {
    {_T("request"), _T("policy")},
  };

  CStringA payload = SerializePolicyFetchRequest(
      WideToUtf8(app_util::GetHostName()),
      WideToUtf8(SystemInfo::GetSerialNumber()),
      CStringA(kGoogleUpdateMachineLevelApps),
      info);
  if (payload.IsEmpty()) {
    REPORT_LOG(LE, (_T("[SerializePolicyFetchRequest failed]")));
    return E_FAIL;
  }

  std::vector<uint8> response;
  HRESULT hr = SendDeviceManagementRequest(
      std::move(http_request), payload,
      FormatDMTokenAuthorizationHeader(dm_token), device_id,
      std::move(query_params), &response);

  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[SendDeviceManagementRequest failed][%#x]"), hr));
    internal::HandleDMResponseError(dm_storage, hr, response);
    return hr;
  }

  std::vector<PolicyValidationResult> validation_results;
  hr = ParseDevicePolicyResponse(response, info, dm_token, device_id, responses,
                                 &validation_results);
  for (const PolicyValidationResult& validation_result : validation_results) {
    SendPolicyValidationResultReportIfNeeded(std::make_unique<SimpleRequest>(),
                                             dm_token, device_id,
                                             validation_result);
  }

  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[ParseDevicePolicyResponse failed][%#x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT SendDeviceManagementRequest(
    std::unique_ptr<HttpRequestInterface> http_request, const CStringA& payload,
    const CString& authorization_header, const CString& device_id,
    std::vector<std::pair<CString, CString>> query_params,
    std::vector<uint8>* response) {
  ASSERT1(http_request.get());
  ASSERT1(response);

  // Get the network configuration.
  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_config_manager =
      NetworkConfigManager::Instance();
  HRESULT hr = network_config_manager.GetUserNetworkConfig(&network_config);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[GetUserNetworkConfig failed][%#x]"), hr));
    return hr;
  }

  // Create a network request and configure its headers.
  std::unique_ptr<NetworkRequest> request(
      new NetworkRequest(network_config->session()));
  request->AddHeader(_T("Content-Type"), kProtobufContentType);
  request->AddHeader(_T("Authorization"), authorization_header);

  // Set it up
  request->AddHttpRequest(http_request.release());

  // Form the request URL with query params.
  CString url;
  hr = ConfigManager::Instance()->GetDeviceManagementUrl(&url);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[GetDeviceManagementUrl failed][%#x]"), hr));
    return hr;
  }

  query_params.emplace_back(_T("agent"), internal::GetAgent());
  query_params.emplace_back(_T("apptype"), _T("Chrome"));
  query_params.emplace_back(_T("deviceid"), device_id);
  query_params.emplace_back(_T("platform"), internal::GetPlatform());

  hr = internal::AppendQueryParamsToUrl(query_params, &url);
  if (FAILED(hr)) {
    REPORT_LOG(LW, (_T("[AppendQueryParamsToUrl failed][%#x]"), hr));
    return hr;
  }

  hr = request->Post(url, payload, payload.GetLength(), response);
  if (FAILED(hr)) {
    REPORT_LOG(LE, (_T("[NetworkRequest::Post failed][%#x, %s]"), hr, url));
    return hr;
  }

  const int http_status_code = request->http_status_code();
  if (http_status_code != HTTP_STATUS_OK) {
    REPORT_LOG(LE, (_T("[NetworkRequest::Post failed][status code %d]"),
                    http_status_code));
    CStringA error_message;
    hr = ParseDeviceManagementResponseError(*response, &error_message);
    if (SUCCEEDED(hr)) {
      REPORT_LOG(LE, (_T("[Server returned: %S]"), error_message));
    }
    return E_FAIL;
  }

  return S_OK;
}

void HandleDMResponseError(DmStorage* dm_storage, HRESULT hr,
                           const std::vector<uint8>& response) {
  ASSERT1(dm_storage);

  if (hr != HRESULTFromHttpStatusCode(HTTP_STATUS_GONE)) {
    REPORT_LOG(LE, (_T("Unexpected HTTP error from the DM Server[%#x]"), hr));
    return;
  }

  // HTTP_STATUS_GONE implies that the device has been unenrolled.
  // Update the DM token and delete cached policies.
  if (ShouldDeleteDmToken(response)) {
    REPORT_LOG(L1, (_T("Remove DM token based on server's response.")));
    dm_storage->DeleteDmToken();
  } else {
    REPORT_LOG(L1, (_T("Invalidate DM token based on server's response.")));
    dm_storage->InvalidateDMToken();
  }

  DeleteBeforeOrAfterReboot(dm_storage->policy_responses_dir());

  // Set the Omaha DM Policies to an empty CachedOmahaPolicy so that the
  // currently-running process forgets about the policies loaded in
  // GoopdateImpl::InitializeGoopdateAndLoadResources.
  ConfigManager::Instance()->SetOmahaDMPolicies(CachedOmahaPolicy());
}

CString GetAgent() {
  CString agent;
  SafeCStringFormat(&agent, _T("%s %s()"), kAppName, GetVersionString());
  return agent;
}

CString GetPlatform() {
  const CString architecture = SystemInfo::GetArchitecture();

  int major_version = 0;
  int minor_version = 0;
  int service_pack_major = 0;
  int service_pack_minor = 0;
  if (!SystemInfo::GetSystemVersion(&major_version, &minor_version,
                                    &service_pack_major, &service_pack_minor)) {
    major_version = 0;
    minor_version = 0;
  }

  CString platform;
  SafeCStringFormat(&platform, _T("Windows NT|%s|%d.%d.0"),
                    architecture == kArchAmd64 ? _T("x86_64") : architecture,
                    major_version, minor_version);
  return platform;
}

CStringA GetOsVersion() {
  CString os_version;
  CString service_pack;

  if (FAILED(goopdate_utils::GetOSInfo(&os_version, &service_pack))) {
    return "0.0.0.0";
  }
  return WideToUtf8(os_version);
}

HRESULT AppendQueryParamsToUrl(
    const std::vector<std::pair<CString, CString>>& query_params,
    CString* url) {
  ASSERT1(url);
  CString query;
  HRESULT hr = BuildQueryString(query_params, &query);
  if (FAILED(hr)) {
    return hr;
  }

  CString result;
  SafeCStringFormat(&result, _T("%s?%s"), *url, query);
  *url = result;
  return S_OK;
}

CString FormatEnrollmentTokenAuthorizationHeader(
    const CString& token) {
  CString header_value;
  SafeCStringFormat(&header_value, _T("GoogleEnrollmentToken token=%s"), token);
  return header_value;
}

CString FormatDMTokenAuthorizationHeader(
    const CString& token) {
  CString header_value;
  SafeCStringFormat(&header_value, _T("GoogleDMToken token=%s"), token);
  return header_value;
}

}  // namespace internal
}  // namespace dm_client
}  // namespace omaha
