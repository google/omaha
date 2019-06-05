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

#include "omaha/goopdate/dm_messages.h"

#include <limits>
#include <utility>

#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"

namespace omaha {

namespace {

void SerializeToCStringA(const ::google::protobuf_opensource::Message& message,
                         CStringA* output) {
  ASSERT1(output);
  output->Empty();
  size_t byte_size = message.ByteSizeLong();
  if (byte_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return;
  }
  ::google::protobuf_opensource::uint8* buffer =
        reinterpret_cast<::google::protobuf_opensource::uint8*>(
            output->GetBufferSetLength(static_cast<int>(byte_size)));
  ::google::protobuf_opensource::uint8* end =
        message.SerializeWithCachedSizesToArray(buffer);
  output->ReleaseBufferSetLength(end - buffer);
}

}  // namespace

CStringA SerializeRegisterBrowserRequest(const CStringA& machine_name,
                                         const CStringA& os_platform,
                                         const CStringA& os_version) {
  enterprise_management::DeviceManagementRequest dm_request;

  ::enterprise_management::RegisterBrowserRequest* request =
        dm_request.mutable_register_browser_request();
  request->set_machine_name(machine_name, machine_name.GetLength());
  request->set_os_platform(os_platform, os_platform.GetLength());
  request->set_os_version(os_version, os_version.GetLength());

  CStringA result;
  SerializeToCStringA(dm_request, &result);
  return result;
}

CStringA SerializePolicyFetchRequest(const CStringA& policy_type) {
  // Request signed policy blobs. kPolicyVerificationKeyHash needs to be kept in
  // sync with the corresponding value in Chromium's cloud_policy_constants.cc.
  static constexpr char kPolicyVerificationKeyHash[] = "1:356l7w";

  enterprise_management::DeviceManagementRequest policy_request;

  enterprise_management::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->add_requests();
  policy_fetch_request->set_policy_type(policy_type);
  policy_fetch_request->set_signature_type(
      enterprise_management::PolicyFetchRequest::SHA1_RSA);
  policy_fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);

  CStringA result;
  SerializeToCStringA(policy_request, &result);
  return result;
}

HRESULT ParseDeviceRegisterResponse(const std::vector<uint8>& response,
                                    CStringA* dm_token) {
  ASSERT1(dm_token);
  enterprise_management::DeviceManagementResponse dm_response;

  if (response.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return E_FAIL;
  }

  if (!dm_response.ParseFromArray(response.data(),
                                  static_cast<int>(response.size()))) {
    return E_FAIL;
  }

  if (!dm_response.has_register_response()) {
    return E_FAIL;
  }

  if (!dm_response.register_response().has_device_management_token()) {
    return E_FAIL;
  }

  const ::std::string& token =
      dm_response.register_response().device_management_token();
  if (token.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return E_FAIL;
  }
  dm_token->SetString(token.data(), static_cast<int>(token.size()));

  return S_OK;
}

HRESULT ParseDevicePolicyResponse(const std::vector<uint8>& dm_response_array,
                                  PolicyResponsesMap* response_map) {
  ASSERT1(response_map);
  enterprise_management::DeviceManagementResponse dm_response;

  if (dm_response_array.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    return E_FAIL;
  }

  if (!dm_response.ParseFromArray(dm_response_array.data(),
                                  static_cast<int>(dm_response_array.size()))) {
    return E_FAIL;
  }

  if (!dm_response.has_policy_response() ||
      dm_response.policy_response().responses_size() == 0) {
    return E_FAIL;
  }

  const enterprise_management::DevicePolicyResponse& policy_response =
      dm_response.policy_response();
  PolicyResponsesMap responses;
  for (int i = 0; i < policy_response.responses_size(); ++i) {
    const enterprise_management::PolicyFetchResponse& response =
        policy_response.responses(i);
    enterprise_management::PolicyData policy_data;
    if (!policy_data.ParseFromString(response.policy_data()) ||
        !policy_data.IsInitialized() ||
        !policy_data.has_policy_type()) {
      OPT_LOG(LW, (_T("Ignoring invalid PolicyData")));
      continue;
    }

    const std::string& type = policy_data.policy_type();
    if (responses.find(type) != responses.end()) {
      OPT_LOG(LW, (_T("Duplicate PolicyFetchResponse for type: %S"),
                   type.c_str()));
      continue;
    }

    std::string policy_fetch_response;
    if (!response.SerializeToString(&policy_fetch_response)) {
      OPT_LOG(LW, (_T("Failed to serialize response for type: %S"),
                   type.c_str()));
      continue;
    }

    responses[type] = std::move(policy_fetch_response);
  }

  *response_map = std::move(responses);
  return S_OK;
}

HRESULT ParseDeviceManagementResponseError(const std::vector<uint8>& response,
                                           CStringA* error_message) {
  ASSERT1(error_message);
  enterprise_management::DeviceManagementResponse dm_response;

  if (response.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return E_FAIL;
  }

  if (!dm_response.ParseFromArray(response.data(),
                                  static_cast<int>(response.size()))) {
    return E_FAIL;
  }

  if (!dm_response.has_error_message()) {
    return S_FALSE;
  }

  const ::std::string& message = dm_response.error_message();
  if (message.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return E_FAIL;
  }
  error_message->SetString(message.data(), static_cast<int>(message.size()));

  return S_OK;
}

}  // namespace omaha
