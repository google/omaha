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
// Time unittest

#include <atltime.h>

#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Test generation of int32 time values (which uses Time64ToInt32 internally).
TEST(TimeTest, SystemTimeToInt32NearEpoch) {
  // Try a simple value near the start of int32 time.
  SYSTEMTIME system_time = {1970,  // year
                            1,     // month (1 == January)
                            0,     // day of week (0 == Sunday)
                            2,     // day of month
                            0,     // hour
                            0,     // minute
                            0,     // second
                            0};    // msec
  system_time.wMilliseconds = 0;
  int32 time1 = SystemTimeToInt32(&system_time);
  system_time.wMilliseconds = 999;
  int32 time2 = SystemTimeToInt32(&system_time);

  // Make sure result is independent of milliseconds value.
  ASSERT_EQ(time1, time2);

  // 00:00:00 on 1970/01/02 should return the number of seconds in 1 day.
  ASSERT_EQ(time1, (60*60*24));
}

// Test an empirical value taken from running dumpbin.exe on a DLL.
// (IMPORTANT: ran this *after* setting machine's time zone to GMT,
//  without daylight savings).
// 40AEE7AA time date stamp Sat May 22 05:39:54 2004
TEST(TimeTest, SystemTimeToInt32) {
  SYSTEMTIME system_time = {2004, 5, 6, 22, 5, 39, 54, 0};
  int32 time = SystemTimeToInt32(&system_time);
  ASSERT_EQ(time, 0x40AEE7AA);
}

// Test conversion between int32 and time64 values.
// By testing SystemTimeToInt32 above, we've already checked Time64ToInt32
// against empirical values, so it's okay to simply test back-and-forth
// conversion here.
TEST(TimeTest, Conversion) {
  // Simple checks when starting with int32 values, because time64 has more
  // precision.
  ASSERT_EQ(Time64ToInt32(Int32ToTime64(0x12345678)), 0x12345678);
  ASSERT_EQ(Time64ToInt32(Int32ToTime64(INT_MAX)), INT_MAX);
  ASSERT_EQ(Time64ToInt32(Int32ToTime64(0)), 0);

  // Extra conversions when going opposite direction because int32 has less
  // precision.
  ASSERT_EQ(Int32ToTime64(Time64ToInt32(Int32ToTime64(0x12345678))),
            Int32ToTime64(0x12345678));
  ASSERT_EQ(Int32ToTime64(Time64ToInt32(Int32ToTime64(INT_MAX))),
            Int32ToTime64(INT_MAX));
  ASSERT_EQ(Int32ToTime64(Time64ToInt32(Int32ToTime64(0))),
            Int32ToTime64(0));
}

void TimeToStringTest(FILETIME *ft, bool daylight_savings_time) {
  CTime t(*ft, daylight_savings_time);
  CString date1(t.FormatGmt(_T("%a, %d %b %Y %H:%M:%S GMT")));
  CString date2(ConvertTimeToGMTString(ft));

  ASSERT_STREQ(date1, date2);
}

TEST(TimeTest, TimeToStringTest) {
  bool daylight_savings_time = false;
  TIME_ZONE_INFORMATION tz;
  if (GetTimeZoneInformation(&tz) == TIME_ZONE_ID_DAYLIGHT) {
    daylight_savings_time = true;
  }

  FILETIME file_time;
  ::GetSystemTimeAsFileTime(&file_time);
  TimeToStringTest(&file_time, daylight_savings_time);

  uint64 t = FileTimeToTime64(file_time);

  // months
  for (int i = 0; i < 13; i++) {
    t += (24 * kHoursTo100ns) * 28;
    Time64ToFileTime(t, &file_time);
    TimeToStringTest(&file_time, daylight_savings_time);
  }

  // days
  for (int i = 0; i < 30; i++) {
    t += (24 * kHoursTo100ns);
    Time64ToFileTime(t, &file_time);
    TimeToStringTest(&file_time, daylight_savings_time);
  }

  // hours
  for (int i = 0; i < 24; i++) {
    t += (24 * kHoursTo100ns);
    Time64ToFileTime(t, &file_time);
    TimeToStringTest(&file_time, daylight_savings_time);
  }
}

TEST(TimeTest, RFC822TimeParsing) {
  SYSTEMTIME time = {0};
  ASSERT_TRUE(RFC822DateToSystemTime(_T("Mon, 16 May 2005 15:44:18 -0700"),
                                     &time,
                                     false));
  ASSERT_EQ(time.wYear , 2005);
  ASSERT_EQ(time.wMonth , 5);
  ASSERT_EQ(time.wDay , 16);
  ASSERT_EQ(time.wHour , 22);
  ASSERT_EQ(time.wMinute , 44);
  ASSERT_EQ(time.wSecond , 18);

  ASSERT_TRUE(RFC822DateToSystemTime(_T("Mon, 16 May 2005 15:44:18 -0700"),
                                     &time,
                                     true));
  ASSERT_EQ(time.wYear , 2005);
  ASSERT_EQ(time.wMonth , 5);
  ASSERT_EQ(time.wDay , 16);
  ASSERT_TRUE(time.wHour == 15 || time.wHour == 14);  // daylight saving time
  ASSERT_EQ(time.wMinute , 44);
  ASSERT_EQ(time.wSecond , 18);

  ASSERT_TRUE(RFC822DateToSystemTime(_T("Tue, 17 May 2005 02:56:18 +0400"),
                                     &time,
                                     false));
  ASSERT_EQ(time.wYear , 2005);
  ASSERT_EQ(time.wMonth , 5);
  ASSERT_EQ(time.wDay , 16);
  ASSERT_EQ(time.wHour , 22);
  ASSERT_EQ(time.wMinute , 56);
  ASSERT_EQ(time.wSecond , 18);

  ASSERT_TRUE(RFC822DateToSystemTime(_T("Tue, 17 May 2005 02:56:18 +0400"),
                                     &time,
                                     true));
  ASSERT_EQ(time.wYear , 2005);
  ASSERT_EQ(time.wMonth , 5);
  ASSERT_EQ(time.wDay , 16);
  ASSERT_TRUE(time.wHour == 15 || time.wHour == 14);  // daylight saving time
  ASSERT_EQ(time.wMinute , 56);
  ASSERT_EQ(time.wSecond , 18);
}

TEST(TimeTest, FileTimeToInt64) {
  {
  FILETIME file_time = {0};
  EXPECT_EQ(0, FileTimeToInt64(file_time));
  }

  {
  FILETIME file_time = {LONG_MAX, 0};
  EXPECT_EQ(LONG_MAX, FileTimeToInt64(file_time));
  }

  {
  FILETIME file_time = {ULONG_MAX, 0};
  EXPECT_EQ(ULONG_MAX, FileTimeToInt64(file_time));
  }

  {
  FILETIME file_time = {ULONG_MAX, ULONG_MAX};
  EXPECT_EQ(kuint64max, FileTimeToInt64(file_time));
  }
}

}  // namespace omaha

