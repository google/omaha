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

#ifndef OMAHA_GOOPDATE_DM_MESSAGES_H__
#define OMAHA_GOOPDATE_DM_MESSAGES_H__

#include <atlstr.h>
#include <inttypes.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_group_policy.h"

namespace enterprise_management {
class PolicyFetchResponse;
}  // namespace enterprise_management

namespace omaha {

// The policy type for Omaha policy settings.
constexpr char kGoogleUpdatePolicyType[] = "google/machine-level-omaha";

struct PolicyValueValidationIssue {
  enum class Severity { kWarning, kError };
  std::string policy_name;
  Severity severity = Severity::kWarning;
  std::string message;

  PolicyValueValidationIssue(const std::string& policy_name, Severity severity,
                             const std::string& message)
      : policy_name(policy_name), severity(severity), message(message) {}

  CString ToString() const {
    CString result;
    SafeCStringAppendFormat(&result,
                            _T("[PolicyValueValidationIssue][policy_name][%s]")
                            _T("[severity][%d][message][%s]"),
                            CString(policy_name.c_str()), severity,
                            CString(message.c_str()));
    return result;
  }
};

struct PolicyValidationResult {
  enum class Status {
    // Indicates successful validation.
    kValidationOK,
    // Bad signature on the initial key.
    kValidationBadInitialSignature,
    // Bad signature.
    kValidationBadSignature,
    // Policy blob contains error code.
    kValidationErrorCodePresent,
    // Policy payload failed to decode.
    kValidationPayloadParseError,
    // Unexpected policy type.
    kValidationWrongPolicyType,
    // Unexpected settings entity id.
    kValidationWrongSettingsEntityID,
    // Timestamp is missing or is older than expected.
    kValidationBadTimestamp,
    // DM token is empty or doesn't match.
    kValidationBadDMToken,
    // Device id is empty or doesn't match.
    kValidationBadDeviceID,
    // User id doesn't match.
    kValidationBadUser,
    // Policy payload protobuf parse error.
    kValidationPolicyParseError,
    // Policy key signature could not be verified using the hard-coded
    // verification key.
    kValidationBadKeyVerificationSignature,
    // Policy value validation raised warning(s).
    kValidationValueWarning,
    // Policy value validation failed with error(s).
    kValidationValueError,
  };

  std::string policy_type;
  std::string policy_token;

  Status status = Status::kValidationOK;
  std::vector<PolicyValueValidationIssue> issues;

  bool HasErrorIssue() const {
    return std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
             return issue.severity ==
                    PolicyValueValidationIssue::Severity::kError;
           });
  }

  CString ToString() const {
    CString result;
    SafeCStringAppendFormat(&result,
                            _T("[PolicyValidationResult][status][%d]")
                            _T("[policy_type][%s][policy_token][%s]"),
                            status, CString(policy_type.c_str()),
                            CString(policy_token.c_str()));

    for (const PolicyValueValidationIssue& issue : issues) {
      SafeCStringAppendFormat(&result, _T("\n\t[%s]"), issue.ToString());
    }
    return result;
  }
};

// Maps policy types to their corresponding serialized PolicyFetchResponses.
using PolicyResponsesMap = std::map<std::string, std::string>;
struct PolicyResponses {
  PolicyResponsesMap responses;
  std::string policy_info;
};

struct CachedPolicyInfo {
  std::string key;
  bool is_version_valid = false;
  int32_t version = -1;
  int64_t timestamp = 0;
};

struct UpdatesSuppressed {
  int64_t start_hour = -1;
  int64_t start_minute = -1;
  int64_t duration_min = -1;
};

struct ApplicationSettings {
  int install = -1;
  int update = -1;
  CString target_channel;
  CString target_version_prefix;
  int rollback_to_target_version = -1;

  CString ToString() const {
    CString result(_T("[ApplicationSettings]"));
    SafeCStringAppendFormat(&result, _T("[install][%d]"), install);
    SafeCStringAppendFormat(&result, _T("[update][%d]"), update);
    SafeCStringAppendFormat(&result, _T("[target_channel][%s]"),
                            target_channel);
    SafeCStringAppendFormat(&result, _T("[target_version_prefix][%s]"),
                            target_version_prefix);
    SafeCStringAppendFormat(&result, _T("[rollback_to_target_version][%d]"),
                            rollback_to_target_version);
    return result;
  }
};

