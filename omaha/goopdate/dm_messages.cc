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

#include "crypto/signature_verifier_win.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"

namespace omaha {

namespace {

// Request signed policy blobs. kPolicyVerificationKeyHash and
// kPolicyVerificationKey need to be kept in sync with the corresponding values
// in Chromium's cloud_policy_constants.cc.
constexpr char kPolicyVerificationKeyHash[] = "1:356l7w";
const uint8_t kPolicyVerificationKey[] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00,
    0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xA7, 0xB3, 0xF9,
    0x0D, 0xC7, 0xC7, 0x8D, 0x84, 0x3D, 0x4B, 0x80, 0xDD, 0x9A, 0x2F, 0xF8,
    0x69, 0xD4, 0xD1, 0x14, 0x5A, 0xCA, 0x04, 0x4B, 0x1C, 0xBC, 0x28, 0xEB,
    0x5E, 0x10, 0x01, 0x36, 0xFD, 0x81, 0xEB, 0xE4, 0x3C, 0x16, 0x40, 0xA5,
    0x8A, 0xE6, 0x08, 0xEE, 0xEF, 0x39, 0x1F, 0x6B, 0x10, 0x29, 0x50, 0x84,
    0xCE, 0xEE, 0x33, 0x5C, 0x48, 0x4A, 0x33, 0xB0, 0xC8, 0x8A, 0x66, 0x0D,
    0x10, 0x11, 0x9D, 0x6B, 0x55, 0x4C, 0x9A, 0x62, 0x40, 0x9A, 0xE2, 0xCA,
    0x21, 0x01, 0x1F, 0x10, 0x1E, 0x7B, 0xC6, 0x89, 0x94, 0xDA, 0x39, 0x69,
    0xBE, 0x27, 0x28, 0x50, 0x5E, 0xA2, 0x55, 0xB9, 0x12, 0x3C, 0x79, 0x6E,
    0xDF, 0x24, 0xBF, 0x34, 0x88, 0xF2, 0x5E, 0xD0, 0xC4, 0x06, 0xEE, 0x95,
    0x6D, 0xC2, 0x14, 0xBF, 0x51, 0x7E, 0x3F, 0x55, 0x10, 0x85, 0xCE, 0x33,
    0x8F, 0x02, 0x87, 0xFC, 0xD2, 0xDD, 0x42, 0xAF, 0x59, 0xBB, 0x69, 0x3D,
    0xBC, 0x77, 0x4B, 0x3F, 0xC7, 0x22, 0x0D, 0x5F, 0x72, 0xC7, 0x36, 0xB6,
    0x98, 0x3D, 0x03, 0xCD, 0x2F, 0x68, 0x61, 0xEE, 0xF4, 0x5A, 0xF5, 0x07,
    0xAE, 0xAE, 0x79, 0xD1, 0x1A, 0xB2, 0x38, 0xE0, 0xAB, 0x60, 0x5C, 0x0C,
    0x14, 0xFE, 0x44, 0x67, 0x2C, 0x8A, 0x08, 0x51, 0x9C, 0xCD, 0x3D, 0xDB,
    0x13, 0x04, 0x57, 0xC5, 0x85, 0xB6, 0x2A, 0x0F, 0x02, 0x46, 0x0D, 0x2D,
    0xCA, 0xE3, 0x3F, 0x84, 0x9E, 0x8B, 0x8A, 0x5F, 0xFC, 0x4D, 0xAA, 0xBE,
    0xBD, 0xE6, 0x64, 0x9F, 0x26, 0x9A, 0x2B, 0x97, 0x69, 0xA9, 0xBA, 0x0B,
    0xBD, 0x48, 0xE4, 0x81, 0x6B, 0xD4, 0x4B, 0x78, 0xE6, 0xAF, 0x95, 0x66,
    0xC1, 0x23, 0xDA, 0x23, 0x45, 0x36, 0x6E, 0x25, 0xF3, 0xC7, 0xC0, 0x61,
    0xFC, 0xEC, 0x66, 0x9D, 0x31, 0xD4, 0xD6, 0xB6, 0x36, 0xE3, 0x7F, 0x81,
    0x87, 0x02, 0x03, 0x01, 0x00, 0x01};

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

std::string GetPolicyVerificationKey() {
  return std::string(reinterpret_cast<const char*>(kPolicyVerificationKey),
                     sizeof(kPolicyVerificationKey));
}

bool VerifySignature(const std::string& data,
                     const std::string& key,
                     const std::string& signature,
                     ALG_ID algorithm_id) {
  crypto::SignatureVerifierWin verifier;
  if (!verifier.VerifyInit(algorithm_id,
                           reinterpret_cast<const uint8_t*>(signature.data()),
                           signature.size(),
                           reinterpret_cast<const uint8_t*>(key.data()),
                           key.size())) {
    REPORT_LOG(LE, (_T("[VerifySignature][Invalid signature/key]")));
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(data.data()),
                        data.size());

  return verifier.VerifyFinal();
}

bool CheckVerificationKeySignature(
    const enterprise_management::PolicyData& policy_data,
    const std::string& key,
    const std::string& verification_key,
    const std::string& signature) {
  enterprise_management::PublicKeyVerificationData signed_data;
  signed_data.set_new_public_key(key);

  std::string username = policy_data.username();
  std::string domain = username.substr(username.rfind('@') + 1);
  if (domain.empty()) {
    REPORT_LOG(LE, (_T("[CheckVerificationKeySignature]")
                    _T("[Domain not found in policy][%S]"), username.c_str()));
    return false;
  }

  signed_data.set_domain(domain);

  if (policy_data.has_public_key_version()) {
    signed_data.set_new_public_key_version(policy_data.public_key_version());
  }

  std::string signed_data_as_string;
  if (!signed_data.SerializeToString(&signed_data_as_string)) {
    REPORT_LOG(LE, (_T("[CheckVerificationKeySignature]")
                    _T("[Could not serialize key and domain to string]")));
    return false;
  }

  return VerifySignature(signed_data_as_string,
                         verification_key,
                         signature,
                         CALG_SHA_256);
}

// Verifies that the |new_public_key_verification_data_signature| verifies with
// the hardcoded |GetPolicyVerificationKey()| for the |new_public_key| in
// |fetch_response|.
bool CheckNewPublicKeyVerificationSignature(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    const enterprise_management::PolicyData& policy_data) {
  if (!fetch_response.has_new_public_key_verification_data_signature()) {
    REPORT_LOG(LE, (_T("[CheckNewPublicKeyVerificationSignature]")
        _T("[Policy missing new_public_key_verification_data_signature]")));
    return false;
  }

  if (!CheckVerificationKeySignature(
           policy_data,
           fetch_response.new_public_key(),
           GetPolicyVerificationKey(),
           fetch_response.new_public_key_verification_data_signature())) {
    REPORT_LOG(LE, (_T("[CheckNewPublicKeyVerificationSignature]")
                    _T("[Signature verification failed]")));
    return false;
  }

  return true;
}

HRESULT ValidatePolicy(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    const enterprise_management::PolicyData& policy_data,
    const std::string& cached_public_key) {
  // We expect to use the cached public key for policy data validation, unless
  // there is a new public key in the response, in which case we first validate
  // the new public key and then use the new public key for the policy data
  // validation.
  const std::string* signature_key = &cached_public_key;

  if (fetch_response.has_new_public_key()) {
    // Validate new_public_key() against the hard-coded verification key.
    if (!CheckNewPublicKeyVerificationSignature(fetch_response, policy_data)) {
      REPORT_LOG(LE, (_T("[ValidatePolicy]")
                      _T("[Failed CheckNewPublicKeyVerificationSignature]")));
      return E_FAIL;
    }

    // Also validate new_public_key() against the cached_public_key, if the
    // latter exists.
    if (!cached_public_key.empty()) {
      if (!fetch_response.has_new_public_key_signature() ||
          !VerifySignature(fetch_response.new_public_key(),
                           cached_public_key,
                           fetch_response.new_public_key_signature(),
                           CALG_SHA1)) {
        REPORT_LOG(LE, (_T("[ValidatePolicy]")
                        _T("[Verification against cached public key failed]")));
        return E_FAIL;
      }
    }

    // Now that the new public key has been successfully verified, we rotate to
    // use it for future policy data validation.
    signature_key = &fetch_response.new_public_key();
  }


  if (!fetch_response.has_policy_data_signature() ||
      !VerifySignature(fetch_response.policy_data(),
                       *signature_key,
                       fetch_response.policy_data_signature(),
                       CALG_SHA1)) {
    REPORT_LOG(LE, (_T("[ValidatePolicy]")
                    _T("[Failed to verify the signature on policy_data()]")));
    return E_FAIL;
  }

  return S_OK;
}

}  // namespace

