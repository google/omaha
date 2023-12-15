// Copyright 2003-2009 Google Inc.
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

#include "omaha/base/reg_key.h"

#include "omaha/base/debug.h"
#include "omaha/base/dynamic_link_kernel32.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

#define kStTestRkeyRelativeBase   _T("Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME _T("\\UnitTest")
#define kStTestRkeyBase   _T("HKCU\\") kStTestRkeyRelativeBase
#define kStRkey1Name      _T("TEST")
#define kStRkey1          kStTestRkeyBase _T("\\") kStRkey1Name
#define kRkey1            kStTestRkeyRelativeBase _T("\\") kStRkey1Name
#define kStRkey2          kStTestRkeyBase _T("\\TEST2")
#define kStRkey3          kStTestRkeyBase _T("\\TEST3")
#define kRkey1SubkeyName  _T("subkey_test")
#define kRkey1Subkey      kRkey1 _T("\\") kRkey1SubkeyName
#define kStRkey1Subkey    kStRkey1 _T("\\") kRkey1SubkeyName

// NON - STATIC

#define kValNameInt          _T("Int32 Value")
#define kRenameValNameInt    _T("Renamed Int32 Value")
#define kIntVal              (DWORD)20
#define kIntVal2             (DWORD)30

#define kValNameInt64        _T("Int64 Value")
#define kIntVal64            (DWORD64)40
#define kIntVal642           (DWORD64)50

#define kValNameStr          _T("Str Value")
#define kStrVal              _T("Some string data 1")
#define kStrVal2             _T("Some string data 2")

#define kValNameBinary       _T("Binary Value")
#define kBinaryVal           "Some binary data abcdefghi 1"
#define kBinaryVal2          "Some binary data abcdefghi 2"

// STATIC

#define kStValNameInt        _T("Static Int32 Value")
#define kStIntVal            (DWORD)60

#define kStValNameInt64      _T("Static Int64 Value")
#define kStIntVal64          (DWORD64)80

#define kStValNameFloat      _T("Static Float Value")
#define kStFloatVal          (static_cast<float>(12.3456789))

#define kStValNameDouble     _T("Static Double Value")
#define kStDoubleVal         (static_cast<double>(98.7654321))

#define kStValNameStr        _T("Static Str Value")
#define kRenameStValNameStr  _T("Renamed Static Str Value")
#define kStStrVal            _T("Some static string data 2")

#define kStValNameBinary     _T("Static Binary Value")
#define kStBinaryVal         "Some static binary data abcdefghi 2"

// Test the private member functions of RegKey
class RegKeyTestClass : public testing::Test {
 protected:
  static HKEY GetHKey(const RegKey& reg) {
    return reg.h_key_;
  }

  static CString GetParentKeyInfo(CString* key_name) {
    return RegKey::GetParentKeyInfo(key_name);
  }
};

class RegKeyCleanupTestKeyTest : public testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_SUCCEEDED(RegKey::DeleteKey(kStTestRkeyBase));
  }

  virtual void TearDown() {
    EXPECT_SUCCEEDED(RegKey::DeleteKey(kStTestRkeyBase));
  }

  RegKey key_;
};

// Make sure the RegKey is nice and clean when we first initialize it
TEST_F(RegKeyTestClass, Init) {
  // Make a new RegKey object so we can test its pristine state
  RegKey reg;

  ASSERT_TRUE(GetHKey(reg) == NULL);
}

