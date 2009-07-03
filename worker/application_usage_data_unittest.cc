// Copyright 2007-2009 Google Inc.
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
//
// ApplicationUsageData unit tests

#include "omaha/common/reg_key.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/application_usage_data.h"

namespace omaha {

const TCHAR kAppDidRunValueName[] = _T("dr");
const TCHAR kHKCUClientStateKeyName[] =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{6ACB7D4D-E5BA-48b0-85FE-A4051500A1BD}");
const TCHAR kMachineClientState[] =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{6ACB7D4D-E5BA-48b0-85FE-A4051500A1BD}");
const TCHAR kLowIntegrityIEHKCU[] =
    _T("HKCU\\Software\\Microsoft\\Internet Explorer\\")
    _T("InternetRegistry\\REGISTRY\\USER\\");
const TCHAR kAppGuid[] = _T("{6ACB7D4D-E5BA-48b0-85FE-A4051500A1BD}");
const TCHAR kRelativeClientState[] =
    _T("Software\\Google\\Update\\ClientState\\")
    _T("{6ACB7D4D-E5BA-48b0-85FE-A4051500A1BD}");

// TODO(omaha): Expected and actual are reversed throughout this file. Fix.

class ApplicationUsageDataTest : public testing::Test {
 protected:
  virtual void SetUp() {
    CString sid;
    ASSERT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &sid));
    low_integrity_key_name_ = AppendRegKeyPath(kLowIntegrityIEHKCU,
                                               sid,
                                               kRelativeClientState);
    TearDown();
  }

  virtual void TearDown() {
    RegKey::DeleteKey(kHKCUClientStateKeyName);
    RegKey::DeleteKey(kMachineClientState);
    RegKey::DeleteKey(low_integrity_key_name_);
  }

  void CreateMachineDidRunValue(bool value) {
    if (!vista_util::IsUserAdmin()) {
      return;
    }
    RegKey key;
    ASSERT_SUCCEEDED(key.Create(kMachineClientState));
    ASSERT_SUCCEEDED(key.SetValue(kAppDidRunValueName,
                                  value == true ? _T("1") : _T("0")));
  }

  bool MachineDidRunValueExists() {
    if (!vista_util::IsUserAdmin()) {
      return true;
    }
    RegKey key;
    if (FAILED(key.Open(kMachineClientState))) {
      return false;
    }

    CString did_run_str(_T("0"));
    if (FAILED(key.GetValue(kAppDidRunValueName, &did_run_str))) {
      return false;
    }

    return true;
  }

  void DeleteMachineDidRunValue() {
    if (!vista_util::IsUserAdmin()) {
      return;
    }
    ASSERT_SUCCEEDED(RegKey::DeleteValue(kMachineClientState,
                                         kAppDidRunValueName));
  }

  void CheckMachineDidRunValue(bool expected) {
    if (!vista_util::IsUserAdmin()) {
      return;
    }
    RegKey key;
    ASSERT_SUCCEEDED(key.Open(kMachineClientState));

    CString did_run_str(_T("0"));
    ASSERT_SUCCEEDED(key.GetValue(kAppDidRunValueName, &did_run_str));
    bool value = (did_run_str == _T("1")) ? true : false;

    ASSERT_EQ(value, expected);
  }

  void CreateUserDidRunValue(bool value) {
    RegKey key;
    ASSERT_SUCCEEDED(key.Create(kHKCUClientStateKeyName));
    ASSERT_SUCCEEDED(key.SetValue(kAppDidRunValueName,
                                  (value == true) ? _T("1") : _T("0")));
  }

  void DeleteUserDidRunValue() {
    ASSERT_SUCCEEDED(RegKey::DeleteValue(kHKCUClientStateKeyName,
                                         kAppDidRunValueName));
  }

  void CheckUserDidRunValue(bool expected) {
    RegKey key;
    ASSERT_SUCCEEDED(key.Open(kHKCUClientStateKeyName));

    CString did_run_str(_T("0"));
    ASSERT_SUCCEEDED(key.GetValue(kAppDidRunValueName, &did_run_str));
    bool value = (did_run_str == _T("1")) ? true : false;

    ASSERT_EQ(value, expected);
  }

  bool UserDidRunValueExists() {
    RegKey key;
    if (FAILED(key.Open(kHKCUClientStateKeyName))) {
      return false;
    }

    CString did_run_str(_T("0"));
    if (FAILED(key.GetValue(kAppDidRunValueName, &did_run_str))) {
      return false;
    }

    return true;
  }

  void CreateLowIntegrityUserDidRunValue(bool value) {
    RegKey key;
    ASSERT_SUCCEEDED(key.Create(low_integrity_key_name_));
    ASSERT_SUCCEEDED(key.SetValue(kAppDidRunValueName,
                                  (value == true) ? _T("1") : _T("0")));
  }

  void DeleteLowIntegrityUserDidRunValue() {
    ASSERT_SUCCEEDED(RegKey::DeleteValue(low_integrity_key_name_,
                                         kAppDidRunValueName));
  }

  void CheckLowIntegrityUserDidRunValue(bool expected) {
    RegKey key;
    ASSERT_SUCCEEDED(key.Open(low_integrity_key_name_));

    CString did_run_str(_T("0"));
    ASSERT_SUCCEEDED(key.GetValue(kAppDidRunValueName, &did_run_str));
    bool value = (did_run_str == _T("1")) ? true : false;

    ASSERT_EQ(value, expected);
  }

  bool LowIntegrityUserDidRunValueExists() {
    RegKey key;
    if (FAILED(key.Open(low_integrity_key_name_))) {
      return false;
    }

    CString did_run_str(_T("0"));
    if (FAILED(key.GetValue(kAppDidRunValueName, &did_run_str))) {
      return false;
    }

    return true;
  }

  // This method takes in machine_did_run, user_did_run and
  // low_user_did_run as int's. The idea is that the test tries to simulate
  // all of these values as being not-present, and if present then true or
  // false.
  // -1 indicates non-presense, 1 indicates true, and 0 false. The caller
  // then loops over all these values to capture testing all the permutations.
  void TestUserAndMachineDidRun(int machine_did_run,
                                int user_did_run,
                                int low_user_did_run,
                                bool expected_exists,
                                bool expected_did_run,
                                int is_vista) {
    ApplicationUsageData data(true, is_vista ? true : false);

    // Set up the registry for the test.
    if (machine_did_run != -1) {
      CreateMachineDidRunValue((machine_did_run == 1) ? true: false);
    }

    if (user_did_run != -1) {
      CreateUserDidRunValue((user_did_run == 1) ? true: false);
    }

    if (low_user_did_run != -1) {
      CreateLowIntegrityUserDidRunValue((low_user_did_run == 1) ? true: false);
    }

    // Perform the test.
    ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
    ASSERT_EQ(data.exists(), expected_exists);
    ASSERT_EQ(data.did_run(), expected_did_run);

    // Check the return values.
    if (machine_did_run == -1) {
      ASSERT_FALSE(MachineDidRunValueExists());
    } else {
      CheckMachineDidRunValue((machine_did_run == 1) ? true: false);
    }

    if (user_did_run == -1) {
      ASSERT_FALSE(UserDidRunValueExists());
    } else {
      CheckUserDidRunValue((user_did_run == 1) ? true: false);
    }

    if (low_user_did_run == -1) {
      ASSERT_FALSE(LowIntegrityUserDidRunValueExists());
    } else {
      CheckLowIntegrityUserDidRunValue((low_user_did_run == 1) ? true: false);
    }
  }

  void TestUserAndMachineDidRunPostProcess(int machine_did_run,
                                           int user_did_run,
                                           int low_user_did_run,
                                           bool expected_exists,
                                           int is_vista) {
    ApplicationUsageData data(true, is_vista ? true : false);

    // Setup the registry for the test.
    if (machine_did_run != -1) {
      CreateMachineDidRunValue((machine_did_run == 1) ? true: false);
    }

    if (user_did_run != -1) {
      CreateUserDidRunValue((user_did_run == 1) ? true: false);
    }

    if (low_user_did_run != -1) {
      CreateLowIntegrityUserDidRunValue((low_user_did_run == 1) ? true: false);
    }

    // Run the test.
    ASSERT_SUCCEEDED(data.ResetDidRun(kAppGuid));
    if (user_did_run == -1) {
      ASSERT_FALSE(UserDidRunValueExists());
    } else {
      CheckUserDidRunValue(false);
    }

    if (low_user_did_run == -1) {
      ASSERT_FALSE(LowIntegrityUserDidRunValueExists());
    } else {
      if (is_vista) {
        CheckLowIntegrityUserDidRunValue(false);
      } else {
        CheckLowIntegrityUserDidRunValue((low_user_did_run == 1) ? true: false);
      }
    }

    if (machine_did_run == -1) {
      ASSERT_FALSE(MachineDidRunValueExists());
    } else {
      if (user_did_run != -1 ||  (is_vista && low_user_did_run != -1)) {
        // This means that the user keys exists for this application
        // we should have delete the machine key.
        ASSERT_EQ(MachineDidRunValueExists(), false);
      } else {
        CheckMachineDidRunValue(false);
      }
    }

    ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
    ASSERT_EQ(data.exists(), expected_exists);
    ASSERT_EQ(data.did_run(), false);
  }

  void UserTestDidRunPreProcess(int user_did_run,
                                int low_user_did_run,
                                int is_vista,
                                bool expected_exists,
                                bool expected_did_run) {
    ApplicationUsageData data(false, is_vista ? true : false);

    // Set up the registry for the test.
    CreateMachineDidRunValue(true);

    if (user_did_run != -1) {
      CreateUserDidRunValue((user_did_run == 1) ? true: false);
    }

    if (low_user_did_run != -1) {
      CreateLowIntegrityUserDidRunValue((low_user_did_run == 1) ? true: false);
    }

    // Perform the test.
    ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
    ASSERT_EQ(data.exists(), expected_exists);
    ASSERT_EQ(data.did_run(), expected_did_run);

    // The machine value should not have changed from what we set it to.
    CheckMachineDidRunValue(true);
    if (user_did_run == -1) {
      // If we did not create the user value it should not exist.
      ASSERT_FALSE(UserDidRunValueExists());
    }

    if (low_user_did_run == -1) {
      // If we did not create the low integrity user value it should not exist.
      ASSERT_FALSE(LowIntegrityUserDidRunValueExists());
    }
  }

  void UserTestDidRunPostProcess(int user_did_run,
                                 int low_user_did_run,
                                 int is_vista) {
    // Create a user ApplicationUsageData class.
    ApplicationUsageData data(false, is_vista ? true : false);

    // This should not affect the test.
    CreateMachineDidRunValue(true);

    if (user_did_run != -1) {
      CreateUserDidRunValue((user_did_run == 1) ? true: false);
    }

    if (low_user_did_run != -1) {
      CreateLowIntegrityUserDidRunValue((low_user_did_run == 1) ? true: false);
    }

    ASSERT_SUCCEEDED(data.ResetDidRun(kAppGuid));

    // The machine did run shold never get affected.
    CheckMachineDidRunValue(true);
    if (user_did_run == -1) {
      ASSERT_FALSE(UserDidRunValueExists());
    } else {
      // In all cases if the HKCU did run is set, it should get cleared.
      CheckUserDidRunValue(false);
    }

    if (low_user_did_run == -1) {
      ASSERT_FALSE(LowIntegrityUserDidRunValueExists());
    } else {
      // In case of vista, the low integrity user value should get reset.
      CheckLowIntegrityUserDidRunValue(is_vista ? false :
                                       (low_user_did_run == 1) ? true : false);
    }
  }

 private:
  CString low_integrity_key_name_;
};

