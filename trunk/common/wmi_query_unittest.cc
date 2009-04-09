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

#include "omaha/common/wmi_query.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(WmiQueryTest, WmiQuery) {
  WmiQuery wq;
  ASSERT_HRESULT_SUCCEEDED(wq.Connect(_T("root\\CIMV2")));
  ASSERT_HRESULT_SUCCEEDED(wq.Query(_T("select * from Win32_OperatingSystem")));

  CString manufacturer;
  EXPECT_HRESULT_SUCCEEDED(wq.GetValue(_T("Manufacturer"), &manufacturer));
  EXPECT_STREQ(_T("Microsoft Corporation"), manufacturer);

  // Expect a retail build of the OS.
  bool is_debug(true);
  EXPECT_HRESULT_SUCCEEDED(wq.GetValue(_T("Debug"), &is_debug));
  EXPECT_FALSE(is_debug);

  int max_number_of_processes(0);
  EXPECT_HRESULT_SUCCEEDED(wq.GetValue(_T("MaxNumberOfProcesses"),
                           &max_number_of_processes));
  EXPECT_EQ(-1, max_number_of_processes);
}

}  // namespace omaha

