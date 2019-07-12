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
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"

namespace omaha {

// Maps policy types to their corresponding serialized PolicyFetchResponses.
using PolicyResponsesMap = std::map<std::string, std::string>;
struct PolicyResponses {
  PolicyResponsesMap responses;
  bool has_new_public_key = false;
};

struct CachedPublicKey {
  std::string key;
  bool is_version_valid = false;
  int32_t version = -1;
};

HRESULT GetCachedPublicKeyFromResponse(const std::string& string_response,
                                       CachedPublicKey* key);

CStringA SerializeRegisterBrowserRequest(const CStringA& machine_name,
                                         const CStringA& os_platform,
                                         const CStringA& os_version);

CStringA SerializePolicyFetchRequest(const CStringA& policy_type,
                                     const CachedPublicKey& key);

HRESULT ParseDeviceRegisterResponse(const std::vector<uint8>& response,
                                    CStringA* dm_token);

// Parses the policies from the DMServer, and return the PolicyFetchResponses in
// |responses|. |responses| contains elements in the following format:
//   {policy_type}=>{SerializeToString-PolicyFetchResponse}.
HRESULT ParseDevicePolicyResponse(const std::vector<uint8>& dm_response_array,
                                  const CachedPublicKey& key,
                                  PolicyResponses* responses_out);

HRESULT ParseDeviceManagementResponseError(const std::vector<uint8>& response,
                                           CStringA* error_message);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DM_MESSAGES_H__
