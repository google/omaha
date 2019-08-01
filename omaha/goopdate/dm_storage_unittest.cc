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

#include "ccc/hosted/policies/services/chrome/omaha_settings.pb.h"
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/dm_messages.h"
#include "omaha/goopdate/dm_storage_test_utils.h"
#include "omaha/testing/unit_test.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"

namespace omaha {

class DmStorageTest : public RegistryProtectedTest {
 protected:
  static const TCHAR kETRuntime[];
  static const TCHAR kETInstall[];
  static const TCHAR kETCompanyPolicy[];
  static const char kDmTCompany[];

#if defined(HAS_LEGACY_DM_CLIENT)
  static const TCHAR kETLegacyPolicy[];
  static const TCHAR kETOldLegacyPolicy[];
  static const char kDmTLegacy[];
#endif  // defined(HAS_LEGACY_DM_CLIENT)

  DmStorage* NewDmStorage(const CString& enrollment_token) {
    return new DmStorage(enrollment_token);
  }

  CPath GetPolicyResponseFilePath(const CPath& policy_responses_dir,
                                  const std::string& policy_type) {
    CStringA encoded_policy_response_dirname;
    Base64Escape(policy_type.c_str(),
                 static_cast<int>(policy_type.length()),
                 &encoded_policy_response_dirname,
                 true);

    CPath policy_response_file(policy_responses_dir);
    policy_response_file.Append(CString(encoded_policy_response_dirname));
    policy_response_file.Append(kPolicyResponseFileName);
    return policy_response_file;
  }

  void CheckFileContentsMatch(const CPath& file_path,
                              const std::string& expected_contents) {
    std::vector<byte> raw_contents;
    ASSERT_HRESULT_SUCCEEDED(ReadEntireFileShareMode(
        file_path, 0, FILE_SHARE_READ, &raw_contents));
    std::string contents(reinterpret_cast<const char*>(&raw_contents[0]),
                         raw_contents.size());

    ASSERT_STREQ(expected_contents.c_str(), contents.c_str());
  }

  std::string CannedOmahaPolicyFetchResponse() {
    ccc_hosted_policies_services_chrome::OmahaSettingsProto omaha_settings;
    google::protobuf_opensource::Map<
        std::string,
        ccc_hosted_policies_services_chrome::OmahaApplicationSettingsProto>*
        app_map =
            omaha_settings.mutable_application_settings()
            ->mutable_application_settings();

    omaha_settings.mutable_auto_update_check_period_minutes()
        ->set_auto_update_check_period_minutes(111);
    omaha_settings.mutable_download_preference()
        ->set_download_preference(CStringA(kDownloadPreferenceCacheable));
    omaha_settings.mutable_updates_suppressed()->set_start_hour(8);
    omaha_settings.mutable_updates_suppressed()->set_start_minute(8);
    omaha_settings.mutable_updates_suppressed()->set_duration_min(47);
    omaha_settings.mutable_proxy_mode()
        ->set_proxy_mode(CStringA(kProxyModePacScript));
    omaha_settings.mutable_proxy_pac_url()->set_proxy_pac_url("foo.c/proxy.pa");
    omaha_settings.mutable_install_default()->set_install_default(
        ccc_hosted_policies_services_chrome
        ::OmahaInstallDefaultProto_InstallDefaultValue_DISABLED);
    omaha_settings.mutable_update_default()->set_update_default(
        ccc_hosted_policies_services_chrome::MANUAL_UPDATES_ONLY);

    ccc_hosted_policies_services_chrome::OmahaApplicationSettingsProto app;
    app.set_app_guid(CStringA(kChromeAppId));

    app.mutable_install()->set_install(
        ccc_hosted_policies_services_chrome
        ::OmahaInstallProto_InstallValue_DISABLED);
    app.mutable_update()->set_update(
        ccc_hosted_policies_services_chrome::AUTOMATIC_UPDATES_ONLY);
    app.mutable_target_version_prefix()->set_target_version_prefix("3.6.55");
    app.mutable_rollback_to_target_version()->set_rollback_to_target_version(
        ccc_hosted_policies_services_chrome
    ::OmahaRollbackToTargetVersionProto_RollbackToTargetVersionValue_ENABLED);

    app_map->insert(
        google::protobuf_opensource::MapPair(std::string("Chrome"), app));

    enterprise_management::PolicyData policy_data;
    policy_data.set_policy_value(omaha_settings.SerializeAsString());

    enterprise_management::PolicyFetchResponse response;
    response.set_policy_data(policy_data.SerializeAsString());

    return response.SerializeAsString();
  }