HRESULT GetCachedPublicKeyFromResponse(const std::string& string_response,
                                       CachedPublicKey* key) {
  ASSERT1(key);

  *key = {};

  enterprise_management::PolicyFetchResponse response;
  enterprise_management::PolicyData policy_data;
  if (string_response.empty() ||
      !response.ParseFromString(string_response) ||
      !policy_data.ParseFromString(response.policy_data())) {
    return E_UNEXPECTED;
  }

  key->key = response.new_public_key();
  if (policy_data.has_public_key_version()) {
    key->is_version_valid = true;
    key->version = policy_data.public_key_version();
  }

  return S_OK;
}

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

CStringA SerializePolicyFetchRequest(const CStringA& policy_type,
                                     const CachedPublicKey& key) {
  enterprise_management::DeviceManagementRequest policy_request;

  enterprise_management::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->add_requests();
  policy_fetch_request->set_policy_type(policy_type);
  policy_fetch_request->set_signature_type(
      enterprise_management::PolicyFetchRequest::SHA1_RSA);
  policy_fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);

  if (key.is_version_valid) {
    policy_fetch_request->set_public_key_version(key.version);
  }

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
                                  const CachedPublicKey& key,
                                  PolicyResponses* responses_out) {
  ASSERT1(responses_out);

  enterprise_management::DeviceManagementResponse dm_response;
  responses_out->has_new_public_key = false;

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
      REPORT_LOG(LW, (_T("Ignoring invalid PolicyData")));
      continue;
    }

    const std::string& type = policy_data.policy_type();
    if (responses.find(type) != responses.end()) {
      REPORT_LOG(LW, (_T("Duplicate PolicyFetchResponse for type: %S"),
                      type.c_str()));
      continue;
    }

    std::string policy_fetch_response;
    if (!response.SerializeToString(&policy_fetch_response)) {
      REPORT_LOG(LW, (_T("Failed to serialize response for type: %S"),
                      type.c_str()));
      continue;
    }

    HRESULT hr = ValidatePolicy(response, policy_data, key.key);
    if (FAILED(hr)) {
      REPORT_LOG(LW,
          (_T("[ParseDevicePolicyResponse][Failed ValidatePolicy][%d][%#x]"),
           key.version, hr));
      continue;
    }

    if (!responses_out->has_new_public_key && response.has_new_public_key()) {
      responses_out->has_new_public_key = true;
    }

    responses[type] = std::move(policy_fetch_response);
  }

  responses_out->responses = std::move(responses);
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