struct GUIDCompare {
  bool operator()(const GUID& left, const GUID& right) const {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  }
};

struct CachedOmahaPolicy {
  bool is_managed = false;
  bool is_initialized = false;

  int64_t auto_update_check_period_minutes = -1;
  CString download_preference;
  int64_t cache_size_limit = -1;
  int64_t cache_life_limit = -1;
  UpdatesSuppressed updates_suppressed;
  CString proxy_mode;
  CString proxy_server;
  CString proxy_pac_url;
  int install_default = -1;
  int update_default = -1;

  std::map<GUID, ApplicationSettings, GUIDCompare> application_settings;

  CString ToString() const {
    CString result(_T("[CachedOmahaPolicy]"));
    SafeCStringAppendFormat(&result, _T("[is_initialized][%d]"),
                            is_initialized);
    SafeCStringAppendFormat(&result, _T("[is_managed][%d]"), is_managed);
    SafeCStringAppendFormat(
        &result, _T("[auto_update_check_period_minutes][%" _T(PRId64) "]"),
        auto_update_check_period_minutes);
    SafeCStringAppendFormat(&result, _T("[download_preference][%s]"),
                            download_preference);
    SafeCStringAppendFormat(&result, _T("[cache_size_limit][%" _T(PRId64) "]"),
                            cache_size_limit);
    SafeCStringAppendFormat(&result, _T("[cache_life_limit][%" _T(PRId64) "]"),
                            cache_life_limit);
    SafeCStringAppendFormat(
        &result,
        _T("[updates_suppressed]") _T(
            "[%" _T(PRId64) "][%" _T(PRId64) "][%" _T(PRId64) "]"),
        updates_suppressed.start_hour, updates_suppressed.start_minute,
        updates_suppressed.duration_min);
    SafeCStringAppendFormat(&result, _T("[proxy_mode][%s]"), proxy_mode);
    SafeCStringAppendFormat(&result, _T("[proxy_server][%s]"), proxy_server);
    SafeCStringAppendFormat(&result, _T("[proxy_pac_url][%s]"), proxy_pac_url);
    SafeCStringAppendFormat(&result, _T("[install_default][%d]"),
                            install_default);
    SafeCStringAppendFormat(&result, _T("[update_default][%d]"),
                            update_default);

    for (auto elem : application_settings) {
      SafeCStringAppendFormat(&result, _T("[application_settings][%s][%s]"),
                              GuidToString(elem.first), elem.second.ToString());
    }

    return result;
  }
};

bool ValidateOmahaPolicyResponse(
    const enterprise_management::PolicyFetchResponse& response,
    PolicyValidationResult* validation_result);

HRESULT GetCachedPolicyInfo(const std::string& raw_response,
                            CachedPolicyInfo* info);

// Interprets the OmahaSettingsClientProto within the PolicyData and populates
// the |info| with that information.
HRESULT GetCachedOmahaPolicy(const std::string& raw_response,
                             CachedOmahaPolicy* info);

CStringA SerializeRegisterBrowserRequest(const CStringA& machine_name,
                                         const CStringA& serial_number,
                                         const CStringA& os_platform,
                                         const CStringA& os_version);

CStringA SerializePolicyFetchRequest(const CStringA& machine_name,
                                     const CStringA& serial_number,
                                     const CStringA& policy_type,
                                     const CachedPolicyInfo& info);

CStringA SerializePolicyValidationReportRequest(
    const PolicyValidationResult& validation_result);

HRESULT ParseDeviceRegisterResponse(const std::vector<uint8>& response,
                                    CStringA* dm_token);

// Parses the policies from the DMServer, and return the PolicyFetchResponses in
// |responses|. |responses| contains elements in the following format:
//   {policy_type}=>{SerializeToString-PolicyFetchResponse}.
HRESULT ParseDevicePolicyResponse(
    const std::vector<uint8>& dm_response_array, const CachedPolicyInfo& info,
    const CString& dm_token, const CString& device_id,
    PolicyResponses* responses_out,
    std::vector<PolicyValidationResult>* validation_results);

HRESULT ParseDeviceManagementResponseError(const std::vector<uint8>& response,
                                           CStringA* error_message);

// Determines whether the DMToken is expected to be deleted based on the
// DMServer response contents.
bool ShouldDeleteDmToken(const std::vector<uint8>& response);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DM_MESSAGES_H__
