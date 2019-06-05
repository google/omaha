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

#include "omaha/goopdate/dm_storage_test_utils.h"
#include "omaha/testing/unit_test.h"

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
  DmStorage dm_storage((CString()));
  EXPECT_EQ(dm_storage.GetEnrollmentToken(), CString());
  EXPECT_EQ(dm_storage.enrollment_token_source(), DmStorage::kETokenSourceNone);
}

// Test the individual sources.
TEST_F(DmStorageTest, EnrollmentTokenFromRuntime) {
  DmStorage dm_storage(kETRuntime);
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETRuntime);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceRuntime);
}

TEST_F(DmStorageTest, EnrollmentTokenFromInstall) {
  ASSERT_NO_FATAL_FAILURE(WriteInstallToken(kETInstall));
  DmStorage dm_storage((CString()));
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETInstall);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceInstall);
}

TEST_F(DmStorageTest, EnrollmentTokenFromCompanyPolicy) {
  ASSERT_NO_FATAL_FAILURE(WriteCompanyPolicyToken(kETCompanyPolicy));
  DmStorage dm_storage((CString()));
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETCompanyPolicy);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceCompanyPolicy);
}

#if defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, EnrollmentTokenFromLegacyPolicy) {
  ASSERT_NO_FATAL_FAILURE(WriteLegacyPolicyToken(kETLegacyPolicy));
  DmStorage dm_storage((CString()));
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETLegacyPolicy);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceLegacyPolicy);
}

TEST_F(DmStorageTest, EnrollmentTokenFromOldLegacyPolicy) {
  ASSERT_NO_FATAL_FAILURE(WriteOldLegacyPolicyToken(kETOldLegacyPolicy));
  DmStorage dm_storage((CString()));
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETOldLegacyPolicy);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceOldLegacyPolicy);
}

#endif  // defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, EnrollmentTokenPrecedence) {
  // Add the sources from lowest to highest priority.
  ASSERT_NO_FATAL_FAILURE(WriteInstallToken(kETInstall));
  {
    DmStorage dm_storage((CString()));
    EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETInstall);
    EXPECT_EQ(dm_storage.enrollment_token_source(),
              DmStorage::kETokenSourceInstall);
  }
  {
    DmStorage dm_storage(kETRuntime);
    EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETRuntime);
    EXPECT_EQ(dm_storage.enrollment_token_source(),
              DmStorage::kETokenSourceRuntime);
  }
#if defined(HAS_LEGACY_DM_CLIENT)
  ASSERT_NO_FATAL_FAILURE(WriteOldLegacyPolicyToken(kETOldLegacyPolicy));
  {
    DmStorage dm_storage(kETRuntime);
    EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETOldLegacyPolicy);
    EXPECT_EQ(dm_storage.enrollment_token_source(),
              DmStorage::kETokenSourceOldLegacyPolicy);
  }
  ASSERT_NO_FATAL_FAILURE(WriteLegacyPolicyToken(kETLegacyPolicy));
  {
    DmStorage dm_storage(kETRuntime);
    EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETLegacyPolicy);
    EXPECT_EQ(dm_storage.enrollment_token_source(),
              DmStorage::kETokenSourceLegacyPolicy);
  }
#endif  // defined(HAS_LEGACY_DM_CLIENT)
  ASSERT_NO_FATAL_FAILURE(WriteCompanyPolicyToken(kETCompanyPolicy));
  {
    DmStorage dm_storage(kETRuntime);
    EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETCompanyPolicy);
    EXPECT_EQ(dm_storage.enrollment_token_source(),
              DmStorage::kETokenSourceCompanyPolicy);
  }
}

TEST_F(DmStorageTest, RuntimeEnrollmentTokenForInstall) {
  {
    DmStorage dm_storage(kETRuntime);
    EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETRuntime);
    EXPECT_EQ(dm_storage.StoreRuntimeEnrollmentTokenForInstall(), S_OK);
  }

  DmStorage dm_storage((CString()));
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETRuntime);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceInstall);
}

TEST_F(DmStorageTest, PolicyEnrollmentTokenForInstall) {
  {
    ASSERT_NO_FATAL_FAILURE(WriteCompanyPolicyToken(kETCompanyPolicy));
    DmStorage dm_storage((CString()));
    EXPECT_EQ(dm_storage.StoreRuntimeEnrollmentTokenForInstall(), S_FALSE);
  }

  DmStorage dm_storage((CString()));
  EXPECT_STREQ(dm_storage.GetEnrollmentToken(), kETCompanyPolicy);
  EXPECT_EQ(dm_storage.enrollment_token_source(),
            DmStorage::kETokenSourceCompanyPolicy);
}

TEST_F(DmStorageTest, NoDmToken) {
  DmStorage dm_storage((CString()));
  EXPECT_EQ(dm_storage.GetDmToken(), CStringA());
}

TEST_F(DmStorageTest, DmTokenFromCompany) {
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(kDmTCompany));
  DmStorage dm_storage((CString()));
  EXPECT_EQ(dm_storage.GetDmToken(), kDmTCompany);
}

#if defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, DmTokenFromLegacy) {
  ASSERT_NO_FATAL_FAILURE(WriteLegacyDmToken(kDmTLegacy));
  DmStorage dm_storage((CString()));
  EXPECT_EQ(dm_storage.GetDmToken(), kDmTLegacy);
}

#endif  // defined(HAS_LEGACY_DM_CLIENT)

TEST_F(DmStorageTest, DmTokenPrecedence) {
  // Add the sources from lowest to highest priority.
#if defined(HAS_LEGACY_DM_CLIENT)
  ASSERT_NO_FATAL_FAILURE(WriteLegacyDmToken(kDmTLegacy));
  {
    DmStorage dm_storage((CString()));
    EXPECT_EQ(dm_storage.GetDmToken(), kDmTLegacy);
  }
#endif  // defined(HAS_LEGACY_DM_CLIENT)

  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(kDmTCompany));
  DmStorage dm_storage((CString()));
  EXPECT_EQ(dm_storage.GetDmToken(), kDmTCompany);
}

// This test must access the true registry, so it doesn't use the DmStorageTest
// fixture.
TEST(DmStorageDeviceIdTest, GetDeviceId) {
  DmStorage dm_storage((CString()));
  EXPECT_FALSE(dm_storage.GetDeviceId().IsEmpty());
}

}  // namespace omaha
