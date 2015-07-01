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

#include <windows.h>
#include <limits.h>
#include "base/basictypes.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/reactor.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/registry_monitor_manager.h"
#include "omaha/base/thread.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR kKeyNameFull[] = _T("HKCU\\key");
const TCHAR kKeyName[]     = _T("key");
const TCHAR kValueName[]   = _T("value");

}  // namespace

class RegistryMonitorTest : public testing::Test {
 protected:
  RegistryMonitorTest() {}

  virtual void SetUp() {
    // Override HKCU.
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
    OverrideSpecifiedRegistryHives(kRegistryHiveOverrideRoot, false, true);
    reset(registry_changed_event_, ::CreateEvent(NULL, true, false, NULL));
  }

  virtual void TearDown() {
    reset(registry_changed_event_);
    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }

  static void RegistryDeleteCallback(const TCHAR* key_name,
                                     const TCHAR* value_name,
                                     RegistryChangeType change_type,
                                     const void* new_value_data,
                                     void* user_data) {
    EXPECT_STREQ(kKeyName, key_name);
    EXPECT_STREQ(kValueName, value_name);
    EXPECT_EQ(REGISTRY_CHANGE_TYPE_DELETE, change_type);
    EXPECT_TRUE(new_value_data);
    EXPECT_TRUE(user_data);
    RegistryMonitorTest* object = static_cast<RegistryMonitorTest*>(user_data);
    DWORD actual_value = reinterpret_cast<DWORD>(new_value_data);
    EXPECT_EQ(ULONG_MAX, actual_value);
    EXPECT_TRUE(::SetEvent(get(object->registry_changed_event_)));
  }

  static void RegistryChangeCallback(const TCHAR* key_name,
                                     const TCHAR* value_name,
                                     RegistryChangeType change_type,
                                     const void* new_value_data,
                                     void* user_data) {
    EXPECT_STREQ(kKeyName, key_name);
    EXPECT_STREQ(kValueName, value_name);
    EXPECT_EQ(REGISTRY_CHANGE_TYPE_UPDATE, change_type);
    EXPECT_TRUE(new_value_data);
    EXPECT_TRUE(user_data);
    RegistryMonitorTest* object = static_cast<RegistryMonitorTest*>(user_data);
    const TCHAR* actual_value = static_cast<const TCHAR*>(new_value_data);
    EXPECT_STREQ(_T("foo"), actual_value);
    EXPECT_TRUE(::SetEvent(get(object->registry_changed_event_)));
  }

  static void RegistryChangesCallback(const TCHAR* key_name,
                                     const TCHAR* value_name,
                                     RegistryChangeType change_type,
                                     const void* new_value_data,
                                     void* user_data) {
    EXPECT_STREQ(kKeyName, key_name);
    EXPECT_STREQ(kValueName, value_name);
    EXPECT_EQ(REGISTRY_CHANGE_TYPE_UPDATE, change_type);
    EXPECT_TRUE(new_value_data);
    EXPECT_TRUE(user_data);
    RegistryMonitorTest* object = static_cast<RegistryMonitorTest*>(user_data);
    EXPECT_TRUE(::SetEvent(get(object->registry_changed_event_)));
  }

  static void RegistryCreateCallback(const TCHAR* key_name,
                                     const TCHAR* value_name,
                                     RegistryChangeType change_type,
                                     const void* new_value_data,
                                     void* user_data) {
    EXPECT_STREQ(kKeyName, key_name);
    EXPECT_STREQ(kValueName, value_name);
    EXPECT_EQ(REGISTRY_CHANGE_TYPE_CREATE, change_type);
    EXPECT_TRUE(new_value_data);
    EXPECT_TRUE(user_data);
    RegistryMonitorTest* object = static_cast<RegistryMonitorTest*>(user_data);
    DWORD actual_value = reinterpret_cast<DWORD>(new_value_data);
    EXPECT_EQ(1, actual_value);
    EXPECT_TRUE(::SetEvent(get(object->registry_changed_event_)));
  }