// Make sure the helper functions work
TEST_F(RegKeyTestClass, Helper) {
  // Dud items cause NULL
  CString temp_key;
  RegKey::RootKeyInfo info;

  // RegKey::GetRootKeyInfo turns a string into the HKEY and subtree value

  // Try out some dud values
  temp_key = _T("");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_TRUE(info.key == NULL);
  ASSERT_STREQ(temp_key, _T(""));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_TRUE(info.key == NULL);
  ASSERT_STREQ(temp_key, _T(""));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  // The basics
  temp_key = _T("HKLM\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_LOCAL_MACHINE);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKEY_LOCAL_MACHINE\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_LOCAL_MACHINE);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKLM[64]\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_LOCAL_MACHINE);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k64BitView);

  temp_key = _T("HKCU\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_CURRENT_USER);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKEY_CURRENT_USER\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_CURRENT_USER);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKU\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_USERS);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKEY_USERS\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_USERS);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKCR\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_CLASSES_ROOT);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("HKEY_CLASSES_ROOT\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_CLASSES_ROOT);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  // Make sure it is case insensitive
  temp_key = _T("hkcr\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_CLASSES_ROOT);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  temp_key = _T("hkey_CLASSES_ROOT\\a");
  info = RegKey::GetRootKeyInfo(&temp_key);
  ASSERT_EQ(info.key, HKEY_CLASSES_ROOT);
  ASSERT_STREQ(temp_key, _T("a"));
  ASSERT_EQ(info.wow_override, RegKey::k32BitView);

  // Test out temp_GetParentKeyInfo

  // dud cases
  temp_key = _T("");
  ASSERT_STREQ(GetParentKeyInfo(&temp_key), _T(""));
  ASSERT_STREQ(temp_key, _T(""));

  temp_key = _T("a");
  ASSERT_STREQ(GetParentKeyInfo(&temp_key), _T(""));
  ASSERT_STREQ(temp_key, _T("a"));

  temp_key = _T("a\\b");
  ASSERT_STREQ(GetParentKeyInfo(&temp_key), _T("a"));
  ASSERT_STREQ(temp_key, _T("b"));

  temp_key = _T("\\b");
  ASSERT_STREQ(GetParentKeyInfo(&temp_key), _T(""));
  ASSERT_STREQ(temp_key, _T("b"));


  // Some regular cases
  temp_key = _T("HKEY_CLASSES_ROOT\\moon");
  ASSERT_STREQ(GetParentKeyInfo(&temp_key), _T("HKEY_CLASSES_ROOT"));
  ASSERT_STREQ(temp_key, _T("moon"));

  temp_key = _T("HKEY_CLASSES_ROOT\\moon\\doggy");
  ASSERT_STREQ(GetParentKeyInfo(&temp_key),
               _T("HKEY_CLASSES_ROOT\\moon"));
  ASSERT_STREQ(temp_key, _T("doggy"));
}


TEST(RegKeyTest, RegKey) {
  //
  // PRIVATE MEMBER WHITE BOX TESTS
  //
  RegKeyWithChangeEvent r_key;
  bool bool_res = false;
  HRESULT hr = E_FAIL;
  DWORD int_val = 0;
  DWORD64 int64_val = 0;
  time64 t = 0;
  float float_val = 0;
  double double_val = 0;
  TCHAR * str_val = NULL;
  byte * binary_val = NULL;
  std::unique_ptr<byte[]> binary_ptr;
  size_t byte_count = 0;

  // Just in case...
  // make sure the no test key residue is left from previous aborted runs
  hr = RegKey::DeleteKey(kStTestRkeyBase);

  // first test the non-static version

  // create a reg key
  hr = r_key.Create(HKEY_CURRENT_USER, kRkey1);
  ASSERT_SUCCEEDED(hr);

  // do the create twice - it should return the already created one
  hr = r_key.Create(HKEY_CURRENT_USER, kRkey1);
  ASSERT_SUCCEEDED(hr);

  // now do an open - should work just fine
  hr = r_key.Open(HKEY_CURRENT_USER, kRkey1);
  ASSERT_SUCCEEDED(hr);

  // get an in-existent value
  hr = r_key.GetValue(kValNameInt, &int_val);
  ASSERT_EQ(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  // get an in-existent value type
  DWORD value_type = REG_NONE;
  hr = r_key.GetValueType(kValNameInt, &value_type);
  ASSERT_EQ(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  // set-up an event to watch for changes
  hr = r_key.SetupEvent(TRUE,
                        REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
  ASSERT_SUCCEEDED(hr);
  HANDLE change_event = r_key.change_event();
  ASSERT_EQ(::WaitForSingleObject(change_event, 0), WAIT_TIMEOUT);

  // set and get some values and verify that the handle gets signaled

  // set an INT 32
  hr = r_key.SetValue(kValNameInt, kIntVal);
  ASSERT_SUCCEEDED(hr);

  hr = r_key.GetValueType(kValNameInt, &value_type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(REG_DWORD, value_type);
  hr = RegKey::GetValueType(kStRkey1, kValNameInt, &value_type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(REG_DWORD, value_type);

  // verify that we got the change and that the event got reset appropriately
  // and set-up the notification again (use the actual event this time)
  ASSERT_EQ(::WaitForSingleObject(change_event, 0), WAIT_OBJECT_0);
  hr = r_key.SetupEvent(TRUE,
                        REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
  ASSERT_SUCCEEDED(hr);
  ASSERT_FALSE(r_key.HasChangeOccurred());

  // check that the value exists
  bool_res = r_key.HasValue(kValNameInt);
  ASSERT_TRUE(bool_res);
  // No change expected on a read
  ASSERT_FALSE(r_key.HasChangeOccurred());

  // read it back
  hr = r_key.GetValue(kValNameInt, &int_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(int_val, kIntVal);
  // No change expected on a read
  ASSERT_FALSE(r_key.HasChangeOccurred());

  // set it again!
  hr = r_key.SetValue(kValNameInt, kIntVal2);
  ASSERT_SUCCEEDED(hr);
  // verify that we got the change and that the event got reset appropriately
  // and set-up the notification again
  ASSERT_TRUE(r_key.HasChangeOccurred());
  hr = r_key.SetupEvent(TRUE,
                        REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
  ASSERT_SUCCEEDED(hr);
  ASSERT_FALSE(r_key.HasChangeOccurred());

  // read it again
  hr = r_key.GetValue(kValNameInt, &int_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(int_val, kIntVal2);
  // No change expected on a read
  ASSERT_FALSE(r_key.HasChangeOccurred());

  // delete the value
  hr = r_key.DeleteValue(kValNameInt);
  ASSERT_SUCCEEDED(hr);
  // verify that we got the change
  ASSERT_TRUE(r_key.HasChangeOccurred());

  // check that the value is gone
  bool_res = r_key.HasValue(kValNameInt);
  ASSERT_FALSE(bool_res);

  // set an INT 64
  hr = r_key.SetValue(kValNameInt64, kIntVal64);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = r_key.HasValue(kValNameInt64);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = r_key.GetValue(kValNameInt64, &int64_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(int64_val, kIntVal64);

  // delete the value
  hr = r_key.DeleteValue(kValNameInt64);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = r_key.HasValue(kValNameInt64);
  ASSERT_FALSE(bool_res);

  // set a string
  hr = r_key.SetValue(kValNameStr, kStrVal);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = r_key.HasValue(kValNameStr);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = r_key.GetValue(kValNameStr, &str_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_STREQ(str_val, kStrVal);
  delete [] str_val;

  // set it again
  hr = r_key.SetValue(kValNameStr, kStrVal2);
  ASSERT_SUCCEEDED(hr);

  // read it again
  hr = r_key.GetValue(kValNameStr, &str_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_STREQ(str_val, kStrVal2);
  delete [] str_val;

  // delete the value
  hr = r_key.DeleteValue(kValNameStr);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = r_key.HasValue(kValNameInt);
  ASSERT_FALSE(bool_res);

  // set a binary value
  hr = r_key.SetValue(kValNameBinary, (const byte *)kBinaryVal,
                      sizeof(kBinaryVal)-1);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = r_key.HasValue(kValNameBinary);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = r_key.GetValue(kValNameBinary, &binary_val, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(0, memcmp(binary_val, kBinaryVal, sizeof(kBinaryVal)-1));
  delete [] binary_val;

  hr = r_key.GetValue(kValNameBinary, &binary_ptr, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(0, memcmp(binary_ptr.get(), kBinaryVal, sizeof(kBinaryVal)-1));

  // set it again
  hr = r_key.SetValue(kValNameBinary, (const byte *)kBinaryVal2,
                      sizeof(kBinaryVal)-1);
  ASSERT_SUCCEEDED(hr);

  // read it again
  hr = r_key.GetValue(kValNameBinary, &binary_val, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(0, memcmp(binary_val, kBinaryVal2, sizeof(kBinaryVal2)-1));
  delete [] binary_val;

  hr = r_key.GetValue(kValNameBinary, &binary_ptr, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(0, memcmp(binary_ptr.get(), kBinaryVal2, sizeof(kBinaryVal2)-1));

  // delete the value
  hr = r_key.DeleteValue(kValNameBinary);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = r_key.HasValue(kValNameBinary);
  ASSERT_FALSE(bool_res);

  // set some values and check the total count

  // set an INT 32
  hr = r_key.SetValue(kValNameInt, kIntVal);
  ASSERT_SUCCEEDED(hr);

  // set an INT 64
  hr = r_key.SetValue(kValNameInt64, kIntVal64);
  ASSERT_SUCCEEDED(hr);

  // set a string
  hr = r_key.SetValue(kValNameStr, kStrVal);
  ASSERT_SUCCEEDED(hr);

  hr = r_key.GetValueType(kValNameStr, &value_type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(REG_SZ, value_type);
  hr = RegKey::GetValueType(kStRkey1, kValNameStr, &value_type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(REG_SZ, value_type);

  // set a binary value
  hr = r_key.SetValue(kValNameBinary, (const byte *)kBinaryVal,
                      sizeof(kBinaryVal)-1);
  ASSERT_SUCCEEDED(hr);

  // get the value count
  uint32 value_count = r_key.GetValueCount();
  ASSERT_EQ(value_count, 4);

  // check the value names
  CString value_name;
  DWORD type;

  hr = r_key.GetValueNameAt(0, &value_name, &type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(value_name, kValNameInt);
  ASSERT_EQ(type, REG_DWORD);

  hr = r_key.GetValueNameAt(1, &value_name, &type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(value_name, kValNameInt64);
  ASSERT_EQ(type, REG_QWORD);

  hr = r_key.GetValueNameAt(2, &value_name, &type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(value_name, kValNameStr);
  ASSERT_EQ(type, REG_SZ);

  hr = r_key.GetValueNameAt(3, &value_name, &type);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(value_name, kValNameBinary);
  ASSERT_EQ(type, REG_BINARY);

  // check that there are no more values
  hr = r_key.GetValueNameAt(4, &value_name, &type);
  ASSERT_FAILED(hr);

  uint32 subkey_count = r_key.GetSubkeyCount();
  ASSERT_EQ(subkey_count, 0);

  RegKey temp_key;

  // now create a subkey and make sure we can get the name
  hr = temp_key.Create(HKEY_CURRENT_USER, kRkey1Subkey);
  ASSERT_SUCCEEDED(hr);

  // check the subkey exists
  bool_res = r_key.HasSubkey(kRkey1SubkeyName);
  ASSERT_TRUE(bool_res);

  // check the name
  subkey_count = r_key.GetSubkeyCount();
  ASSERT_EQ(subkey_count, 1);

  CString subkey_name;
  hr = r_key.GetSubkeyNameAt(0, &subkey_name);
  ASSERT_EQ(subkey_name, kRkey1SubkeyName);

  // verify that the event handle remained the same throughout everything
  ASSERT_EQ(change_event, r_key.change_event());

  // close this key
  r_key.Close();

  // whack the whole key
  hr = RegKey::DeleteKey(kStTestRkeyBase);
  ASSERT_SUCCEEDED(hr);

  // STATIC
  // now set a different value using the static versions

  // get an in-existent value from an un-existent key
  hr = RegKey::GetValue(kStRkey1, kStValNameInt, &int_val);
  ASSERT_EQ(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  // set int32
  hr = RegKey::SetValue(kStRkey1, kStValNameInt, kStIntVal);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameInt);
  ASSERT_TRUE(bool_res);

  // get an in-existent value from an existent key
  hr = RegKey::GetValue(kStRkey1, _T("bogus"), &int_val);
  ASSERT_EQ(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameInt, &int_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(int_val, kStIntVal);

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameInt);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameInt);
  ASSERT_FALSE(bool_res);


  // set int64
  hr = RegKey::SetValue(kStRkey1, kStValNameInt64, kStIntVal64);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameInt64);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameInt64, &int64_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(int64_val, kStIntVal64);

  // read it back to test time64
  bool limited_value;
  hr = GetLimitedTimeValue(kStRkey1, kStValNameInt64,  kStIntVal64 + 10, &t,
                           &limited_value);
  ASSERT_SUCCEEDED(hr);
  EXPECT_FALSE(limited_value);
  ASSERT_EQ(t, kStIntVal64);
  hr = GetLimitedTimeValue(kStRkey1, kStValNameInt64,  kStIntVal64 - 10, &t,
                           &limited_value);
  EXPECT_TRUE(limited_value);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(t, kStIntVal64 - 10);
  // Verify that the GetValue permanently made the value lower
  hr = GetLimitedTimeValue(kStRkey1, kStValNameInt64,  kStIntVal64, &t,
                           &limited_value);
  EXPECT_FALSE(limited_value);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(t, kStIntVal64 - 10);

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameInt64);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameInt64);
  ASSERT_FALSE(bool_res);

  // set float
  hr = RegKey::SetValue(kStRkey1, kStValNameFloat, kStFloatVal);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameFloat);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameFloat, &float_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(float_val, kStFloatVal);

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameFloat);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameFloat);
  ASSERT_FALSE(bool_res);
  hr = RegKey::GetValue(kStRkey1, kStValNameFloat, &float_val);
  ASSERT_FAILED(hr);


  // set double
  hr = RegKey::SetValue(kStRkey1, kStValNameDouble, kStDoubleVal);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameDouble);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameDouble, &double_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(double_val, kStDoubleVal);

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameDouble);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameDouble);
  ASSERT_FALSE(bool_res);
  hr = RegKey::GetValue(kStRkey1, kStValNameDouble, &double_val);
  ASSERT_FAILED(hr);

  // set string
  hr = RegKey::SetValue(kStRkey1, kStValNameStr, kStStrVal);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameStr);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameStr, &str_val);
  ASSERT_SUCCEEDED(hr);
  ASSERT_STREQ(str_val, kStStrVal);
  delete [] str_val;

  // get an in-existent value from an existent key
  hr = RegKey::GetValue(kStRkey1, _T("bogus"), &str_val);
  ASSERT_EQ(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameStr);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameStr);
  ASSERT_FALSE(bool_res);

  // set binary
  hr = RegKey::SetValue(kStRkey1, kStValNameBinary, (const byte *)kStBinaryVal,
                        sizeof(kStBinaryVal)-1);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameBinary);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameBinary, &binary_val, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(0, memcmp(binary_val, kStBinaryVal, sizeof(kStBinaryVal)-1));
  delete [] binary_val;

  hr = RegKey::GetValue(kStRkey1, kStValNameBinary, &binary_ptr, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(0, memcmp(binary_ptr.get(), kStBinaryVal, sizeof(kStBinaryVal)-1));

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameBinary);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameBinary);
  ASSERT_FALSE(bool_res);

  // special case - set a binary value with length 0
  hr = RegKey::SetValue(kStRkey1, kStValNameBinary,
                        (const byte *)kStBinaryVal, 0);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameBinary);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameBinary, &binary_val, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(byte_count, 0);
  ASSERT_TRUE(binary_val == NULL);
  delete [] binary_val;

  hr = RegKey::GetValue(kStRkey1, kStValNameBinary, &binary_ptr, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(byte_count, 0);
  ASSERT_FALSE(binary_ptr);

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameBinary);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameBinary);
  ASSERT_FALSE(bool_res);

  // special case - set a NULL binary value
  hr = RegKey::SetValue(kStRkey1, kStValNameBinary, NULL, 100);
  ASSERT_SUCCEEDED(hr);

  // check that the value exists
  bool_res = RegKey::HasValue(kStRkey1, kStValNameBinary);
  ASSERT_TRUE(bool_res);

  // read it back
  hr = RegKey::GetValue(kStRkey1, kStValNameBinary, &binary_val, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(byte_count, 0);
  ASSERT_TRUE(binary_val == NULL);
  delete [] binary_val;

  hr = RegKey::GetValue(kStRkey1, kStValNameBinary, &binary_ptr, &byte_count);
  ASSERT_SUCCEEDED(hr);
  ASSERT_EQ(byte_count, 0);
  ASSERT_TRUE(binary_ptr.get() == nullptr);

  // delete the value
  hr = RegKey::DeleteValue(kStRkey1, kStValNameBinary);
  ASSERT_SUCCEEDED(hr);

  // check that the value is gone
  bool_res = RegKey::HasValue(kStRkey1, kStValNameBinary);
  ASSERT_FALSE(bool_res);

  // whack the whole key

  hr = RegKey::DeleteKey(kStTestRkeyBase);
  ASSERT_SUCCEEDED(hr);
}

TEST_F(RegKeyTestClass, VerifyWow64Redirection) {
  // Verifies that Open/Create calls accessing HKLM/SOFTWARE always redirect
  // to the 32-bit copy of the key (HKLM/SOFTWARE/Wow6432Node) on a 64-bit OS.

  bool should_run = false;
#ifdef _WIN64
  // Always run if the code being tested is 64-bit native.
  should_run = true;
#else
  // We still want to verify this if we're 32-bit code on a 64-bit OS.
  BOOL is64bit = FALSE;
  ASSERT_NE(0, Kernel32::IsWow64Process(GetCurrentProcess(), &is64bit));
  should_run = !!is64bit;
#endif

  if (!should_run) {
    std::cout << "Skipping VerifyWow64Redirection on 32-bit." << std::endl;
    return;
  }

  HKEY hk = NULL;
  const REGSAM perms32 = KEY_ALL_ACCESS | KEY_WOW64_32KEY;
  const REGSAM perms64 = KEY_ALL_ACCESS | KEY_WOW64_64KEY;

  EXPECT_SUCCEEDED(RegKey::CreateKey(_T("HKLM\\Software\\UpdateRedirTest")));

  LONG err = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\UpdateRedirTest"),
                       0, perms32, &hk);
  EXPECT_EQ(ERROR_SUCCESS, err);
  if (err == ERROR_SUCCESS) {
    ::RegCloseKey(hk);
  }
  err = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\UpdateRedirTest"),
                       0, perms64, &hk);
  EXPECT_EQ(ERROR_FILE_NOT_FOUND, err);
  if (err == ERROR_SUCCESS) {
    ::RegCloseKey(hk);
  }

  EXPECT_SUCCEEDED(RegKey::DeleteKey(_T("HKLM\\Software\\UpdateRedirTest")));

  err = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\UpdateRedirTest"),
                       0, perms32, &hk);
  EXPECT_EQ(ERROR_FILE_NOT_FOUND, err);
  if (err == ERROR_SUCCESS) {
    // TODO(omaha): If we're okay with the unit tests only running on Vista+,
    // it'd be nice to add a call to ::RegDeleteTree(hk, NULL) here, and below.
    ::RegCloseKey(hk);
  }
  err = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\UpdateRedirTest"),
                       0, perms64, &hk);
  EXPECT_EQ(ERROR_FILE_NOT_FOUND, err);
  if (err == ERROR_SUCCESS) {
    ::RegCloseKey(hk);
  }
}

// RegKey::GetValue changes the output CString when errors occur.
TEST_F(RegKeyTestClass, ChangesStringOnErrors) {
  CString string_val = _T("foo");
  EXPECT_FAILED(RegKey::GetValue(_T("HCKU"), _T("no_such_value"), &string_val));
  ASSERT_TRUE(string_val.IsEmpty());
}

TEST_F(RegKeyCleanupTestKeyTest, CreateKeys) {
  // 3 keys specified but the count is two.
  const TCHAR* keys[] = {kStRkey1, kStRkey2, kStRkey3};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys, 2));

  EXPECT_TRUE(RegKey::HasKey(kStRkey1));
  EXPECT_TRUE(RegKey::HasKey(kStRkey2));
  EXPECT_FALSE(RegKey::HasKey(kStRkey3));
}

TEST_F(RegKeyCleanupTestKeyTest, CreateKey) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kStRkey1));

  EXPECT_TRUE(RegKey::HasKey(kStRkey1));
}

