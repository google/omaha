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

#include "omaha/goopdate/dm_storage.h"

#include "omaha/base/const_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/const_group_policy.h"

namespace omaha {

namespace {

// Returns an enrollment token stored in Omaha's ClientState key, or an empty
// string if not present or in case of failure.
CString LoadEnrollmentTokenFromInstall() {
  CString value;
  HRESULT hr = RegKey::GetValue(
      app_registry_utils::GetAppClientStateKey(true /* is_machine */,
                                               kGoogleUpdateAppId),
      kRegValueCloudManagementEnrollmentToken,
      &value);
  return SUCCEEDED(hr) ? value : CString();
}

// Returns an enrollment token provisioned to the computer via Group Policy, or
// an empty string if not set or in case of failure.
CString LoadEnrollmentTokenFromCompanyPolicy() {
  return ConfigManager::Instance()->GetCloudManagementEnrollmentToken();
}

#if defined(HAS_LEGACY_DM_CLIENT)

// Returns an enrollment token provisioned to the computer via Group Policy for
// an install of Google Chrome, or an empty string if not set or in case of
// failure.
CString LoadEnrollmentTokenFromLegacyPolicy() {
  CString value;
  HRESULT hr = RegKey::GetValue(kRegKeyLegacyGroupPolicy,
                                kRegValueCloudManagementEnrollmentTokenPolicy,
                                &value);
  return SUCCEEDED(hr) ? value : CString();
}

// Returns an enrollment token provisioned to the computer via Group Policy for
// an install of Google Chrome in a deprecated location used by old versions of
// Chrome, or an empty string if not set or in case of failure.
CString LoadEnrollmentTokenFromOldLegacyPolicy() {
  CString value;
  HRESULT hr = RegKey::GetValue(
      kRegKeyLegacyGroupPolicy,
      kRegValueMachineLevelUserCloudPolicyEnrollmentToken,
      &value);
  return SUCCEEDED(hr) ? value : CString();
}

#endif  // defined(HAS_LEGACY_DM_CLIENT)

// Returns the device management token found in the registry key |path|, or an
// empty string if not set or in case of failure.
CStringA LoadDmTokenFromKey(const TCHAR* path) {
  ASSERT1(path);
  RegKey key;
  HRESULT hr = key.Open(path, KEY_QUERY_VALUE);
  if (FAILED(hr)) {
    return CStringA();
  }

  byte* value = NULL;
  size_t byte_count = 0;
  DWORD type = REG_NONE;
  hr = key.GetValue(kRegValueDmToken, &value, &byte_count, &type);
  std::unique_ptr<byte[]> safe_value(value);
  if (FAILED(hr) || type != REG_BINARY || byte_count == 0 ||
      byte_count > 4096 /* kMaxDMTokenLength */ ) {
    return CStringA();
  }
  return CStringA(reinterpret_cast<char*>(value), static_cast<int>(byte_count));
}

// Stores |dm_token| in the registry key |path|.
HRESULT StoreDmTokenInKey(const CStringA& dm_token, const TCHAR* path) {
  ASSERT1(path);
  RegKey key;
  HRESULT hr = key.Create(path, NULL /* reg_class */,
                          REG_OPTION_NON_VOLATILE /* options */,
                          KEY_SET_VALUE);
  if (FAILED(hr)) {
    return hr;
  }

  hr = key.SetValue(kRegValueDmToken,
                    reinterpret_cast<const byte*>(dm_token.GetString()),
                    dm_token.GetLength(), REG_BINARY);
  return hr;
}

}  // namespace

DmStorage::DmStorage(const CString& runtime_enrollment_token)
    : runtime_enrollment_token_(runtime_enrollment_token),
      enrollment_token_source_(kETokenSourceNone),
      dm_token_source_(kDmTokenSourceNone) {
}

CString DmStorage::GetEnrollmentToken() {
  if (enrollment_token_source_ == kETokenSourceNone) {
    LoadEnrollmentTokenFromStorage();
  }
  ASSERT1((enrollment_token_source_ == kETokenSourceNone) ==
          enrollment_token_.IsEmpty());
  return enrollment_token_;
}

HRESULT DmStorage::StoreRuntimeEnrollmentTokenForInstall() {
  if (enrollment_token_source_ != kETokenSourceRuntime) {
    return S_FALSE;
  }
  HRESULT hr = RegKey::SetValue(
      app_registry_utils::GetAppClientStateKey(true /* is_machine */,
                                               kGoogleUpdateAppId),
      kRegValueCloudManagementEnrollmentToken,
      enrollment_token_);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[StoreRuntimeEnrollmentTokenForInstall failed][%#x]"),
                 hr));
  }
  return hr;
}

