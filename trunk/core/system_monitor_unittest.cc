// Copyright 2008-2009 Google Inc.
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

#include "omaha/common/constants.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/synchronized.h"
#include "omaha/core/system_monitor.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class SystemMonitorTest
    : public testing::Test,
      public SystemMonitorObserver {
 protected:
  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
    OverrideRegistryHives(kRegistryHiveOverrideRoot);
    gate_.reset(new Gate);
  }

  virtual void TearDown() {
    gate_.reset();
    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }

  // SystemMonitorObserver interface.
  virtual void LastCheckedDeleted() {
    gate_->Open();
  }

  virtual void NoRegisteredClients() {
    gate_->Open();
  }

  void MonitorLastCheckedTest(bool is_machine);
  void MonitorClientsTest(bool is_machine);

  scoped_ptr<Gate> gate_;
};

void SystemMonitorTest::MonitorLastCheckedTest(bool is_machine) {
  const TCHAR* key_name = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  DWORD last_checked_value(1);
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(key_name,
                                            kRegValueLastChecked,
                                            last_checked_value));
  SystemMonitor system_monitor(is_machine);
  system_monitor.set_observer(this);
  ASSERT_HRESULT_SUCCEEDED(system_monitor.Initialize(true));

  // Trigger the callback first time.
  EXPECT_HRESULT_SUCCEEDED(RegKey::DeleteValue(key_name,
                                               kRegValueLastChecked));
  EXPECT_TRUE(gate_->Wait(1000));
  EXPECT_FALSE(RegKey::HasValue(key_name, kRegValueLastChecked));

  // Trigger the callback second time.
  last_checked_value = 2;
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(key_name,
                                            kRegValueLastChecked,
                                            last_checked_value));

  // It takes a while for the registry monitor to detect the changes. There are
  // two changes here: setting and deleting the values.
  ::Sleep(50);
  EXPECT_TRUE(gate_->Close());
  EXPECT_HRESULT_SUCCEEDED(RegKey::DeleteValue(key_name,
                                               kRegValueLastChecked));
  EXPECT_TRUE(gate_->Wait(1000));
  EXPECT_FALSE(RegKey::HasValue(key_name, kRegValueLastChecked));
}

void SystemMonitorTest::MonitorClientsTest(bool is_machine) {
  const TCHAR* key_name = is_machine ? MACHINE_REG_CLIENTS : USER_REG_CLIENTS;
  const TCHAR guid[] = _T("{4AAF2315-B7C8-4633-A1BA-884EFAB755F7}");

  CString app_guid = ConcatenatePath(key_name, guid);
  CString omaha_guid = ConcatenatePath(key_name, kGoogleUpdateAppId);
  const TCHAR* keys_to_create[] = { app_guid, omaha_guid };
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKeys(keys_to_create,
                                              arraysize(keys_to_create)));

  SystemMonitor system_monitor(is_machine);
  system_monitor.set_observer(this);
  EXPECT_HRESULT_SUCCEEDED(system_monitor.Initialize(true));

  EXPECT_HRESULT_SUCCEEDED(RegKey::DeleteKey(keys_to_create[0], true));
  EXPECT_TRUE(gate_->Wait(1000));
}

TEST_F(SystemMonitorTest, SystemMonitor) {
  SystemMonitor system_monitor(false);
  ASSERT_HRESULT_SUCCEEDED(system_monitor.Initialize(false));
  EXPECT_EQ(0, system_monitor.SendMessage(WM_POWERBROADCAST, 0, 0));
  EXPECT_EQ(TRUE, system_monitor.SendMessage(WM_QUERYENDSESSION, 0, 0));
  EXPECT_EQ(0, system_monitor.SendMessage(WM_ENDSESSION, 0, 0));
  EXPECT_EQ(0, system_monitor.SendMessage(WM_WTSSESSION_CHANGE, 0, 0));
}

// Tests the callback gets called when the "LastChecked" is deleted and
// the value is not recreated automatically by the monitor.
TEST_F(SystemMonitorTest, DeleteLastChecked_User) {
  MonitorLastCheckedTest(false);
}

TEST_F(SystemMonitorTest, DeleteLastChecked_Machine) {
  MonitorLastCheckedTest(true);
}

TEST_F(SystemMonitorTest, MonitorClients_User) {
  MonitorClientsTest(false);
}

TEST_F(SystemMonitorTest, MonitorClients_Machine) {
  MonitorClientsTest(true);
}

}  // namespace omaha

