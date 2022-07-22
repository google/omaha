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

#include "omaha/goopdate/dm_storage_test_utils.h"

#include "omaha/base/constants.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

void SetBinaryValue(const TCHAR* path,
                    const TCHAR* value_name,
                    const CStringA& data) {
  RegKey key;
  ASSERT_HRESULT_SUCCEEDED(key.Create(path, NULL, REG_OPTION_NON_VOLATILE,
                                      KEY_SET_VALUE));
  ASSERT_HRESULT_SUCCEEDED(
      key.SetValue(value_name,
                   reinterpret_cast<const byte*>(data.GetString()),
                   data.GetLength(), REG_BINARY));
}

}  // namespace

void WriteInstallToken(const TCHAR* enrollment_token) {
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(
      app_registry_utils::GetAppClientStateKey(true /* is_machine */,
                                               kGoogleUpdateAppId),
      kRegValueCloudManagementEnrollmentToken,
      enrollment_token));
}

void WriteCompanyPolicyToken(const TCHAR* enrollment_token) {
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyCloudManagementGroupPolicy,
      kRegValueEnrollmentToken,
      enrollment_token));
}

void WriteCompanyDmToken(const char* dm_token) {
  ASSERT_NO_FATAL_FAILURE(SetBinaryValue(kRegKeyCompanyEnrollment,
                                         kRegValueDmToken,
                                         dm_token));
}

#if defined(HAS_LEGACY_DM_CLIENT)

void WriteLegacyPolicyToken(const TCHAR* enrollment_token) {
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyGroupPolicy,
      kRegValueCloudManagementEnrollmentTokenPolicy,
      enrollment_token));
}

void WriteOldLegacyPolicyToken(const TCHAR* enrollment_token) {
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyGroupPolicy,
      kRegValueMachineLevelUserCloudPolicyEnrollmentToken,
      enrollment_token));
}

void WriteLegacyDmToken(const char* dm_token) {
  ASSERT_NO_FATAL_FAILURE(SetBinaryValue(kRegKeyLegacyEnrollment,
                                         kRegValueDmToken,
                                         dm_token));
}

#endif  // defined(HAS_LEGACY_DM_CLIENT)

}  // namespace omaha
