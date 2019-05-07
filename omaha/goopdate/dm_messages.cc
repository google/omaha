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

#include "omaha/base/debug.h"
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

} // namespace

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
