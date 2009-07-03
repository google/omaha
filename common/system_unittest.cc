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
// System unittest
//
// TODO(omaha): there are some untested functions: memory stats, thread
// priorities, getdirsize (that's mine), backup/restore of registry trees.. not
// sure how high priority it is to test these things but should probably be
// added

#include "omaha/common/system.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(SystemTest, System) {
    uint32 time_waited = 0;
    ASSERT_SUCCEEDED(System::WaitForDiskActivity(10000, 25, &time_waited));

    uint64 free_bytes_current_user = 0;
    uint64 total_bytes_current_user = 0;
    uint64 free_bytes_all_users = 0;
    ASSERT_SUCCEEDED(System::GetDiskStatistics(_T("C:\\"),
                                               &free_bytes_current_user,
                                               &total_bytes_current_user,
                                               &free_bytes_all_users));

    ASSERT_EQ(System::GetProcessHandleCount(),
              System::GetProcessHandleCountOld());
}

// Assume the workstations and PULSE are not running on batteries. The test
// fails on laptops running on batteries.
TEST(SystemTest, IsRunningOnBatteries) {
  ASSERT_FALSE(System::IsRunningOnBatteries());
}

TEST(SystemTest, GetProcessMemoryStatistics) {
  uint64 current_working_set(0);
  uint64 peak_working_set(0);
  uint64 min_working_set_size(0);
  uint64 max_working_set_size(0);
  ASSERT_HRESULT_SUCCEEDED(
    System::GetProcessMemoryStatistics(&current_working_set,
                                       &peak_working_set,
                                       &min_working_set_size,
                                       &max_working_set_size));
  EXPECT_LT(0, current_working_set);
  EXPECT_LT(0, peak_working_set);
  EXPECT_LT(0, min_working_set_size);
  EXPECT_LT(0, max_working_set_size);
}

TEST(SystemTest, GetProcessHandleCount) {
  DWORD handle_count(0);
  ASSERT_TRUE(::GetProcessHandleCount(::GetCurrentProcess(), &handle_count));
  EXPECT_LE(0u, handle_count);
  EXPECT_EQ(handle_count, System::GetProcessHandleCount());
}

}  // namespace omaha

