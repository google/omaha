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

#ifndef OMAHA_GOOPDATE_DM_CLIENT_H__
#define OMAHA_GOOPDATE_DM_CLIENT_H__

#include <windows.h>
#include <atlpath.h>
#include <atlstr.h>
#include <utility>
#include <vector>
#include "omaha/goopdate/dm_messages.h"

namespace omaha {

class DmStorage;
class HttpRequestInterface;

namespace dm_client {

// The policy type that supports getting the policies for all Machine
// applications from the DMServer.
const char kGoogleUpdateMachineLevelApps[] = "google/machine-level-apps";

// The content-type for all protocol buffer requests.
const TCHAR kProtobufContentType[] = _T("application/protobuf");

enum RegistrationState {
  // This client appears to not be managed. In particular, neither a device
  // management token nor an enrollment token can be found.
  kNotManaged,

  // This client has an enrollment token available, but is not yet registered
  // for device management (i.e., no device management token can be found).
  kRegistrationPending,

  // This client is registered for cloud management.
  kRegistered,
};

// Returns the registration state for the machine.
RegistrationState GetRegistrationState(DmStorage* dm_storage);

// Returns S_OK if registration takes place and succeeds, S_FALSE if
// registration was not needed (either it has already been done, or no
// enrollment token is found), or a failure HRESULT in case of error.
// If |is_foreground| is false, RegisterIfNeeded applies a wait before running
// the registration, since the DM server is rate-limited. The wait is a random
// value in the range [0, 60000] milisecond.
// (up to one minute).
HRESULT RegisterIfNeeded(DmStorage* dm_storage, bool is_foreground);

// Retrieve and persist locally the policies from the Device Management Server.
HRESULT RefreshPolicies();

namespace internal {

HRESULT RegisterWithRequest(DmStorage* dm_storage,
                            std::unique_ptr<HttpRequestInterface> http_request,
                            const CString& enrollment_token,
                            const CString& device_id);

// Sends policy validation result back to DM Server.
HRESULT SendPolicyValidationResultReportIfNeeded(
    std::unique_ptr<HttpRequestInterface> http_request, const CString& dm_token,
    const CString& device_id, const PolicyValidationResult& validation_result);

// Fetch policies from the DMServer. The policies are returned in |responses|
// containing elements in the following format:
//   {policy_type}=>{SerializeToString-PolicyFetchResponse}.
HRESULT FetchPolicies(DmStorage* dm_storage,
                      std::unique_ptr<HttpRequestInterface> http_request,
                      const CString& dm_token, const CString& device_id,
                      const CachedPolicyInfo& info, PolicyResponses* responses);

HRESULT SendDeviceManagementRequest(
    std::unique_ptr<HttpRequestInterface> http_request, const CStringA& payload,
    const CString& authorization_header, const CString& device_id,
    std::vector<std::pair<CString, CString>> query_params,
    std::vector<uint8>* response);

void HandleDMResponseError(DmStorage* dm_storage, HRESULT hr,
                           const std::vector<uint8>& response);

CString GetAgent();
CString GetPlatform();
CStringA GetOsVersion();
HRESULT AppendQueryParamsToUrl(
    const std::vector<std::pair<CString, CString>>& query_params,
    CString* url);
CString FormatEnrollmentTokenAuthorizationHeader(const CString& token);
CString FormatDMTokenAuthorizationHeader(const CString& token);

}  // namespace internal
}  // namespace dm_client
}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DM_CLIENT_H__
