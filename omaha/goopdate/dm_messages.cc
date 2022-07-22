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

#include <atlstr.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

#include "crypto/signature_verifier_win.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/common/const_group_policy.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"
#include "wireless/android/enterprise/devicemanagement/proto/omaha_settings.pb.h"

namespace omaha {

namespace {

namespace edm = ::wireless_android_enterprise_devicemanagement;

// Request signed policy blobs. kPolicyVerificationKeyHash and
// kPolicyVerificationKey need to be kept in sync with the corresponding values
// in Chromium's cloud_policy_constants.cc.
constexpr char kPolicyVerificationKeyHash[] = "1:356l7w";
constexpr uint8_t kPolicyVerificationKey[] = {
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

constexpr std::array<int64, 3> kInstallPolicyValidValues = {
    kPolicyDisabled, kPolicyEnabled, kPolicyEnabledMachineOnly};

constexpr std::array<int64, 4> kUpdatePolicyValidValues = {
    kPolicyDisabled, kPolicyEnabled, kPolicyManualUpdatesOnly,
    kPolicyAutomaticUpdatesOnly};

constexpr std::array<const TCHAR*, 5> kProxyModeValidValues = {
    kProxyModeDirect,       kProxyModeAutoDetect, kProxyModePacScript,
    kProxyModeFixedServers, kProxyModeSystem,
};

template <typename Container, typename Value>
bool Contains(const Container& container, const Value& value) {
  return std::find(std::begin(container), std::end(container), value) !=
         std::end(container);
}

template <typename Container>
bool ContainsStringNoCase(const Container& container,
                          const CString& string_to_find) {
  return std::find_if(std::begin(container), std::end(container),
                      [&](const auto& value) {
                        return string_to_find.CompareNoCase(value) == 0;
                      }) != std::end(container);
}

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

bool VerifySignature(const std::string& data, const std::string& key,
                     const std::string& signature, ALG_ID algorithm_id) {
  crypto::SignatureVerifierWin verifier;
  if (!verifier.VerifyInit(
          algorithm_id, reinterpret_cast<const uint8_t*>(signature.data()),
          signature.size(), reinterpret_cast<const uint8_t*>(key.data()),
          key.size())) {
    REPORT_LOG(LE, (_T("[VerifySignature][Invalid signature/key]")));
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(data.data()),
                        data.size());

  return verifier.VerifyFinal();
}

// Verifies that the |new_public_key_verification_data_signature| verifies with
// the hardcoded |GetPolicyVerificationKey()| for the |new_public_key| in
// |fetch_response|.
bool CheckNewPublicKeyVerificationSignature(
    const enterprise_management::PolicyFetchResponse& fetch_response) {
  if (!fetch_response.has_new_public_key_verification_data() ||
      !fetch_response.has_new_public_key_verification_data_signature()) {
    REPORT_LOG(
        LE,
        (_T("[CheckNewPublicKeyVerificationSignature]")
         _T("[Policy missing new_public_key_verification_data or signature]")));
    return false;
  }

  if (!VerifySignature(
          fetch_response.new_public_key_verification_data(),
          GetPolicyVerificationKey(),
          fetch_response.new_public_key_verification_data_signature(),
          CALG_SHA_256)) {
    REPORT_LOG(LE, (_T("[CheckNewPublicKeyVerificationSignature]")
                    _T("[Signature verification failed]")));
    return false;
  }

  return true;
}

// We expect to return the cached public key for policy data validation, unless
// there is a new public key in the response, in which case we first validate
// the new public key and then return the new public key for the policy data
// validation.
bool ValidateNewPublicKey(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    const std::string& cached_public_key, std::string* signature_key,
    PolicyValidationResult* validation_result) {
  ASSERT1(signature_key);
  if (!fetch_response.has_new_public_key_verification_data()) {
    if (cached_public_key.empty()) {
      REPORT_LOG(LE, (_T("[ValidateNewPublicKey]")
                      _T("[No public key cached or in response]")));
      validation_result->status =
          PolicyValidationResult::Status::kValidationBadSignature;
      return false;
    }

    *signature_key = cached_public_key;
    return true;
  }

  // Validate new_public_key() against the hard-coded verification key.
  if (!CheckNewPublicKeyVerificationSignature(fetch_response)) {
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadKeyVerificationSignature;
    return false;
  }

  enterprise_management::PublicKeyVerificationData public_key_data;

  if (!public_key_data.ParseFromString(
          fetch_response.new_public_key_verification_data())) {
    REPORT_LOG(LE, (_T("[ValidateNewPublicKey][Failed to deserialize key]")));
    validation_result->status =
        PolicyValidationResult::Status::kValidationPayloadParseError;
    return false;
  }

  // Also validate new_public_key() against the cached_public_key, if the
  // latter exists.
  if (!cached_public_key.empty()) {
    if (!fetch_response.has_new_public_key_signature() ||
        !VerifySignature(public_key_data.new_public_key(), cached_public_key,
                         fetch_response.new_public_key_signature(),
                         CALG_SHA1)) {
      REPORT_LOG(LE, (_T("[ValidateNewPublicKey]")
                      _T("[Verification against cached public key failed]")));
      validation_result->status = PolicyValidationResult::Status::
          kValidationBadKeyVerificationSignature;
      return false;
    }
  }

  // Now that the new public key has been successfully verified, we rotate to
  // use it for future policy data validation.
  *signature_key = public_key_data.new_public_key();

  return true;
}

bool ValidateDMToken(const enterprise_management::PolicyData& policy_data,
                     const CString& expected_dm_token,
                     PolicyValidationResult* validation_result) {
  if (!policy_data.has_request_token()) {
    REPORT_LOG(LW, (_T("[ValidateDMToken][No DMToken in PolicyData]")));
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadDMToken;
    return false;
  }

  CString received_token(policy_data.request_token().c_str());
  if (expected_dm_token.CompareNoCase(received_token)) {
    REPORT_LOG(LE, (_T("[ValidateDMToken][Unexpected DMToken]")
                    _T("[Expected][%s][Got][%s]"),
                    expected_dm_token, received_token));
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadDMToken;
    return false;
  }

  return true;
}

bool ValidateDeviceId(const enterprise_management::PolicyData& policy_data,
                      const CString& expected_device_id,
                      PolicyValidationResult* validation_result) {
  if (!policy_data.has_device_id()) {
    REPORT_LOG(LW, (_T("[ValidateDeviceId][No Device Id in PolicyData]")));
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadDeviceID;
    return false;
  }

  CString received_id(policy_data.device_id().c_str());
  if (expected_device_id.CompareNoCase(received_id)) {
    REPORT_LOG(LE, (_T("[ValidateDeviceId][Unexpected Device Id]")
                    _T("[Expected][%s][Got][%s]"),
                    expected_device_id, received_id));
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadDeviceID;
    return false;
  }

  return true;
}

bool ValidateTimestamp(const enterprise_management::PolicyData& policy_data,
                       const int64_t cached_timestamp,
                       PolicyValidationResult* validation_result) {
  if (!policy_data.has_timestamp()) {
    REPORT_LOG(LW, (_T("[ValidateTimestamp][No timestamp in PolicyData]")));
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadTimestamp;
    return false;
  }

  if (policy_data.timestamp() < cached_timestamp) {
    REPORT_LOG(LE, (_T("[ValidateTimestamp]")
                    _T("[Unexpected timestamp older than cached timestamp]")));
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadTimestamp;
    return false;
  }

  return true;
}

std::string PolicyTypeFromResponse(
    const enterprise_management::PolicyFetchResponse& response) {
  enterprise_management::PolicyData policy_data;
  if (!policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.IsInitialized() || !policy_data.has_policy_type()) {
    return std::string();
  }

  return policy_data.policy_type();
}

bool ValidatePolicySignature(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    const std::string& signature_key,
    PolicyValidationResult* validation_result) {
  ASSERT1(validation_result);
  if (!fetch_response.has_policy_data_signature()) {
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadSignature;
    return false;
  }

  if (!VerifySignature(fetch_response.policy_data(), signature_key,
                       fetch_response.policy_data_signature(), CALG_SHA1)) {
    validation_result->status =
        PolicyValidationResult::Status::kValidationBadSignature;
    return false;
  }
  return true;
}

enterprise_management::PolicyValidationReportRequest::ValidationResultType
TranslatePolicyValidationResult(PolicyValidationResult::Status status) {
  using Report = enterprise_management::PolicyValidationReportRequest;
  static const std::map<PolicyValidationResult::Status,
                        Report::ValidationResultType>
      kValidationStatusMap = {
          {PolicyValidationResult::Status::kValidationOK,
           Report::VALIDATION_RESULT_TYPE_SUCCESS},
          {PolicyValidationResult::Status::kValidationBadInitialSignature,
           Report::VALIDATION_RESULT_TYPE_BAD_INITIAL_SIGNATURE},
          {PolicyValidationResult::Status::kValidationBadSignature,
           Report::VALIDATION_RESULT_TYPE_BAD_SIGNATURE},
          {PolicyValidationResult::Status::kValidationErrorCodePresent,
           Report::VALIDATION_RESULT_TYPE_ERROR_CODE_PRESENT},
          {PolicyValidationResult::Status::kValidationPayloadParseError,
           Report::VALIDATION_RESULT_TYPE_PAYLOAD_PARSE_ERROR},
          {PolicyValidationResult::Status::kValidationWrongPolicyType,
           Report::VALIDATION_RESULT_TYPE_WRONG_POLICY_TYPE},
          {PolicyValidationResult::Status::kValidationWrongSettingsEntityID,
           Report::VALIDATION_RESULT_TYPE_WRONG_SETTINGS_ENTITY_ID},
          {PolicyValidationResult::Status::kValidationBadTimestamp,
           Report::VALIDATION_RESULT_TYPE_BAD_TIMESTAMP},
          {PolicyValidationResult::Status::kValidationBadDMToken,
           Report::VALIDATION_RESULT_TYPE_BAD_DM_TOKEN},
          {PolicyValidationResult::Status::kValidationBadDeviceID,
           Report::VALIDATION_RESULT_TYPE_BAD_DEVICE_ID},
          {PolicyValidationResult::Status::kValidationBadUser,
           Report::VALIDATION_RESULT_TYPE_BAD_USER},
          {PolicyValidationResult::Status::kValidationPolicyParseError,
           Report::VALIDATION_RESULT_TYPE_POLICY_PARSE_ERROR},
          {PolicyValidationResult::Status::
               kValidationBadKeyVerificationSignature,
           Report::VALIDATION_RESULT_TYPE_BAD_KEY_VERIFICATION_SIGNATURE},
          {PolicyValidationResult::Status::kValidationValueWarning,
           Report::VALIDATION_RESULT_TYPE_VALUE_WARNING},
          {PolicyValidationResult::Status::kValidationValueError,
           Report::VALIDATION_RESULT_TYPE_VALUE_ERROR},
      };

  auto mapped_status = kValidationStatusMap.find(status);
  return mapped_status == kValidationStatusMap.end()
             ? Report::VALIDATION_RESULT_TYPE_ERROR_UNSPECIFIED
             : mapped_status->second;
}

enterprise_management::PolicyValueValidationIssue::ValueValidationIssueSeverity
TranslatePolicyValidationResultSeverity(
    PolicyValueValidationIssue::Severity severity) {
  using Issue = enterprise_management::PolicyValueValidationIssue;
  switch (severity) {
    case PolicyValueValidationIssue::Severity::kWarning:
      return Issue::VALUE_VALIDATION_ISSUE_SEVERITY_WARNING;
    case PolicyValueValidationIssue::Severity::kError:
      return Issue::VALUE_VALIDATION_ISSUE_SEVERITY_ERROR;
    default:
      return Issue::VALUE_VALIDATION_ISSUE_SEVERITY_UNSPECIFIED;
  }
}

bool ExtractOmahaSettingsFromPolicyResponse(
    const enterprise_management::PolicyFetchResponse& response,
    edm::OmahaSettingsClientProto* omaha_settings,
    PolicyValidationResult* validation_result) {
  ASSERT1(omaha_settings);
  ASSERT1(validation_result);

  enterprise_management::PolicyData policy_data;
  if (!policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.has_policy_value()) {
    validation_result->status =
        PolicyValidationResult::Status::kValidationPayloadParseError;
    return false;
  }

  if (!omaha_settings->ParseFromString(policy_data.policy_value())) {
    validation_result->status =
        PolicyValidationResult::Status::kValidationPolicyParseError;
    return false;
  }

  return true;
}

void ValidateAutoUpdateCheckPeriodPolicy(
    const edm::OmahaSettingsClientProto& omaha_settings,
    PolicyValidationResult* validation_result) {
  if (omaha_settings.has_auto_update_check_period_minutes() &&
      (omaha_settings.auto_update_check_period_minutes() < 0 ||
       omaha_settings.auto_update_check_period_minutes() >
           kMaxAutoUpdateCheckPeriodMinutes)) {
    validation_result->issues.emplace_back(
        "auto_update_check_period_minutes",
        PolicyValueValidationIssue::Severity::kError,
        "Value out of range (0 - " +
            std::to_string(kMaxAutoUpdateCheckPeriodMinutes) + "): " +
            std::to_string(omaha_settings.auto_update_check_period_minutes()));
  }
}

void ValidateDownloadPreferencePolicy(
    const edm::OmahaSettingsClientProto& omaha_settings,
    PolicyValidationResult* validation_result) {
  if (!omaha_settings.has_download_preference()) return;

  const CString download_preference(
      omaha_settings.download_preference().c_str());
  if (download_preference.CompareNoCase(kDownloadPreferenceCacheable) != 0) {
    validation_result->issues.emplace_back(
        "download_preference", PolicyValueValidationIssue::Severity::kWarning,
        "Unrecognized download preference: " +
            omaha_settings.download_preference());
  }
}

void ValidateUpdatesSuppressedPolicies(
    const edm::OmahaSettingsClientProto& omaha_settings,
    PolicyValidationResult* validation_result) {
  if (!omaha_settings.has_updates_suppressed()) return;

  if (omaha_settings.updates_suppressed().start_hour() < 0 ||
      omaha_settings.updates_suppressed().start_hour() >= 24) {
    validation_result->issues.emplace_back(
        "updates_suppressed.start_hour",
        PolicyValueValidationIssue::Severity::kError,
        "Value out of range(0 - 23) : " +
            std::to_string(omaha_settings.updates_suppressed().start_hour()));
  }
  if (omaha_settings.updates_suppressed().start_minute() < 0 ||
      omaha_settings.updates_suppressed().start_minute() >= 60) {
    validation_result->issues.emplace_back(
        "updates_suppressed.start_minute",
        PolicyValueValidationIssue::Severity::kError,
        "Value out of range(0 - 59) : " +
            std::to_string(omaha_settings.updates_suppressed().start_minute()));
  }
  if (omaha_settings.updates_suppressed().duration_min() < 0 ||
      omaha_settings.updates_suppressed().duration_min() >
          kMaxUpdatesSuppressedDurationMin) {
    validation_result->issues.emplace_back(
        "updates_suppressed.duration_min",
        PolicyValueValidationIssue::Severity::kError,
        "Value out of range(0 - " +
            std::to_string(kMaxUpdatesSuppressedDurationMin) + ") : " +
            std::to_string(omaha_settings.updates_suppressed().duration_min()));
  }
}

void ValidateProxyPolicies(const edm::OmahaSettingsClientProto& omaha_settings,
                           PolicyValidationResult* validation_result) {
  if (omaha_settings.has_proxy_mode()) {
    const CString proxy_mode(omaha_settings.proxy_mode().c_str());
    if (!ContainsStringNoCase(kProxyModeValidValues, proxy_mode)) {
      validation_result->issues.emplace_back(
          "proxy_mode", PolicyValueValidationIssue::Severity::kError,
          "Unrecognized proxy mode: " + omaha_settings.proxy_mode());
    }
  }

  if (omaha_settings.has_proxy_server()) {
    if (!omaha_settings.has_proxy_mode()) {
      validation_result->issues.emplace_back(
          "proxy_server", PolicyValueValidationIssue::Severity::kWarning,
          "Proxy server setting is ignored because proxy mode is not set.");
    } else {
      const CString proxy_mode(omaha_settings.proxy_mode().c_str());
      if (proxy_mode.CompareNoCase(kProxyModeFixedServers) != 0) {
        validation_result->issues.emplace_back(
            "proxy_server", PolicyValueValidationIssue::Severity::kWarning,
            "Proxy server setting [" + omaha_settings.proxy_server() +
                "] is ignored because proxy mode is not "
                "fixed_servers");
      }
    }
  }

  if (omaha_settings.has_proxy_pac_url()) {
    if (!omaha_settings.has_proxy_mode()) {
      validation_result->issues.emplace_back(
          "proxy_pac_url", PolicyValueValidationIssue::Severity::kWarning,
          "Proxy Pac URL setting is ignored because proxy mode is not "
          "set.");
    } else {
      const CString proxy_mode(omaha_settings.proxy_mode().c_str());
      if (proxy_mode.CompareNoCase(kProxyModePacScript) != 0) {
        validation_result->issues.emplace_back(
            "proxy_pac_url", PolicyValueValidationIssue::Severity::kWarning,
            "Proxy Pac URL setting [" + omaha_settings.proxy_pac_url() +
                "] is ignored because proxy mode is not "
                "pac_script");
      }
    }
  }
}

void ValidateInstallDefaultPolicy(
    const edm::OmahaSettingsClientProto& omaha_settings,
    PolicyValidationResult* validation_result) {
  if (omaha_settings.has_install_default() &&
      !Contains(kInstallPolicyValidValues, omaha_settings.install_default())) {
    validation_result->issues.emplace_back(
        "install_default", PolicyValueValidationIssue::Severity::kError,
        "Invalid install default value: " +
            std::to_string(omaha_settings.install_default()));
  }
}

void ValidateUpdateDefaultPolicy(
    const edm::OmahaSettingsClientProto& omaha_settings,
    PolicyValidationResult* validation_result) {
  if (omaha_settings.has_update_default() &&
      !Contains(kUpdatePolicyValidValues, omaha_settings.update_default())) {
    validation_result->issues.emplace_back(
        "update_default", PolicyValueValidationIssue::Severity::kError,
        "Invalid update default value: " +
            std::to_string(omaha_settings.update_default()));
  }
}

void ValidateGlobalPolicies(
    const edm::OmahaSettingsClientProto& omaha_settings,
    PolicyValidationResult* validation_result) {
  ValidateAutoUpdateCheckPeriodPolicy(omaha_settings, validation_result);
  ValidateDownloadPreferencePolicy(omaha_settings, validation_result);
  ValidateUpdatesSuppressedPolicies(omaha_settings, validation_result);
  ValidateProxyPolicies(omaha_settings, validation_result);
  ValidateInstallDefaultPolicy(omaha_settings, validation_result);
  ValidateUpdateDefaultPolicy(omaha_settings, validation_result);
}

void ValidateAppInstallPolicy(const edm::ApplicationSettings& app_settings,
                              PolicyValidationResult* validation_result) {
  if (app_settings.has_install() &&
      !Contains(kInstallPolicyValidValues, app_settings.install())) {
    validation_result->issues.emplace_back(
        "install", PolicyValueValidationIssue::Severity::kError,
        app_settings.app_guid() + " invalid install policy: " +
            std::to_string(app_settings.install()));
  }
}

void ValidateAppUpdatePolicy(const edm::ApplicationSettings& app_settings,
                             PolicyValidationResult* validation_result) {
  if (app_settings.has_update() &&
      !Contains(kUpdatePolicyValidValues, app_settings.update())) {
    validation_result->issues.emplace_back(
        "update", PolicyValueValidationIssue::Severity::kError,
        app_settings.app_guid() +
            " invalid update policy: " + std::to_string(app_settings.update()));
  }
}

void ValidateAppTargetChannelPolicy(
    const edm::ApplicationSettings& app_settings,
    PolicyValidationResult* validation_result) {
  if (app_settings.has_target_channel() &&
      app_settings.target_channel().empty()) {
    validation_result->issues.emplace_back(
        "target_channel", PolicyValueValidationIssue::Severity::kWarning,
        app_settings.app_guid() + " empty policy value");
  }
}

void ValidateAppTargetVersionPrefixPolicy(
    const edm::ApplicationSettings& app_settings,
    PolicyValidationResult* validation_result) {
  if (app_settings.has_target_version_prefix() &&
      app_settings.target_version_prefix().empty()) {
    validation_result->issues.emplace_back(
        "target_version_prefix", PolicyValueValidationIssue::Severity::kWarning,
        app_settings.app_guid() + " empty policy value");
  }
}

void ValidateAppPolicies(const edm::ApplicationSettings& app_settings,
                         PolicyValidationResult* validation_result) {
  if (!app_settings.has_app_guid()) return;

  GUID app_guid = {};
  CString app_guid_str(app_settings.app_guid().c_str());
  if (FAILED(StringToGuidSafe(app_guid_str, &app_guid))) return;

  ValidateAppInstallPolicy(app_settings, validation_result);
  ValidateAppUpdatePolicy(app_settings, validation_result);
  ValidateAppTargetChannelPolicy(app_settings, validation_result);
  ValidateAppTargetVersionPrefixPolicy(app_settings, validation_result);
}

}  // namespace

bool ValidateOmahaPolicyResponse(
    const enterprise_management::PolicyFetchResponse& response,
    PolicyValidationResult* validation_result) {
  ASSERT1(validation_result);

  edm::OmahaSettingsClientProto omaha_settings;
  if (!ExtractOmahaSettingsFromPolicyResponse(response, &omaha_settings,
                                              validation_result)) {
    REPORT_LOG(LE, (_T("[ExtractOmahaSettingsFromPolicyResponse]: %s"),
                    validation_result->ToString()));
    return false;
  }

  ValidateGlobalPolicies(omaha_settings, validation_result);

  const auto& repeated_app_settings = omaha_settings.application_settings();
  for (const auto& app_settings : repeated_app_settings)
    ValidateAppPolicies(app_settings, validation_result);

  REPORT_LOG(L1, (_T("[ValidateOmahaPolicyResponse]: %s"),
                  validation_result->ToString()));
  return !validation_result->HasErrorIssue();
}

bool ValidatePolicyFetchResponse(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    const CString& expected_dm_token, const CString& expected_device_id,
    const CachedPolicyInfo& info, PolicyValidationResult* validation_result) {
  enterprise_management::PolicyData fetch_policy_data;
  if (!fetch_policy_data.ParseFromString(fetch_response.policy_data())) {
    REPORT_LOG(LW, (_T("[ValidatePolicyFetchResponse][Invalid PolicyData]")));
    validation_result->status =
        PolicyValidationResult::Status::kValidationPolicyParseError;
    return false;
  }

  if (fetch_policy_data.has_policy_token())
    validation_result->policy_token = fetch_policy_data.policy_token();

  if (!ValidateDMToken(fetch_policy_data, expected_dm_token,
                       validation_result) ||
      !ValidateDeviceId(fetch_policy_data, expected_device_id,
                        validation_result) ||
      !ValidateTimestamp(fetch_policy_data, info.timestamp,
                         validation_result)) {
    return false;
  }

  std::string signature_key;
  if (!ValidateNewPublicKey(fetch_response, info.key, &signature_key,
                            validation_result))
    return false;

  validation_result->policy_type = PolicyTypeFromResponse(fetch_response);
  if (validation_result->policy_type.empty()) {
    REPORT_LOG(LW, (_T("[ValidatePolicyFetchResponse][Missing PolicyType]")
                    _T("[%d]"),
                    info.version));
    validation_result->status =
        PolicyValidationResult::Status::kValidationWrongPolicyType;
    return false;
  }

  if (!ValidatePolicySignature(fetch_response, signature_key,
                               validation_result)) {
    REPORT_LOG(LE, (_T("[ValidatePolicySignature]")
                    _T("[Failed to verify the signature for policy type %s]"),
                    validation_result->policy_type.c_str()));
    return false;
  }

  if (validation_result->policy_type == kGoogleUpdatePolicyType &&
      !ValidateOmahaPolicyResponse(fetch_response, validation_result)) {
    return false;
  }
  return true;
}

HRESULT GetCachedPolicyInfo(const std::string& raw_response,
                            CachedPolicyInfo* info) {
  ASSERT1(info);

  *info = {};

  enterprise_management::PolicyFetchResponse response;
  enterprise_management::PolicyData policy_data;
  enterprise_management::PublicKeyVerificationData verification_data;
  if (raw_response.empty() || !response.ParseFromString(raw_response) ||
      !policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.has_timestamp() ||
      !response.has_new_public_key_verification_data() ||
      !verification_data.ParseFromString(
          response.new_public_key_verification_data())) {
    return E_UNEXPECTED;
  }

  info->key = verification_data.new_public_key();
  if (verification_data.has_new_public_key_version()) {
    info->is_version_valid = true;
    info->version = verification_data.new_public_key_version();
  }
  info->timestamp = policy_data.timestamp();

  return S_OK;
}

HRESULT GetCachedOmahaPolicy(const std::string& raw_response,
                             CachedOmahaPolicy* info) {
  ASSERT1(info);

  info->is_managed = false;
  info->is_initialized = false;

  enterprise_management::PolicyFetchResponse response;
  enterprise_management::PolicyData policy_data;
  edm::OmahaSettingsClientProto omaha_settings;
  if (raw_response.empty() || !response.ParseFromString(raw_response) ||
      !policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.has_policy_value() ||
      !omaha_settings.ParseFromString(policy_data.policy_value())) {
    return E_UNEXPECTED;
  }

  info->is_managed = true;
  info->is_initialized = true;

  if (omaha_settings.has_auto_update_check_period_minutes()) {
    info->auto_update_check_period_minutes =
        omaha_settings.auto_update_check_period_minutes();
  }
  if (omaha_settings.has_download_preference()) {
    info->download_preference =
        CString(omaha_settings.download_preference().c_str());
  }
  if (omaha_settings.has_updates_suppressed()) {
    info->updates_suppressed.start_hour =
        omaha_settings.updates_suppressed().start_hour();
    info->updates_suppressed.start_minute =
        omaha_settings.updates_suppressed().start_minute();
    info->updates_suppressed.duration_min =
        omaha_settings.updates_suppressed().duration_min();
  }
  if (omaha_settings.has_proxy_mode()) {
    info->proxy_mode = CString(omaha_settings.proxy_mode().c_str());
  }
  if (omaha_settings.has_proxy_server()) {
    info->proxy_server = CString(omaha_settings.proxy_server().c_str());
  }
  if (omaha_settings.has_proxy_pac_url()) {
    info->proxy_pac_url = CString(omaha_settings.proxy_pac_url().c_str());
  }
  if (omaha_settings.has_install_default()) {
    info->install_default = omaha_settings.install_default();
  }
  if (omaha_settings.has_update_default()) {
    info->update_default = omaha_settings.update_default();
  }

  const auto& repeated_app_settings = omaha_settings.application_settings();

  for (const auto& app_settings_proto : repeated_app_settings) {
    if (!app_settings_proto.has_app_guid()) {
      continue;
    }

    GUID app_guid;
    if (FAILED(StringToGuidSafe(CString(app_settings_proto.app_guid().c_str()),
                                &app_guid))) {
      continue;
    }

    ApplicationSettings app_settings;
    app_settings.install = app_settings_proto.has_install()
                               ? app_settings_proto.install()
                               : info->install_default;
    app_settings.update = app_settings_proto.has_update()
                              ? app_settings_proto.update()
                              : info->update_default;

    if (app_settings_proto.has_target_channel()) {
      app_settings.target_channel =
          CString(app_settings_proto.target_channel().c_str());
    }
    if (app_settings_proto.has_target_version_prefix()) {
      app_settings.target_version_prefix =
          CString(app_settings_proto.target_version_prefix().c_str());
    }
    if (app_settings_proto.has_rollback_to_target_version()) {
      app_settings.rollback_to_target_version =
          !!app_settings_proto.rollback_to_target_version();
    }

    info->application_settings.insert(std::make_pair(app_guid, app_settings));
  }

  return S_OK;
}

CStringA SerializeRegisterBrowserRequest(const CStringA& machine_name,
                                         const CStringA& serial_number,
                                         const CStringA& os_platform,
                                         const CStringA& os_version) {
  enterprise_management::DeviceManagementRequest dm_request;

  ::enterprise_management::RegisterBrowserRequest* request =
      dm_request.mutable_register_browser_request();
  request->set_machine_name(machine_name, machine_name.GetLength());
  request->set_os_platform(os_platform, os_platform.GetLength());
  request->set_os_version(os_version, os_version.GetLength());

  ::enterprise_management::BrowserDeviceIdentifier* device_identifier =
      request->mutable_browser_device_identifier();
  device_identifier->set_computer_name(machine_name);
  device_identifier->set_serial_number(serial_number);

  CStringA result;
  SerializeToCStringA(dm_request, &result);
  return result;
}

CStringA SerializePolicyFetchRequest(const CStringA& machine_name,
                                     const CStringA& serial_number,
                                     const CStringA& policy_type,
                                     const CachedPolicyInfo& info) {
  enterprise_management::DeviceManagementRequest policy_request;

  enterprise_management::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->add_requests();
  policy_fetch_request->set_policy_type(policy_type);
  policy_fetch_request->set_signature_type(
      enterprise_management::PolicyFetchRequest::SHA1_RSA);
  policy_fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);

