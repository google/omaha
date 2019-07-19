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
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
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
};

HRESULT GetCachedPolicyInfo(const std::string& raw_response,
                            CachedPolicyInfo* info);

// Interprets the OmahaSettingsProto within the PolicyData and populates the
// |info| with that information.
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
