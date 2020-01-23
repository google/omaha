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
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_group_policy.h"

namespace omaha {

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
  int install = kInstallPolicyDefault;
  int update = kUpdatePolicyDefault;
  CString target_version_prefix;
  bool rollback_to_target_version = false;

  CString ToString() const {
    CString result(_T("[ApplicationSettings]"));
    SafeCStringAppendFormat(&result, _T("[install][%d]"), install);
    SafeCStringAppendFormat(&result, _T("[update][%d]"), update);
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
  bool is_initialized = false;

  int64_t auto_update_check_period_minutes = -1;
  CString download_preference;
  UpdatesSuppressed updates_suppressed;
  CString proxy_mode;
  CString proxy_server;
  CString proxy_pac_url;
  int install_default = kInstallPolicyDefault;
  int update_default = kUpdatePolicyDefault;

  std::map<GUID, ApplicationSettings, GUIDCompare> application_settings;

  CString ToString() const {
    CString result(_T("[CachedOmahaPolicy]"));
    SafeCStringAppendFormat(&result,
        _T("[is_initialized][%d]"), is_initialized);
    SafeCStringAppendFormat(&result,
        _T("[auto_update_check_period_minutes][%" _T(PRId64) "]"),
        auto_update_check_period_minutes);
    SafeCStringAppendFormat(&result,
        _T("[download_preference][%s]"), download_preference);
    SafeCStringAppendFormat(&result, _T("[updates_suppressed]")
        _T("[%" _T(PRId64) "][%" _T(PRId64) "][%" _T(PRId64) "]"),
        updates_suppressed.start_hour,
        updates_suppressed.start_minute,
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
                                       GuidToString(elem.first),
                                       elem.second.ToString());
    }

    return result;
  }
};

HRESULT GetCachedPolicyInfo(const std::string& raw_response,
                            CachedPolicyInfo* info);

// Interprets the OmahaSettingsClientProto within the PolicyData and populates
// the |info| with that information.
HRESULT GetCachedOmahaPolicy(const std::string& raw_response,
                             CachedOmahaPolicy* info);

CStringA SerializeRegisterBrowserRequest(const CStringA& machine_name,
                                         const CStringA& os_platform,
                                         const CStringA& os_version);

CStringA SerializePolicyFetchRequest(const CStringA& policy_type,
                                     const CachedPolicyInfo& info);

HRESULT ParseDeviceRegisterResponse(const std::vector<uint8>& response,
                                    CStringA* dm_token);

// Parses the policies from the DMServer, and return the PolicyFetchResponses in
// |responses|. |responses| contains elements in the following format:
//   {policy_type}=>{SerializeToString-PolicyFetchResponse}.
HRESULT ParseDevicePolicyResponse(const std::vector<uint8>& dm_response_array,
                                  const CachedPolicyInfo& info,
                                  const CString& dm_token,
                                  const CString& device_id,
                                  PolicyResponses* responses_out);

HRESULT ParseDeviceManagementResponseError(const std::vector<uint8>& response,
                                           CStringA* error_message);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DM_MESSAGES_H__