TEST_F(RegKeyCleanupTestKeyTest, RenameValue) {
  RegKey reg_key;
  ASSERT_SUCCEEDED(reg_key.Create(HKEY_CURRENT_USER, kRkey1));
  ASSERT_SUCCEEDED(reg_key.SetValue(kValNameInt, kIntVal));
  ASSERT_TRUE(reg_key.HasValue(kValNameInt));

  ASSERT_SUCCEEDED(reg_key.RenameValue(kValNameInt, kRenameValNameInt));
  ASSERT_FALSE(reg_key.HasValue(kValNameInt));

  DWORD int_val = 0;
  EXPECT_SUCCEEDED(reg_key.GetValue(kRenameValNameInt, &int_val));
  EXPECT_EQ(kIntVal, int_val);

  EXPECT_SUCCEEDED(reg_key.Close());
}

TEST_F(RegKeyCleanupTestKeyTest, RenameValueStatic) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kStRkey1, kStValNameStr, kStStrVal));
  ASSERT_TRUE(RegKey::HasValue(kStRkey1, kStValNameStr));

  RegKey::RenameValue(kStRkey1, kStValNameStr, kRenameStValNameStr);
  ASSERT_FALSE(RegKey::HasValue(kStRkey1, kStValNameStr));

  CString str_val;
  EXPECT_SUCCEEDED(RegKey::GetValue(kStRkey1, kRenameStValNameStr, &str_val));
  EXPECT_STREQ(kStStrVal, str_val);
}