TEST_F(ApplicationUsageDataTest, ReadDidRunUser1) {
  ApplicationUsageData data(true, false);

  ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
  ASSERT_EQ(data.exists(), false);
  ASSERT_EQ(data.did_run(), false);

  // Test with false user value.
  CreateUserDidRunValue(false);
  ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
  ASSERT_EQ(data.exists(), true);
  ASSERT_EQ(data.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunUser2) {
  // Test with true user value.
  ApplicationUsageData data1(true, false);
  CreateUserDidRunValue(true);
  ASSERT_SUCCEEDED(data1.ReadDidRun(kAppGuid));
  ASSERT_EQ(data1.exists(), true);
  ASSERT_EQ(data1.did_run(), true);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunUser3) {
  // low integrity user = false, vista
  ApplicationUsageData data2(true, true);
  CreateLowIntegrityUserDidRunValue(false);
  ASSERT_SUCCEEDED(data2.ReadDidRun(kAppGuid));
  ASSERT_EQ(data2.exists(), true);
  ASSERT_EQ(data2.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunUser4) {
  // low integrity user = true, vista
  ApplicationUsageData data2(true, true);
  CreateLowIntegrityUserDidRunValue(true);
  ASSERT_SUCCEEDED(data2.ReadDidRun(kAppGuid));
  ASSERT_EQ(data2.exists(), true);
  ASSERT_EQ(data2.did_run(), true);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunUser5) {
  // low integrity user = true, not vista
  ApplicationUsageData data2(true, false);
  CreateLowIntegrityUserDidRunValue(true);
  ASSERT_SUCCEEDED(data2.ReadDidRun(kAppGuid));
  ASSERT_EQ(data2.exists(), false);
  ASSERT_EQ(data2.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunMachine1) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  ApplicationUsageData data(true, true);

  // create machine application key and test
  CreateMachineDidRunValue(false);
  ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
  ASSERT_EQ(data.exists(), true);
  ASSERT_EQ(data.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunMachine2) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  ApplicationUsageData data1(true, true);
  CreateMachineDidRunValue(true);
  ASSERT_SUCCEEDED(data1.ReadDidRun(kAppGuid));
  ASSERT_EQ(data1.exists(), true);
  ASSERT_EQ(data1.did_run(), true);
}

TEST_F(ApplicationUsageDataTest, ReadDidRunBoth1) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  // We try all combinations of machine, user and low integrity user
  // registry value for did run. -1 indicates the value does not exist
  // 1 indicates true and 0 indicates false.
  for (int vista = 0; vista < 2; ++vista) {
    for (int machine = -1; machine < 2; ++machine) {
      for (int user = -1; user < 2; ++user) {
        for (int lowuser = -1; lowuser < 2; ++lowuser) {
          bool expected_did_run = false;
          bool expected_exists = false;

          if (machine > -1 || user > -1 || (vista && lowuser > -1)) {
            expected_exists = true;
          }

          if (machine > 0 || user > 0 || (vista && lowuser > 0)) {
            expected_did_run = true;
          }

          TestUserAndMachineDidRun(machine, user, lowuser,
                                   expected_exists,
                                   expected_did_run,
                                   vista);
          TearDown();
        }
      }
    }
  }
}

TEST_F(ApplicationUsageDataTest, ResetDidRunUser1) {
  ApplicationUsageData data(true, true);

  // create user application key and test
  CreateUserDidRunValue(false);
  ASSERT_SUCCEEDED(data.ResetDidRun(kAppGuid));
  CheckUserDidRunValue(false);

  ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
  ASSERT_EQ(data.exists(), true);
  ASSERT_EQ(data.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ResetDidRunUser2) {
  ApplicationUsageData data1(true, true);
  CreateUserDidRunValue(true);
  ASSERT_SUCCEEDED(data1.ResetDidRun(kAppGuid));
  CheckUserDidRunValue(false);

  ASSERT_SUCCEEDED(data1.ReadDidRun(kAppGuid));
  ASSERT_EQ(data1.exists(), true);
  ASSERT_EQ(data1.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ResetDidRunMachine1) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  ApplicationUsageData data(true, true);
  CreateMachineDidRunValue(false);
  ASSERT_SUCCEEDED(data.ResetDidRun(kAppGuid));
  CheckMachineDidRunValue(false);

  ASSERT_SUCCEEDED(data.ReadDidRun(kAppGuid));
  ASSERT_EQ(data.exists(), true);
  ASSERT_EQ(data.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ResetDidRunMachine2) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  ApplicationUsageData data1(true, true);
  CreateMachineDidRunValue(true);
  ASSERT_SUCCEEDED(data1.ResetDidRun(kAppGuid));
  CheckMachineDidRunValue(false);

  ASSERT_SUCCEEDED(data1.ReadDidRun(kAppGuid));
  ASSERT_EQ(data1.exists(), true);
  ASSERT_EQ(data1.did_run(), false);
}

TEST_F(ApplicationUsageDataTest, ResetDidRunBoth) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  // We try all combinations of machine, user and low integrity user
  // registry value for did run. -1 indicates the value does not exist
  // 1 indicates true and 0 indicates false.
  for (int vista = 0; vista < 2; ++vista) {
    for (int machine = -1; machine < 2; ++machine) {
      for (int user = -1; user < 2; ++user) {
        for (int lowuser = -1; lowuser < 2; ++lowuser) {
          bool expected_exists = false;
          if (machine > -1 || user > -1 || (vista && lowuser > -1)) {
            expected_exists = true;
          }

          TestUserAndMachineDidRunPostProcess(machine, user, lowuser,
                                              expected_exists,
                                              vista);
          TearDown();
        }
      }
    }
  }
}

TEST_F(ApplicationUsageDataTest, UserReadDidRunUser) {
  for (int vista = 0; vista < 2; ++vista) {
      for (int user = -1; user < 2; ++user) {
        for (int lowuser = -1; lowuser < 2; ++lowuser) {
          bool expected_exists = false;
          bool expected_did_run = false;

          if (user != -1 || (vista && lowuser != -1)) {
            expected_exists = true;
          }

          if (user > 0 || (vista && lowuser > 0)) {
            expected_did_run = true;
          }

          UserTestDidRunPreProcess(user, lowuser, vista, expected_exists,
                                   expected_did_run);
          TearDown();
        }
      }
  }
}

TEST_F(ApplicationUsageDataTest, UserResetDidRunUser1) {
  for (int vista = 0; vista < 2; ++vista) {
    for (int user = -1; user < 2; ++user) {
      for (int lowuser = -1; lowuser < 2; ++lowuser) {
        UserTestDidRunPostProcess(user, lowuser, vista);
        TearDown();
      }
    }
  }
}

}  // namespace omaha