  if (info.is_version_valid) {
    policy_fetch_request->set_public_key_version(info.version);
  }

  ::enterprise_management::BrowserDeviceIdentifier* device_identifier =
      policy_fetch_request->mutable_browser_device_identifier();
  device_identifier->set_computer_name(machine_name);
  device_identifier->set_serial_number(serial_number);

  CStringA result;
  SerializeToCStringA(policy_request, &result);
  return result;
}

CStringA SerializePolicyValidationReportRequest(
    const PolicyValidationResult& validation_result) {
  PolicyValidationResult::Status aggregated_status = validation_result.status;

  if (aggregated_status == PolicyValidationResult::Status::kValidationOK) {
    for (const PolicyValueValidationIssue& issue : validation_result.issues) {
      if (issue.severity == PolicyValueValidationIssue::Severity::kError) {
        aggregated_status =
            PolicyValidationResult::Status::kValidationValueError;
        break;
      } else if (issue.severity ==
                 PolicyValueValidationIssue::Severity::kWarning) {
        aggregated_status =
            PolicyValidationResult::Status::kValidationValueWarning;
      }
    }
  }

  if (aggregated_status == PolicyValidationResult::Status::kValidationOK) {
    return CStringA();
  }

  enterprise_management::DeviceManagementRequest policy_request;

  enterprise_management::PolicyValidationReportRequest*
      policy_validation_report_request =
          policy_request.mutable_policy_validation_report_request();
  policy_validation_report_request->set_validation_result_type(
      TranslatePolicyValidationResult(aggregated_status));
  policy_validation_report_request->set_policy_type(
      validation_result.policy_type);
  policy_validation_report_request->set_policy_token(
      validation_result.policy_token);

  for (const PolicyValueValidationIssue& issue : validation_result.issues) {
    enterprise_management::PolicyValueValidationIssue*
        policy_value_validation_issue =
            policy_validation_report_request
                ->add_policy_value_validation_issues();
    policy_value_validation_issue->set_policy_name(issue.policy_name);
    policy_value_validation_issue->set_severity(
        TranslatePolicyValidationResultSeverity(issue.severity));
    policy_value_validation_issue->set_debug_message(issue.message);
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

HRESULT ParseDevicePolicyResponse(
    const std::vector<uint8>& dm_response_array, const CachedPolicyInfo& info,
    const CString& dm_token, const CString& device_id,
    PolicyResponses* responses_out,
    std::vector<PolicyValidationResult>* validation_results) {
  ASSERT1(responses_out);
  ASSERT1(validation_results);

  if (dm_response_array.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    return E_FAIL;
  }

  enterprise_management::DeviceManagementResponse dm_response;
  if (!dm_response.ParseFromArray(dm_response_array.data(),
                                  static_cast<int>(dm_response_array.size()))) {
    return E_FAIL;
  }

  if (!dm_response.has_policy_response() ||
      dm_response.policy_response().responses_size() == 0) {
    return E_FAIL;
  }

  PolicyResponsesMap responses;
  bool should_update_cached_info = true;
  for (int i = 0; i < dm_response.policy_response().responses_size(); ++i) {
    PolicyValidationResult validation_result;
    const enterprise_management::PolicyFetchResponse& response =
        dm_response.policy_response().responses(i);
    if (!ValidatePolicyFetchResponse(response, dm_token, device_id, info,
                                     &validation_result)) {
      validation_results->push_back(validation_result);
      continue;
    }

    if (should_update_cached_info &&
        response.has_new_public_key_verification_data()) {
      // Cache this policy response for future validations. At the moment, we
      // only use the public key information within the cached policy response.
      std::string policy_info;
      if (!response.SerializeToString(&policy_info)) {
        return E_UNEXPECTED;
      }

      responses_out->policy_info = std::move(policy_info);
      should_update_cached_info = false;
    }

    std::string policy_type = PolicyTypeFromResponse(response);
    ASSERT1(!policy_type.empty());
    if (responses.find(policy_type) != responses.end()) {
      REPORT_LOG(LW, (_T("Duplicate PolicyFetchResponse for type: %S"),
                      policy_type.c_str()));
      continue;
    }

    std::string policy_fetch_response;
    if (!response.SerializeToString(&policy_fetch_response)) {
      REPORT_LOG(LW, (_T("Failed to serialize response for type: %S"),
                      policy_type.c_str()));
      continue;
    }

    responses[policy_type] = std::move(policy_fetch_response);
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

bool ShouldDeleteDmToken(const std::vector<uint8>& response) {
  enterprise_management::DeviceManagementResponse dm_response;
  if (response.size() > std::numeric_limits<size_t>::max() ||
      !dm_response.ParseFromArray(response.data(),
                                  static_cast<int>(response.size()))) {
    return false;
  }

  return std::find(dm_response.error_detail().begin(),
                   dm_response.error_detail().end(),
                   enterprise_management::
                       CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN) !=
         dm_response.error_detail().end();
}

}  // namespace omaha