  void CheckCannedCachedOmahaPolicy(const CachedOmahaPolicy& info) {
    EXPECT_TRUE(info.is_initialized);
    EXPECT_EQ(111, info.auto_update_check_period_minutes);
    EXPECT_STREQ(kDownloadPreferenceCacheable, info.download_preference);
    EXPECT_EQ(8, info.updates_suppressed.start_hour);
    EXPECT_EQ(8, info.updates_suppressed.start_minute);
    EXPECT_EQ(47, info.updates_suppressed.duration_min);
    EXPECT_STREQ(kProxyModePacScript, info.proxy_mode);
    EXPECT_STREQ(_T("foo.c/proxy.pa"), info.proxy_pac_url);
    EXPECT_EQ(kPolicyDisabled, info.install_default);
    EXPECT_EQ(kPolicyManualUpdatesOnly, info.update_default);

    EXPECT_EQ(info.application_settings.size(), 1);
    EXPECT_TRUE(::IsEqualGUID(info.application_settings.begin()->first,
                StringToGuid(kChromeAppId)));
    const ApplicationSettings& app = info.application_settings.begin()->second;
    EXPECT_EQ(kPolicyDisabled, app.install);
    EXPECT_EQ(kPolicyAutomaticUpdatesOnly, app.update);
    EXPECT_STREQ(_T("3.6.55"), app.target_version_prefix);
    EXPECT_TRUE(app.rollback_to_target_version);
  }

  void VerifyPolicies(const CPath& policy_responses_dir,
                      const PolicyResponses& expected_responses) {
    if (!expected_responses.policy_info.empty()) {
      CPath policy_info_file(policy_responses_dir);
      policy_info_file.Append(kCachedPolicyInfoFileName);
      CheckFileContentsMatch(policy_info_file, expected_responses.policy_info);
    }

    for (const auto& expected_response : expected_responses.responses) {
      CPath policy_response_file = GetPolicyResponseFilePath(
          policy_responses_dir, expected_response.first);

      CheckFileContentsMatch(policy_response_file, expected_response.second);

      if (expected_response.first == kGoogleUpdatePolicyType) {
        CachedOmahaPolicy info;
        EXPECT_EQ(S_OK,
            DmStorage::ReadCachedOmahaPolicy(policy_responses_dir, &info));
        CheckCannedCachedOmahaPolicy(info);
      }
    }
  }
};

const TCHAR DmStorageTest::kETRuntime[] = _T("runtime");
const TCHAR DmStorageTest::kETInstall[] = _T("install");
const TCHAR DmStorageTest::kETCompanyPolicy[] = _T("company_policy");
const char DmStorageTest::kDmTCompany[] = "company";
#if defined(HAS_LEGACY_DM_CLIENT)
const TCHAR DmStorageTest::kETLegacyPolicy[] = _T("legacy_policy");
const TCHAR DmStorageTest::kETOldLegacyPolicy[] = _T("old_legacy_policy");
const char DmStorageTest::kDmTLegacy[] = "legacy";
#endif  // defined(HAS_LEGACY_DM_CLIENT)

// Test that empty strings are returned when the registry holds nothing.
TEST_F(DmStorageTest, NoEnrollmentToken) {
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_EQ(dm_storage->GetEnrollmentToken(), CString());
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceNone);
}

// Test the individual sources.
TEST_F(DmStorageTest, EnrollmentTokenFromRuntime) {
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage(kETRuntime));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETRuntime);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceRuntime);
}

TEST_F(DmStorageTest, EnrollmentTokenFromInstall) {
  ASSERT_NO_FATAL_FAILURE(WriteInstallToken(kETInstall));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETInstall);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceInstall);
}

TEST_F(DmStorageTest, EnrollmentTokenFromCompanyPolicy) {
  ASSERT_NO_FATAL_FAILURE(WriteCompanyPolicyToken(kETCompanyPolicy));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETCompanyPolicy);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceCompanyPolicy);
}

#if defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, EnrollmentTokenFromLegacyPolicy) {
  ASSERT_NO_FATAL_FAILURE(WriteLegacyPolicyToken(kETLegacyPolicy));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETLegacyPolicy);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceLegacyPolicy);
}

TEST_F(DmStorageTest, EnrollmentTokenFromOldLegacyPolicy) {
  ASSERT_NO_FATAL_FAILURE(WriteOldLegacyPolicyToken(kETOldLegacyPolicy));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETOldLegacyPolicy);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceOldLegacyPolicy);
}

#endif  // defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, EnrollmentTokenPrecedence) {
  // Add the sources from lowest to highest priority.
  ASSERT_NO_FATAL_FAILURE(WriteInstallToken(kETInstall));
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
    EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETInstall);
    EXPECT_EQ(dm_storage->enrollment_token_source(),
              DmStorage::kETokenSourceInstall);
  }
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage(kETRuntime));
    EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETRuntime);
    EXPECT_EQ(dm_storage->enrollment_token_source(),
              DmStorage::kETokenSourceRuntime);
  }
#if defined(HAS_LEGACY_DM_CLIENT)
  ASSERT_NO_FATAL_FAILURE(WriteOldLegacyPolicyToken(kETOldLegacyPolicy));
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage(kETRuntime));
    EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETOldLegacyPolicy);
    EXPECT_EQ(dm_storage->enrollment_token_source(),
              DmStorage::kETokenSourceOldLegacyPolicy);
  }
  ASSERT_NO_FATAL_FAILURE(WriteLegacyPolicyToken(kETLegacyPolicy));
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage(kETRuntime));
    EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETLegacyPolicy);
    EXPECT_EQ(dm_storage->enrollment_token_source(),
              DmStorage::kETokenSourceLegacyPolicy);
  }
