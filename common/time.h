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
// Time functions

#ifndef OMAHA_COMMON_TIME_H__
#define OMAHA_COMMON_TIME_H__

#include <windows.h>
#include <atlstr.h>
#include "omaha/common/commontypes.h"

namespace omaha {

#define kMicrosecsTo100ns (10ULL)
#define kMillisecsTo100ns (10000ULL)
#define kSecsTo100ns (1000 * kMillisecsTo100ns)
#define kMinsTo100ns (60 * kSecsTo100ns)
#define kHoursTo100ns (60 * kMinsTo100ns)
#define kDaysTo100ns (24 * kHoursTo100ns)

// Jan 1 1980 was tuesday (day 2)
#define kStartSystemTime {1980, 1, 2, 1, 0, 0, 0, 0}

// this is Jan 1 1980 in time64
#define kStart100NsTime (119600064000000000uI64)
#define kTimeGranularity (kDaysTo100ns)

// 2^15-1 because we use signed delta times
#define kTime16Max ((1 << 15) - 1)

// Constant value used in conversion between FILETIME and time_t
// It is the time difference between January 1, 1601 and January 1, 1970
#define kTimeTConvValue (116444736000000000)

time64 ConvertTime16ToTime64(uint16 time16);
uint16 ConvertTime64ToTime16(time64 time);

#ifdef _DEBUG
void ComputeStartTime();
#endif

uint64 GetCurrent100NSTime();

// Note - these return 0 if we can't convert the time
time64 SystemTimeToTime64(const SYSTEMTIME *sys_time);

// Conversions to/from values compatible with
// EXE/DLL timestamps and the C time() function
// NOTE: behavior is independent of wMilliseconds value
int32  SystemTimeToInt32(const SYSTEMTIME *sys_time);
int32  Time64ToInt32(const time64 & time);
time64 Int32ToTime64(const int32 & time);
time64 TimeTToTime64(const time_t& old_value);

// Returns the system time in GMT
SYSTEMTIME Time64ToSystemTime(const time64 & time);

// Returns the system time in the computer's time zone
SYSTEMTIME Time64ToLocalTime(const time64 & time);

// Returns the UTC (system) time given the local time
SYSTEMTIME LocalTimeToSystemTime(const SYSTEMTIME *local_time);

// This returns a standard formatted string that represents
// the UTC time corresponding to 'ft'.  This is suitable for use
// in e.g. HTTP headers.
//
// @note IMPORTANT!  This does not return a localized string - it's
// always in English.  The string returned is intended for use in
// machine-readable contexts, i.e. HTTP headers and thus should not
// be localized.
CString ConvertTimeToGMTString(const FILETIME *ft);

// Convert to and from FileTime
time64 FileTimeToTime64(const FILETIME & file_time);
void Time64ToFileTime(const time64 & time, FILETIME *ft);

void SetTimeOverride(const time64 & time_new);

// Convert from FILETIME to time_t
time_t FileTimeToTimeT(const FILETIME& file_time);

// Convert from time_t to FILETIME
void TimeTToFileTime(const time_t& time, FILETIME* file_time);

// Parses RFC 822 Date/Time format
//    5.  DATE AND TIME SPECIFICATION
//     5.1.  SYNTAX
//
//     date-time   =  [ day "," ] date time        ; dd mm yy
//                                                 ;  hh:mm:ss zzz
//     day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
//                 /  "Fri"  / "Sat" /  "Sun"
//
//     date        =  1*2DIGIT month 2DIGIT        ; day month year
//                                                 ;  e.g. 20 Jun 82
//
//     month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
//                 /  "May"  /  "Jun" /  "Jul"  /  "Aug"
//                 /  "Sep"  /  "Oct" /  "Nov"  /  "Dec"
//
//     time        =  hour zone                    ; ANSI and Military
//
//     hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
//                                                 ; 00:00:00 - 23:59:59
//
//     zone        =  "UT"  / "GMT"                ; Universal Time
//                                                 ; North American : UT
//                 /  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
//                 /  "CST" / "CDT"                ;  Central:  - 6/ - 5
//                 /  "MST" / "MDT"                ;  Mountain: - 7/ - 6
//                 /  "PST" / "PDT"                ;  Pacific:  - 8/ - 7
//                 /  1ALPHA                       ; Military: Z = UT;
//                                                 ;  A:-1; (J not used)
//                                                 ;  M:-12; N:+1; Y:+12
//                 / ( ("+" / "-") 4DIGIT )        ; Local differential
//                                                 ;  hours+min. (HHMM)
// return local time if ret_local_time == true,
// return time is GMT / UTC time otherwise
bool RFC822DateToSystemTime(const TCHAR* str_RFC822_date,
                            SYSTEMTIME* psys_time,
                            bool ret_local_time);

// TODO(omaha): overlap in functionality with FileTimeToTime64. Consider
// removing this one.
inline int64 FileTimeToInt64(const FILETIME& filetime) {
  LARGE_INTEGER large_int = {filetime.dwLowDateTime, filetime.dwHighDateTime};
  return large_int.QuadPart;
}

}  // namespace omaha

#endif  // OMAHA_COMMON_TIME_H__