  static void RegistryKeyCallback(const TCHAR* key_name, void* user_data) {
    EXPECT_STREQ(kKeyName, key_name);
    RegistryMonitorTest* object = static_cast<RegistryMonitorTest*>(user_data);
    EXPECT_TRUE(::SetEvent(get(object->registry_changed_event_)));
  }

  scoped_event registry_changed_event_;

  static DWORD const kWaitForChangeMs = 5000;
};

TEST_F(RegistryMonitorTest, DeleteValue) {
  DWORD value = 0;
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull, kValueName, value));
  RegistryMonitor registry_monitor;
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.Initialize());
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.MonitorValue(
      HKEY_CURRENT_USER, kKeyName, kValueName, REG_DWORD,
      RegistryDeleteCallback, this));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.StartMonitoring());

  EXPECT_HRESULT_SUCCEEDED(RegKey::DeleteValue(kKeyNameFull, kValueName));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));
}

TEST_F(RegistryMonitorTest, ChangeValue) {
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull, kValueName, _T("")));
  RegistryMonitor registry_monitor;
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.Initialize());
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.MonitorValue(
    HKEY_CURRENT_USER, kKeyName, kValueName, REG_SZ,
    RegistryChangeCallback, this));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.StartMonitoring());

  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull,
                                            kValueName,
                                            _T("foo")));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));
}

// Tests changing the same value two times. This is useful to detect if
// the key is registered back for notification after a succesful callback.
TEST_F(RegistryMonitorTest, ChangeValues) {
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull,
                                            kValueName,
                                            _T("")));

  RegistryMonitor registry_monitor;
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.Initialize());
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.MonitorValue(
      HKEY_CURRENT_USER, kKeyName, kValueName, REG_SZ,
      RegistryChangesCallback, this));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.StartMonitoring());

  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull,
                                            kValueName,
                                            _T("foo")));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));
  EXPECT_TRUE(::ResetEvent(get(registry_changed_event_)));

  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull,
                                            kValueName,
                                            _T("bar")));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));
}

TEST_F(RegistryMonitorTest, CreateValue) {
  RegistryMonitor registry_monitor;
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.Initialize());
  ASSERT_HRESULT_SUCCEEDED(RegKey::CreateKey(kKeyNameFull));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.MonitorValue(
      HKEY_CURRENT_USER, kKeyName, kValueName, REG_DWORD,
      RegistryCreateCallback, this));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.StartMonitoring());

  DWORD value = 1;
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(kKeyNameFull, kValueName, value));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));
}

// Monitoring values under the same key pair is allowed.
TEST_F(RegistryMonitorTest, MonitorSame) {
  RegistryMonitor registry_monitor;
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.Initialize());
  ASSERT_HRESULT_SUCCEEDED(RegKey::CreateKey(kKeyNameFull));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.MonitorValue(
      HKEY_CURRENT_USER, kKeyName, kValueName, REG_DWORD,
      RegistryCreateCallback, this));
  ASSERT_HRESULT_SUCCEEDED(registry_monitor.MonitorValue(
      HKEY_CURRENT_USER, kKeyName, kValueName, REG_DWORD,
      RegistryCreateCallback, this));
}

TEST_F(RegistryMonitorTest, MonitorKey) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(kKeyNameFull));

  RegistryMonitor registry_monitor;
  EXPECT_HRESULT_SUCCEEDED(registry_monitor.Initialize());
  EXPECT_HRESULT_SUCCEEDED(registry_monitor.MonitorKey(
      HKEY_CURRENT_USER, kKeyName, RegistryKeyCallback, this));

  EXPECT_HRESULT_SUCCEEDED(registry_monitor.StartMonitoring());

  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(_T("HKCU\\key\\subkey")));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));

  EXPECT_TRUE(::ResetEvent(get(registry_changed_event_)));
  EXPECT_HRESULT_SUCCEEDED(RegKey::DeleteKey(_T("HKCU\\key\\subkey")));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(registry_changed_event_),
                                                 kWaitForChangeMs));
}

}  // namespace omaha

