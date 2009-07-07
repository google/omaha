// Copyright 2004-2009 Google Inc.
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
// localization_unittest.cpp
//
// Unit test functions for Localization

#include "omaha/common/localization.h"
#include "omaha/common/string.h"
#include "omaha/common/time.h"
#include "omaha/testing/unit_test.h"

using testing::Message;

namespace omaha {

// Test out the time display functions
void LocalizationTimeTest() {
  CString time_str;

  // Lets process this a bit to give ourselves a known time.
  SYSTEMTIME temp_time;
  temp_time.wYear = 2004;
  temp_time.wMonth = 4;
  temp_time.wDayOfWeek = 1;
  temp_time.wDay = 19;
  temp_time.wHour = 19;
  temp_time.wMinute = 18;
  temp_time.wSecond = 17;
  temp_time.wMilliseconds = 16;

  time64 override_time = SystemTimeToTime64(&temp_time);

  // Useful when debugging to confirm that this worked
  SYSTEMTIME confirm = Time64ToSystemTime(override_time);

  // the need to check two different times below is because:

  // FileTimeToLocalFileTime uses the current settings for the time
  // zone and daylight saving time. Therefore, if it is daylight
  // saving time, this function will take daylight saving time into
  // account, even if the time you are converting is in standard
  // time.
  // TODO(omaha): we may want to fix this.

  // Show just the time [ie -12:19pm]
  time_str = ShowTimeForLocale(override_time, 1033 /* US english */);
  ASSERT_TRUE(time_str == _T("12:18 PM") || time_str == _T("11:18 AM"))
      << _T("Returned time string was ") << time_str.GetString();

  // Show just the time [ie - 12:19:18pm]
  time_str = ShowFormattedTimeForLocale(override_time, 1033,
                                        _T("hh:mm:ss tt"));
  ASSERT_TRUE(time_str == _T("12:18:17 PM") || time_str == _T("11:18:17 AM"))
      << _T("Returned time string was ") << time_str.GetString();

  // Try it out with a some different values to test out single digit
  // minutes and such
  temp_time.wHour = 15;
  temp_time.wMinute = 4;
  temp_time.wSecond = 3;
  temp_time.wMilliseconds = 2;
  override_time = SystemTimeToTime64(&temp_time);

  time_str = ShowTimeForLocale(override_time, 1033);
  ASSERT_TRUE(time_str == _T("8:04 AM") || time_str == _T("7:04 AM"))
      << _T("Returned time string was ") << time_str.GetString();

  time_str = ShowFormattedTimeForLocale(override_time, 1033,
                                        _T("hh:mm:ss tt"));
  ASSERT_TRUE(time_str == _T("08:04:03 AM") || time_str == _T("07:04:03 AM"))
      << _T("Returned time string was ") << time_str.GetString();


  //
  // Check the date functionality
  //

  temp_time.wYear = 2004;
  temp_time.wMonth = 4;
  temp_time.wDayOfWeek = 1;
  temp_time.wDay = 19;

  // Show the short date
  time_str = ShowDateForLocale(override_time, 1033);
//  CHKM(time_str == _T("Monday, April 19, 2004"),
  ASSERT_STREQ(time_str, _T("4/19/2004"));

  // Show the customized date
  time_str = ShowFormattedDateForLocale(override_time, 1033,
                                        _T("MMM d, yyyy"));
  ASSERT_STREQ(time_str, _T("Apr 19, 2004"));

  // Try it out with a some different values to test out single dates and such
  temp_time.wDay = 1;
  override_time = SystemTimeToTime64(&temp_time);

  time_str = ShowFormattedDateForLocale(override_time, 1033,
                                        _T("ddd, MMM dd"));
  ASSERT_STREQ(time_str, _T("Thu, Apr 01"));

  time_str = ShowFormattedDateForLocale(override_time, 1033, _T("MM/dd/yyyy"));
  ASSERT_STREQ(time_str, _T("04/01/2004"));
}

// Test out the numbers and display functions
void LocalizationNumberTest() {
  // Make sure we are using the normal american version
  SetLcidOverride(1033);  // the codepage for american english

  // Try some basics
  ASSERT_STREQ(Show(1), _T("1"));
  ASSERT_STREQ(Show(2), _T("2"));

  // Try some extremes
  ASSERT_STREQ(Show(0), _T("0"));
  ASSERT_STREQ(Show(kInt32Max), _T("2,147,483,647"));
  ASSERT_STREQ(Show(-kInt32Max), _T("-2,147,483,647"));
  ASSERT_STREQ(Show(kUint32Max), _T("4,294,967,295"));

  // Try some doubles
  ASSERT_STREQ(Show(0.3, 0), _T("0"));
  ASSERT_STREQ(Show(0.3, 1), _T("0.3"));
  ASSERT_STREQ(Show(0.3, 2), _T("0.30"));
  ASSERT_STREQ(Show(0.3, 5), _T("0.30000"));

  // Try some with interesting rounding
  ASSERT_STREQ(Show(0.159, 0), _T("0"));
  ASSERT_STREQ(Show(0.159, 1), _T("0.1"));
  ASSERT_STREQ(Show(0.159, 2), _T("0.15"));
  ASSERT_STREQ(Show(0.159, 5), _T("0.15900"));

  // Try a nice whole number
  ASSERT_STREQ(Show(12.0, 0), _T("12"));
  ASSERT_STREQ(Show(12.0, 1), _T("12.0"));
  ASSERT_STREQ(Show(12.0, 2), _T("12.00"));
  ASSERT_STREQ(Show(12.0, 5), _T("12.00000"));
}

TEST(LocalizationTest, Localization) {
  LocalizationTimeTest();
  LocalizationNumberTest();
}

}  // namespace omaha