TEST_F(RegKeyCleanupTestKeyTest, CopyValue) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kStRkey1, kStValNameStr, kStStrVal));
  EXPECT_SUCCEEDED(RegKey::SetValue(kStRkey1, NULL, kStStrVal));
  EXPECT_TRUE(RegKey::HasValue(kStRkey1, kStValNameStr));

  // Test that CopyValue fails when the to_key does not exist.
  EXPECT_FALSE(RegKey::HasKey(kStRkey2));
  EXPECT_FAILED(RegKey::CopyValue(kStRkey1, kStRkey2, kStValNameStr));
  EXPECT_FALSE(RegKey::HasKey(kStRkey2));

  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey2));
  // Test CopyValue(full_from_key_name, full_to_key_name, value_name).
  EXPECT_FALSE(RegKey::HasValue(kStRkey2, kStValNameStr));
  RegKey::CopyValue(kStRkey1, kStRkey2, kStValNameStr);
  CString str_val;
  EXPECT_SUCCEEDED(RegKey::GetValue(kStRkey2, kStValNameStr, &str_val));
  EXPECT_STREQ(kStStrVal, str_val);

  // Test CopyValue to a (Default) value.
  EXPECT_FALSE(RegKey::HasValue(kStRkey2, NULL));
  RegKey::CopyValue(kStRkey1, kStRkey2, NULL);
  str_val.Empty();
  EXPECT_SUCCEEDED(RegKey::GetValue(kStRkey2, NULL, &str_val));
  EXPECT_STREQ(kStStrVal, str_val);

  // Test CopyValue(full_from_key_name, from_value_name, full_to_key_name,
  //                to_value_name).
  EXPECT_FALSE(RegKey::HasValue(kStRkey2, kRenameStValNameStr));
  RegKey::CopyValue(kStRkey1, kStValNameStr, kStRkey2, kRenameStValNameStr);
  str_val.Empty();
  EXPECT_SUCCEEDED(RegKey::GetValue(kStRkey2, kRenameStValNameStr, &str_val));
  EXPECT_STREQ(kStStrVal, str_val);
}

