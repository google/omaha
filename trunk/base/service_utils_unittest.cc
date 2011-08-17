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

#include "omaha/base/service_utils.h"
#include <lmsname.h>
#include "omaha/testing/unit_test.h"

namespace omaha {

// Set to true when the test callback sees the service indicated in the
// context parameter.
bool found_service = false;

HRESULT TestEnumCallback(void* context, const wchar_t* service_name) {
  EXPECT_TRUE(context);

  const wchar_t* find_service_name = reinterpret_cast<const wchar_t*>(context);
  if (lstrcmpW(service_name, find_service_name) == 0) {
    found_service = true;
  }

  return S_OK;
}

TEST(ServiceUtilsTest, ScmDatabaseEnumerateServices) {
  found_service = false;
  EXPECT_TRUE(SUCCEEDED(ScmDatabase::EnumerateServices(TestEnumCallback,
                            reinterpret_cast<void*>(_T("RpcSs")))));
  EXPECT_TRUE(found_service);
}

TEST(ServiceUtilsTest, IsServiceInstalled) {
  EXPECT_TRUE(ServiceInstall::IsServiceInstalled(SERVICE_SCHEDULE));
  EXPECT_FALSE(ServiceInstall::IsServiceInstalled(_T("FooBar")));
}

TEST(ServiceUtilsTest, IsServiceRunning) {
  EXPECT_TRUE(ServiceUtils::IsServiceRunning(SERVICE_SCHEDULE));
  EXPECT_FALSE(ServiceUtils::IsServiceRunning(_T("FooBar")));
}

TEST(ServiceUtilsTest, IsServiceDisabled) {
  EXPECT_FALSE(ServiceUtils::IsServiceDisabled(SERVICE_SCHEDULE));
}

}  // namespace omaha

