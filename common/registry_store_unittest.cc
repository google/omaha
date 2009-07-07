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
// Unit test for RegistryStore.

#include "omaha/common/registry_store.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

static const TCHAR kRSTestKey[] =
    _T("HKCU\\Software\\Google\\Common_Installer__TEST_STORE");
static const TCHAR kRSTestName[] = _T("TestValueName");
static const byte kRSTestValue[] = {0x01, 0x02, 0x03, 0x04, 0x05};
static const int kRSTestValueSize = arraysize(kRSTestValue);

TEST(RegistryStoreTest, RegistryStore) {
  RegistryStore registry_store;
  uint32 value_count = 42; // We want to make sure it's overwritten with 0.
  CString value_name;
  std::vector<byte> data;

  // Set up and get in a known state.
  EXPECT_TRUE(registry_store.Open(kRSTestKey));
  EXPECT_TRUE(registry_store.Clear());

  // Add and test a single value
  EXPECT_FALSE(registry_store.Exists(kRSTestName));
  EXPECT_FALSE(registry_store.Read(kRSTestName, &data));
  data.clear();

  EXPECT_TRUE(registry_store.Write(kRSTestName,
                                   const_cast<byte*>(kRSTestValue),
                                   kRSTestValueSize));

  EXPECT_TRUE(registry_store.Exists(kRSTestName));
  EXPECT_TRUE(registry_store.Read(kRSTestName, &data));
  EXPECT_EQ(data.size(), kRSTestValueSize);
  for (int i = 0; i < kRSTestValueSize; i++)
    EXPECT_EQ(data[i], kRSTestValue[i]);

  // Remove and re-add value
  EXPECT_TRUE(registry_store.Remove(kRSTestName));
  EXPECT_FALSE(registry_store.Exists(kRSTestName));
  EXPECT_TRUE(registry_store.GetValueCount(&value_count));
  EXPECT_EQ(value_count, 0);
  EXPECT_TRUE(registry_store.Write(kRSTestName,
                                   const_cast<byte*>(kRSTestValue),
                                   kRSTestValueSize));
  EXPECT_TRUE(registry_store.GetValueCount(&value_count));
  EXPECT_EQ(value_count, 1);
  EXPECT_TRUE(registry_store.GetValueNameAt(0, &value_name));
  EXPECT_TRUE(value_name == kRSTestName);

  // Clean up and finish.
  EXPECT_TRUE(registry_store.Clear());
  EXPECT_FALSE(registry_store.Exists(kRSTestName));
  EXPECT_FALSE(registry_store.GetValueCount(&value_count));
  EXPECT_TRUE(registry_store.Close());
}

}  // namespace omaha