// Delete a key that does not have children.

TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_NoChildren_Recursively) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));

  EXPECT_EQ(S_OK, RegKey::DeleteKey(kStRkey1, true));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));
}

TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_NoChildren_NotRecursively) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));

  EXPECT_EQ(S_OK, RegKey::DeleteKey(kStRkey1, false));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));
}

TEST_F(RegKeyCleanupTestKeyTest, RecurseDeleteSubKey_NoChildren) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));
  EXPECT_SUCCEEDED(key_.Open(kStTestRkeyBase));

  EXPECT_EQ(S_OK, key_.RecurseDeleteSubKey(kStRkey1Name));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));
}

TEST_F(RegKeyCleanupTestKeyTest, DeleteSubKey_NoChildren) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));
  EXPECT_SUCCEEDED(key_.Open(kStTestRkeyBase));

  EXPECT_EQ(S_OK, key_.DeleteSubKey(kStRkey1Name));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));
}

// Delete a key that has a child.

TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_WithChild_Recursively) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1Subkey));

  EXPECT_EQ(S_OK, RegKey::DeleteKey(kStRkey1, true));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

// Deleting a key with children present results in ERROR_ACCESS_DENIED.
TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_WithChild_NotRecursively) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1Subkey));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED),
            RegKey::DeleteKey(kStRkey1, false));
  EXPECT_TRUE(RegKey::HasKey(kStRkey1));
  EXPECT_TRUE(RegKey::HasKey(kStRkey1Subkey));
}