CStringA DmStorage::GetDmToken() {
  if (dm_token_source_ == kDmTokenSourceNone) {
    LoadDmTokenFromStorage();
  }
  ASSERT1((dm_token_source_ == kDmTokenSourceNone) == dm_token_.IsEmpty());
  return dm_token_;
}

HRESULT DmStorage::StoreDmToken(const CStringA& dm_token) {
  HRESULT hr = StoreDmTokenInKey(dm_token, kRegKeyCompanyEnrollment);
  if (SUCCEEDED(hr)) {
    dm_token_source_ = kDmTokenSourceCompany;
#if defined(HAS_LEGACY_DM_CLIENT)
    hr = StoreDmTokenInKey(dm_token, kRegKeyLegacyEnrollment);
#endif
  }
  return hr;
}

CString DmStorage::GetDeviceId() {
  if (device_id_.IsEmpty()) {
    LoadDeviceIdFromStorage();
  }
  return device_id_;
}

void DmStorage::LoadEnrollmentTokenFromStorage() {
  // Load from most to least preferred, stopping when one is found.
  enrollment_token_ = LoadEnrollmentTokenFromCompanyPolicy();
  if (!enrollment_token_.IsEmpty()) {
    enrollment_token_source_ = kETokenSourceCompanyPolicy;
    return;
  }

#if defined(HAS_LEGACY_DM_CLIENT)
  enrollment_token_ = LoadEnrollmentTokenFromLegacyPolicy();
  if (!enrollment_token_.IsEmpty()) {
    enrollment_token_source_ = kETokenSourceLegacyPolicy;
    return;
  }

  enrollment_token_ = LoadEnrollmentTokenFromOldLegacyPolicy();
  if (!enrollment_token_.IsEmpty()) {
    enrollment_token_source_ = kETokenSourceOldLegacyPolicy;
    return;
  }
#endif  // defined(HAS_LEGACY_DM_CLIENT)

  if (!runtime_enrollment_token_.IsEmpty()) {
    enrollment_token_ = runtime_enrollment_token_;
    enrollment_token_source_ = kETokenSourceRuntime;
    return;
  }

  enrollment_token_ = LoadEnrollmentTokenFromInstall();
  if (!enrollment_token_.IsEmpty()) {
    enrollment_token_source_ = kETokenSourceInstall;
  }
}

void DmStorage::LoadDmTokenFromStorage() {
  // Load from most to least preferred, stopping when one is found.
  dm_token_ = LoadDmTokenFromKey(kRegKeyCompanyEnrollment);
  if (!dm_token_.IsEmpty()) {
    dm_token_source_ = kDmTokenSourceCompany;
    return;
  }

#if defined(HAS_LEGACY_DM_CLIENT)
  dm_token_ = LoadDmTokenFromKey(kRegKeyLegacyEnrollment);
  if (!dm_token_.IsEmpty()) {
    dm_token_source_ = kDmTokenSourceLegacy;
  }
#endif  // defined(HAS_LEGACY_DM_CLIENT)
}

void DmStorage::LoadDeviceIdFromStorage() {
  RegKey key;
  HRESULT hr = key.Open(kRegKeyCryptography, KEY_QUERY_VALUE);
  if (SUCCEEDED(hr)) {
    hr = key.GetValue(kRegValueMachineGuid, &device_id_);
  }
  if (FAILED(hr)) {
    device_id_.Empty();
  }
}

}  // namespace omaha
