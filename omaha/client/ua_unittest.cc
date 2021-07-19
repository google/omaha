// Copyright 2014 Google Inc.
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
// ========================================================================

#include <windows.h>
#include <atltime.h>

#include <tuple>

#include "omaha/base/constants.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/time.h"
#include "omaha/client/ua.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace goopdate_utils {

class UATest : public testing::TestWithParam<std::tuple<bool, bool> > {
 public:
  UATest() : last_check_period_sec_(0), is_machine_(false), is_domain_(false) {}

 protected:
  int last_check_period_sec_;
  bool is_machine_;
  bool is_domain_;

 private:
  virtual void SetUp() {
    std::tie(is_machine_, is_domain_) = GetParam();

    RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
    RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV, kRegValueLastCheckPeriodSec);
    EXPECT_SUCCEEDED(
        ConfigManager::Instance()->SetLastCheckedTime(is_machine_, 0));

    bool is_overridden(false);
    last_check_period_sec_ =
        ConfigManager::Instance()->GetLastCheckPeriodSec(&is_overridden);

    EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueIsEnrolledToDomain,
                                      is_domain_ ? 1UL : 0UL));
  }

  virtual void TearDown() {
    EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                         kRegValueIsEnrolledToDomain));

    RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
    RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV, kRegValueLastCheckPeriodSec);

    ConfigManager::DeleteInstance();
  }

  DISALLOW_COPY_AND_ASSIGN(UATest);
};

INSTANTIATE_TEST_CASE_P(IsMachineIsDomain,
                        UATest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool()));

TEST_P(UATest, UpdateLastChecked) {
  EXPECT_SUCCEEDED(UpdateLastChecked(is_machine_));
  EXPECT_FALSE(ShouldCheckForUpdates(is_machine_));

  ConfigManager::Instance()->SetLastCheckedTime(is_machine_, 0);
  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_NoLastCheckedPresent) {
  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_LastCheckedPresent) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  ConfigManager::Instance()->SetLastCheckedTime(is_machine_, now - 10);
  EXPECT_FALSE(ShouldCheckForUpdates(is_machine_));

  // Choose a value in the past which is solidly beyond 2 update cycles.
  ConfigManager::Instance()->SetLastCheckedTime(
      is_machine_,
      now - static_cast<DWORD>(2.5 * last_check_period_sec_));

  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_LastCheckedInFuture) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  // The absolute difference is within the check period.
  ConfigManager::Instance()->SetLastCheckedTime(
      is_machine_,
      now + 600);
  EXPECT_FALSE(ShouldCheckForUpdates(is_machine_));

  // The absolute difference is greater than the check period. Choose a value
  // in the future which is solidly beyond 2 update cycles.
  ConfigManager::Instance()->SetLastCheckedTime(
      is_machine_,
      now + static_cast<DWORD>(2.5 * last_check_period_sec_));
  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_PeriodZero) {
  EXPECT_SUCCEEDED(SetPolicy(kRegValueAutoUpdateCheckPeriodOverrideMinutes, 0));

  EXPECT_EQ(!is_domain_, ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_PeriodOverride) {
  const DWORD kOverrideMinutes = 10;
  const DWORD kOverrideSeconds = kOverrideMinutes * 60;
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_SUCCEEDED(SetPolicy(kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                             kOverrideMinutes));

  ConfigManager::Instance()->SetLastCheckedTime(is_machine_, now - 10);
  EXPECT_FALSE(ShouldCheckForUpdates(is_machine_));

  ConfigManager::Instance()->SetLastCheckedTime(is_machine_,
                                                now - kOverrideSeconds - 1);
  EXPECT_EQ(is_domain_, ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_SkipUpdate) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  // Choose a value in the past which falls within the time interval where
  // the client could skip the update check, In this case 30 minutes beyond
  // the last update cycle.
  ConfigManager::Instance()->SetLastCheckedTime(
      is_machine_,
      now - last_check_period_sec_ - 30 * 60);

  // There is a .1 probability that an update check will be skipped in this
  // case. This test has a probability of 1.7e-46 to fail in the worst case
  // scenario.
  // The runtime cost of the test in the worst case is low (around 500 ms on
  // an actual machine).
  bool has_skipped_update(false);
  for (int i = 0; i != 1000; ++i) {
    if (!ShouldCheckForUpdates(is_machine_)) {
      has_skipped_update = true;
      break;
    }
  }

  EXPECT_TRUE(has_skipped_update);

  if (!is_domain_) {
    return;
  }

  // Verify the overriding the update check period is not causing skips.
  const DWORD kOverrideMinutes = last_check_period_sec_ / 60;
  EXPECT_SUCCEEDED(SetPolicy(kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                             kOverrideMinutes));

  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_RetryAfter) {
  ConfigManager::Instance()->SetRetryAfterTime(is_machine_, 0);
  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  ConfigManager::Instance()->SetRetryAfterTime(is_machine_, now - 1000);
  EXPECT_TRUE(ShouldCheckForUpdates(is_machine_));

  ConfigManager::Instance()->SetRetryAfterTime(is_machine_,
                                               now + kSecondsPerHour);
  EXPECT_FALSE(ShouldCheckForUpdates(is_machine_));

  ConfigManager::Instance()->SetRetryAfterTime(is_machine_, 0);
}

TEST_P(UATest, ShouldCheckForUpdates_UpdatesSuppressed) {
  CTime now(CTime::GetCurrentTime());
  tm local = {};
  now.GetLocalTm(&local);

  EXPECT_SUCCEEDED(
      SetPolicy(kRegValueUpdatesSuppressedStartHour, local.tm_hour));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedStartMin, 0));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedDurationMin, 60));

  EXPECT_EQ(!is_domain_, ShouldCheckForUpdates(is_machine_));

  if (local.tm_min) {
    EXPECT_SUCCEEDED(
        SetPolicy(kRegValueUpdatesSuppressedDurationMin, local.tm_min - 1));

    EXPECT_EQ(true, ShouldCheckForUpdates(is_machine_));
  }
}

TEST_P(UATest, ShouldCheckForUpdates_UpdatesSuppressed_InvalidHour) {
  CTime now(CTime::GetCurrentTime());
  tm local = {};
  now.GetLocalTm(&local);

  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedStartHour, 26));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedStartMin, 0));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedDurationMin, 60));

  EXPECT_EQ(true, ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_UpdatesSuppressed_InvalidMin) {
  CTime now(CTime::GetCurrentTime());
  tm local = {};
  now.GetLocalTm(&local);

  EXPECT_SUCCEEDED(
      SetPolicy(kRegValueUpdatesSuppressedStartHour, local.tm_hour));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedStartMin, 456));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedDurationMin, 60));

  EXPECT_EQ(true, ShouldCheckForUpdates(is_machine_));
}

TEST_P(UATest, ShouldCheckForUpdates_UpdatesSuppressed_InvalidDuration) {
  CTime now(CTime::GetCurrentTime());
  tm local = {};
  now.GetLocalTm(&local);

  EXPECT_SUCCEEDED(
      SetPolicy(kRegValueUpdatesSuppressedStartHour, local.tm_hour));
  EXPECT_SUCCEEDED(SetPolicy(kRegValueUpdatesSuppressedStartMin, 0));
  EXPECT_SUCCEEDED(
      SetPolicy(kRegValueUpdatesSuppressedDurationMin, 200 * kMinPerHour));

  EXPECT_EQ(true, ShouldCheckForUpdates(is_machine_));
}

}  // namespace goopdate_utils

}  // namespace omaha