TEST_F(RegKeyCleanupTestKeyTest, RecurseDeleteSubKey_WithChild) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1Subkey));
  EXPECT_SUCCEEDED(key_.Open(kStTestRkeyBase));

  EXPECT_EQ(S_OK, key_.RecurseDeleteSubKey(kStRkey1Name));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

// Deleting a key with children present results in ERROR_ACCESS_DENIED.
TEST_F(RegKeyCleanupTestKeyTest, DeleteSubKey_WithChild) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1Subkey));
  EXPECT_SUCCEEDED(key_.Open(kStTestRkeyBase));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED),
            key_.DeleteSubKey(kStRkey1Name));
  EXPECT_TRUE(RegKey::HasKey(kStRkey1));
  EXPECT_TRUE(RegKey::HasKey(kStRkey1Subkey));
}

// Delete a key that does not exist.

TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_KeyDoesNotExist_Recursively) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));

  EXPECT_EQ(S_FALSE, RegKey::DeleteKey(kStRkey1Subkey, true));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_KeyDoesNotExist_NotRecursively) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));

  EXPECT_EQ(S_FALSE, RegKey::DeleteKey(kStRkey1Subkey, false));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

TEST_F(RegKeyCleanupTestKeyTest, RecurseDeleteSubKey_KeyDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));
  EXPECT_SUCCEEDED(key_.Open(kStRkey1));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));

  EXPECT_EQ(S_FALSE, key_.RecurseDeleteSubKey(kRkey1SubkeyName));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