#endif  // defined(HAS_LEGACY_DM_CLIENT)
  ASSERT_NO_FATAL_FAILURE(WriteCompanyPolicyToken(kETCompanyPolicy));
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage(kETRuntime));
    EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETCompanyPolicy);
    EXPECT_EQ(dm_storage->enrollment_token_source(),
              DmStorage::kETokenSourceCompanyPolicy);
  }
}

TEST_F(DmStorageTest, RuntimeEnrollmentTokenForInstall) {
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage(kETRuntime));
    EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETRuntime);
    EXPECT_EQ(dm_storage->StoreRuntimeEnrollmentTokenForInstall(), S_OK);
  }

  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETRuntime);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceInstall);
}

TEST_F(DmStorageTest, PolicyEnrollmentTokenForInstall) {
  {
    ASSERT_NO_FATAL_FAILURE(WriteCompanyPolicyToken(kETCompanyPolicy));
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
    EXPECT_EQ(dm_storage->StoreRuntimeEnrollmentTokenForInstall(), S_FALSE);
  }

  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_STREQ(dm_storage->GetEnrollmentToken(), kETCompanyPolicy);
  EXPECT_EQ(dm_storage->enrollment_token_source(),
            DmStorage::kETokenSourceCompanyPolicy);
}

TEST_F(DmStorageTest, NoDmToken) {
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_EQ(dm_storage->GetDmToken(), CStringA());
}

TEST_F(DmStorageTest, DmTokenFromCompany) {
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(kDmTCompany));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_EQ(dm_storage->GetDmToken(), kDmTCompany);
}

#if defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, DmTokenFromLegacy) {
  ASSERT_NO_FATAL_FAILURE(WriteLegacyDmToken(kDmTLegacy));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_EQ(dm_storage->GetDmToken(), kDmTLegacy);
}

#endif  // defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, DmTokenPrecedence) {
  // Add the sources from lowest to highest priority.
#if defined(HAS_LEGACY_DM_CLIENT)
  ASSERT_NO_FATAL_FAILURE(WriteLegacyDmToken(kDmTLegacy));
  {
    std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
    EXPECT_EQ(dm_storage->GetDmToken(), kDmTLegacy);
  }
#endif  // defined(HAS_LEGACY_DM_CLIENT)

  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(kDmTCompany));
  std::unique_ptr<DmStorage> dm_storage(NewDmStorage((CString())));
  EXPECT_EQ(dm_storage->GetDmToken(), kDmTCompany);
}

TEST_F(DmStorageTest, PersistPolicies) {
  PolicyResponsesMap old_responses = {
    {"google/chrome/machine-level-user", "test-data-chrome"},
    {"google/drive/machine-level-user", "test-data-drive"},
    {"google/earth/machine-level-user", "test-data-earth"},
  };

  const CPath policy_responses_dir = CPath(ConcatenatePath(
      app_util::GetCurrentModuleDirectory(),
      _T("Policies")));

  PolicyResponses expected_old_responses = {old_responses, ""};
  ASSERT_HRESULT_SUCCEEDED(DmStorage::PersistPolicies(policy_responses_dir,
                                                      expected_old_responses));
  VerifyPolicies(policy_responses_dir, expected_old_responses);

  PolicyResponsesMap new_responses = {
    {"google/chrome/machine-level-user", "test-data-chr"},  // Shorter data.
    // {"google/drive/machine-level-user", "test-data-drive"},  // Obsolete.
    {"google/earth/machine-level-user",
     "test-data-earth-foo-bar-baz-foo-bar-baz-foo-bar-baz"},  // Longer data.
    {"google/newdrive/machine-level-user", "test-data-newdrive"},  // New.
    {kGoogleUpdatePolicyType, CannedOmahaPolicyFetchResponse()},  // New.
  };

  PolicyResponses expected_new_responses = {new_responses, "expected data"};
  ASSERT_HRESULT_SUCCEEDED(DmStorage::PersistPolicies(policy_responses_dir,
                                                      expected_new_responses));
  VerifyPolicies(policy_responses_dir, expected_new_responses);
  EXPECT_FALSE(GetPolicyResponseFilePath(
      policy_responses_dir, "google/drive/machine-level-user").FileExists());

  EXPECT_HRESULT_SUCCEEDED(DeleteDirectory(policy_responses_dir));
}

// This test must access the true registry, so it doesn't use the DmStorageTest
// fixture.
TEST(DmStorageDeviceIdTest, GetDeviceId) {
  EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
  ON_SCOPE_EXIT(DmStorage::DeleteInstance);
  EXPECT_FALSE(DmStorage::Instance()->GetDeviceId().IsEmpty());
}

}  // namespace omaha