TEST_F(RegKeyCleanupTestKeyTest, DeleteSubKey_KeyDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kStRkey1));
  EXPECT_SUCCEEDED(key_.Open(kStRkey1));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));

  EXPECT_EQ(S_FALSE, key_.DeleteSubKey(kRkey1SubkeyName));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

// Delete a key whose parent does not exist.
// There is no equivalent test for RecurseDeleteSubKey and DeleteSubKey.

TEST_F(RegKeyCleanupTestKeyTest, DeleteKey_ParentKeyDoesNotExist_Recursively) {
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));

  EXPECT_EQ(S_FALSE, RegKey::DeleteKey(kStRkey1Subkey, true));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

TEST_F(RegKeyCleanupTestKeyTest, DeleteLinkNotTarget) {
  // Create a target key with string and DWORD values.
  const CString target_path = kStRkey1 _T("\\LinkTarget");
  ASSERT_SUCCEEDED(
      RegKey::SetValue(target_path, _T("TargetString"), _T("Hello")));
  ASSERT_SUCCEEDED(
      RegKey::SetValue(target_path, _T("TargetDWORD"), static_cast<DWORD>(1)));

  // Create a source registry link to the above target key.
  const CString source_path = kStRkey1 _T("\\LinkSource");
  {
    RegKey link;
    EXPECT_SUCCEEDED(link.Create(source_path,
                                 nullptr,
                                 REG_OPTION_CREATE_LINK |
                                     REG_OPTION_NON_VOLATILE,
                                 KEY_WRITE));
    CString user_sid;
    ASSERT_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &user_sid));
    const CString value = _T("\\Registry\\User\\") +
                          user_sid +
                          _T("\\") kRkey1 _T("\\LinkTarget");
    ASSERT_SUCCEEDED(
        link.SetValue(_T("SymbolicLinkValue"),
                      reinterpret_cast<const byte*>(value.GetString()),
                      value.GetLength() * sizeof(TCHAR),
                      REG_LINK));
  }

  // Verify that reading from the source registry link actually reads the target
  // values.
  {
    CString string_value;
    ASSERT_SUCCEEDED(
        RegKey::GetValue(source_path, _T("TargetString"), &string_value));
    ASSERT_EQ(string_value, _T("Hello"));
    DWORD value = 0;
    ASSERT_SUCCEEDED(RegKey::GetValue(source_path, _T("TargetDWORD"), &value));
    ASSERT_EQ(value, 1U);
  }

  // Now delete the link and ensure that the source registry link is no longer
  // working, but the target key and values are still present.
  {
    ASSERT_SUCCEEDED(RegKey::DeleteKey(source_path));
    CString string_value;
    ASSERT_FAILED(
        RegKey::GetValue(source_path, _T("TargetString"), &string_value));
    ASSERT_SUCCEEDED(
        RegKey::GetValue(target_path, _T("TargetString"), &string_value));
    ASSERT_EQ(string_value, _T("Hello"));
    DWORD value = 0;
    ASSERT_FAILED(RegKey::GetValue(source_path, _T("TargetDWORD"), &value));
    ASSERT_SUCCEEDED(RegKey::GetValue(target_path, _T("TargetDWORD"), &value));
    ASSERT_EQ(value, 1U);
  }
}

TEST_F(RegKeyCleanupTestKeyTest,
       DeleteKey_ParentKeyDoesNotExist_NotRecursively) {
  EXPECT_FALSE(RegKey::HasKey(kStRkey1));

  EXPECT_EQ(S_FALSE, RegKey::DeleteKey(kStRkey1Subkey, false));
  EXPECT_FALSE(RegKey::HasKey(kStRkey1Subkey));
}

}  // namespace omaha
