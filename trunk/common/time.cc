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

#include "omaha/common/time.h"

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

// Date Constants

#define kNumOfDays 7
#define kNumOfMonth 12

static const TCHAR kRFC822_DateDelimiters[]    = _T(" ,:");
static const TCHAR kRFC822_TimeDelimiter[]     = _T(":");
SELECTANY const TCHAR* kRFC822_Day[kNumOfDays] = {
  _T("Mon"),
  _T("Tue"),
  _T("Wed"),
  _T("Thu"),
  _T("Fri"),
  _T("Sat"),
  _T("Sun") };

SELECTANY const TCHAR* kRFC822_Month[kNumOfMonth] = {
  _T("Jan"),
  _T("Feb"),
  _T("Mar"),
  _T("Apr"),
  _T("May"),
  _T("Jun"),
  _T("Jul"),
  _T("Aug"),
  _T("Sep"),
  _T("Oct"),
  _T("Nov"),
  _T("Dec") };

struct TimeZoneInfo {
  const TCHAR* zone_name;
  int hour_dif;
};

SELECTANY TimeZoneInfo kRFC822_TimeZone[] = {
  { _T("UT"),  0 },
  { _T("GMT"), 0 },
  { _T("EST"), -5 },
  { _T("EDT"), -4 },
  { _T("CST"), -6 },
  { _T("CDT"), -5 },
  { _T("MST"), -7 },
  { _T("MDT"), -6 },
  { _T("PST"), -8 },
  { _T("PDT"), -7 },
  { _T("A"),   -1 },  // Military time zones
  { _T("B"),   -2 },
  { _T("C"),   -3 },
  { _T("D"),   -4 },
  { _T("E"),   -5 },
  { _T("F"),   -6 },
  { _T("G"),   -7 },
  { _T("H"),   -8 },
  { _T("I"),   -9 },
  { _T("K"),   -10 },
  { _T("L"),   -11 },
  { _T("M"),   -12 },
  { _T("N"),    1 },
  { _T("O"),    2 },
  { _T("P"),    3 },
  { _T("Q"),    4 },
  { _T("R"),    5 },
  { _T("S"),    6 },
  { _T("T"),    7 },
  { _T("U"),    8 },
  { _T("V"),    9 },
  { _T("W"),    10 },
  { _T("X"),    11 },
  { _T("Y"),    12 },
  { _T("Z"),    0 },
};

SELECTANY const TCHAR *days[] =
  { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };

SELECTANY const TCHAR *months[] = {
  L"Jan",
  L"Feb",
  L"Mar",
  L"Apr",
  L"May",
  L"Jun",
  L"Jul",
  L"Aug",
  L"Sep",
  L"Oct",
  L"Nov",
  L"Dec"
};

// NOTE: so long as the output is used internally only, no localization is
// needed here.
CString ConvertTimeToGMTString(const FILETIME *ft) {
  ASSERT(ft, (L""));

  CString s;
  SYSTEMTIME st;
  if (!FileTimeToSystemTime(ft, &st)) {
    return L"";
  }

  // same as FormatGmt(_T("%a, %d %b %Y %H:%M:%S GMT"));
  s.Format(NOTRANSL(L"%s, %02d %s %d %02d:%02d:%02d GMT"), days[st.wDayOfWeek],
    st.wDay, months[st.wMonth-1], st.wYear, st.wHour, st.wMinute, st.wSecond);
  return s;
}

time64 ConvertTime16ToTime64(uint16 time16) {
  return time16 * kTimeGranularity + kStart100NsTime;
}

uint16 ConvertTime64ToTime16(time64 time) {
  ASSERT1(time >= kStart100NsTime);

  time64 t64 = (time - kStart100NsTime) / kTimeGranularity;
  ASSERT1(t64 <= kTime16Max);

  return static_cast<uint16>(t64);
}

time64 TimeTToTime64(const time_t& old_value) {
  FILETIME file_time;
  TimeTToFileTime(old_value, &file_time);
  return FileTimeToTime64(file_time);
}

#ifdef _DEBUG
void ComputeStartTime() {
    SYSTEMTIME start_system_time = kStartSystemTime;
    time64 start_100ns_time = SystemTimeToTime64(&start_system_time);
    UTIL_LOG(L1, (_T("posting list starting time = %s\n"),
                  String_Int64ToString(start_100ns_time, 10)));
}
#endif

// Time management

// Allow the unittest to override.
static time64 time_override = 0;

// #ifdef UNITTEST
void SetTimeOverride(const time64 & time_new) {
  time_override = time_new;
}

// #endif

time64 GetCurrent100NSTime() {
  if (time_override != 0)
    return time_override;

  // In order gte the 100ns time we shouldn't use SystemTime
  // as it's granularity is 1 ms. Below is the correct implementation.
  // On the other hand the system clock granularity is 15 ms, so we
  // are not gaining much by having the timestamp in nano-sec
  // If we decide to go with ms, divide "time64 time" by 10000
  // SYSTEMTIME sys_time;
  // GetLocalTime(&sys_time);
  // return SystemTimeToTime64(&sys_time);

  // get the current time in 100-nanoseconds intervals
  FILETIME file_time;
  ::GetSystemTimeAsFileTime(&file_time);

  time64 time = FileTimeToTime64(file_time);
  return time;
}

time64 SystemTimeToTime64(const SYSTEMTIME* sys_time) {
  ASSERT1(sys_time);

  FILETIME file_time;
  SetZero(file_time);

  if (!::SystemTimeToFileTime(sys_time, &file_time)) {
    UTIL_LOG(LE,
             (_T("[SystemTimeToTime64 - failed to SystemTimeToFileTime][0x%x]"),
              HRESULTFromLastError()));
    return 0;
  }

  return FileTimeToTime64(file_time);
}

// returns a value compatible with EXE/DLL timestamps
// and the C time() function
// NOTE: behavior is independent of wMilliseconds value
int32 SystemTimeToInt32(const SYSTEMTIME *sys_time) {
  ASSERT(sys_time, (L""));

  time64 t64 = SystemTimeToTime64(sys_time);
  int32 t32 = 0;

  if (t64 != 0) {
    t32 = Time64ToInt32(t64);
  }
  return t32;
}

int32 Time64ToInt32(const time64 & time) {
  // convert to 32-bit format
  // time() (32-bit) measures seconds since 1970/01/01 00:00:00 (UTC)
  // FILETIME (64-bit) measures 100-ns intervals since 1601/01/01 00:00:00 (UTC)

  // seconds between 1601 and 1970
  time64 t32 = (time / kSecsTo100ns) -
               ((time64(60*60*24) * time64(365*369 + 89)));
  ASSERT(t32 == (t32 & 0x7FFFFFFF), (L""));  // make sure it fits

  // cast at the end (avoids overflow/underflow when computing 32-bit value)
  return static_cast<int32>(t32);
}

time64 Int32ToTime64(const int32 & time) {
  // convert to 64-bit format
  // time() (32-bit) measures seconds since 1970/01/01 00:00:00 (UTC)
  // FILETIME (64-bit) measures 100-ns intervals since 1601/01/01 00:00:00 (UTC)

  // seconds between 1601 and 1970
  time64 t64 = (static_cast<time64>(time) +
               (time64(60*60*24) * time64(365*369 + 89))) * kSecsTo100ns;
  return t64;
}

// TODO(omaha): The next 2 functions can fail if FileTimeToLocalFileTime or
// FileTimeToSystemTime fails.
// Consider having it return a HRESULT. Right now if FileTimeToSystemTime fails,
// it returns an undefined value.

// Convert a uint to a genuine systemtime
SYSTEMTIME Time64ToSystemTime(const time64& time) {
  FILETIME file_time;
  SetZero(file_time);
  Time64ToFileTime(time, &file_time);

  SYSTEMTIME sys_time;
  SetZero(sys_time);
  if (!FileTimeToSystemTime(&file_time, &sys_time)) {
    UTIL_LOG(LE, (_T("[Time64ToSystemTime]")
                  _T("[failed to FileTimeToSystemTime][0x%x]"),
                  HRESULTFromLastError()));
  }

  return sys_time;
}


// Convert a uint to a genuine localtime
// Should ONLY be used for display, since internally we use only UTC
SYSTEMTIME Time64ToLocalTime(const time64& time) {
  FILETIME file_time;
  SetZero(file_time);
  Time64ToFileTime(time, &file_time);

  FILETIME local_file_time;
  SetZero(local_file_time);
  if (!FileTimeToLocalFileTime(&file_time, &local_file_time)) {
    UTIL_LOG(LE, (_T("[Time64ToLocalTime]")
                  _T("[failed to FileTimeToLocalFileTime][0x%x]"),
                  HRESULTFromLastError()));
  }

  SYSTEMTIME local_time;
  SetZero(local_time);
  if (!FileTimeToSystemTime(&local_file_time, &local_time)) {
    UTIL_LOG(LE, (_T("[Time64ToLocalTime]")
                  _T("[failed to FileTimeToSystemTime][0x%x]"),
                  HRESULTFromLastError()));
  }

  return local_time;
}

time64 FileTimeToTime64(const FILETIME & file_time) {
  return static_cast<time64>(
      file_time.dwHighDateTime) << 32 | file_time.dwLowDateTime;
}

void Time64ToFileTime(const time64 & time, FILETIME *ft) {
  ASSERT(ft, (L""));

  ft->dwHighDateTime = static_cast<DWORD>(time >> 32);
  ft->dwLowDateTime = static_cast<DWORD>(time & 0xffffffff);
}

// Convert from FILETIME to time_t
time_t FileTimeToTimeT(const FILETIME& file_time) {
  return static_cast<time_t>(
      (FileTimeToTime64(file_time) - kTimeTConvValue) / kSecsTo100ns);
}

// Convert from time_t to FILETIME
void TimeTToFileTime(const time_t& time, FILETIME* file_time) {
  ASSERT1(file_time);

  LONGLONG ll = Int32x32To64(time, kSecsTo100ns) + kTimeTConvValue;
  file_time->dwLowDateTime = static_cast<DWORD>(ll);
  file_time->dwHighDateTime = static_cast<DWORD>(ll >> 32);
}

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
                            bool ret_local_time) {
  ASSERT(str_RFC822_date != NULL, (L""));
  ASSERT(psys_time != NULL, (L""));

  CString str_date = str_RFC822_date;
  CString str_token;
  int cur_pos = 0;

  str_token= str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
  if (str_token == "")
    return false;

  int i = 0;
  for (i = 0; i < kNumOfDays; i++) {
    if (str_token == kRFC822_Day[i]) {
      str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
      if (str_token == "")
        return false;
      break;
    }
  }

  int day = String_StringToInt(str_token);
  if (day < 0 || day > 31)
    return false;

  str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
  if (str_token == "")
    return false;

  int month = -1;
  for (i = 0; i < kNumOfMonth; i++) {
    if (str_token == kRFC822_Month[i]) {
      month = i+1;  // month is 1 based number
      break;
    }
  }
  if (month == -1)  // month not found
    return false;

  str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
  if (str_token == "")
    return false;

  int year = String_StringToInt(str_token);
  if (year < 100)  // two digit year format, convert to 1950 - 2050 range
    if (year < 50)
      year += 2000;
    else
      year += 1900;

  str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
  if (str_token == "")
    return false;

  int hour = String_StringToInt(str_token);
  if (hour < 0 || hour > 23)
    return false;

  str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
  if (str_token == "")
    return false;

  int minute = String_StringToInt(str_token);
  if (minute < 0 || minute > 59)
    return false;

  str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
  if (str_token == "")
    return false;

  int second = 0;
  // distingushed between XX:XX and XX:XX:XX time formats
  if (str_token.GetLength() == 2 &&
      String_IsDigit(str_token[0]) &&
      String_IsDigit(str_token[1])) {
    second = String_StringToInt(str_token);
    if (second < 0 || second > 59)
      return false;

    str_token = str_date.Tokenize(kRFC822_DateDelimiters, cur_pos);
    if (str_token == "")
      return false;
  }

  int bias = 0;
  if (str_token[0] == '+' ||
      str_token[0] == '-' ||
      String_IsDigit(str_token[0])) {  // numeric format
    int zone = String_StringToInt(str_token);

    // zone is in HHMM format, need to convert to the number of minutes
    bias = (zone / 100) * 60 + (zone % 100);
  } else {  // text format
    for (i = 0; i < sizeof(kRFC822_TimeZone) / sizeof(TimeZoneInfo); i++)
      if (str_token == kRFC822_TimeZone[i].zone_name) {
        bias = kRFC822_TimeZone[i].hour_dif * 60;
        break;
      }
  }

  SYSTEMTIME mail_time;
  memset(&mail_time, 0, sizeof(mail_time));

  mail_time.wYear   = static_cast<WORD>(year);
  mail_time.wMonth  = static_cast<WORD>(month);
  mail_time.wDay    = static_cast<WORD>(day);
  mail_time.wHour   = static_cast<WORD>(hour);
  mail_time.wMinute = static_cast<WORD>(minute);
  mail_time.wSecond = static_cast<WORD>(second);

  // TzSpecificLocalTimeToSystemTime() is incompatible with Win 2000,
  // convert time manually here
  time64 time_64 = SystemTimeToTime64(&mail_time);
  time_64 = time_64 - (bias*kMinsTo100ns);

  *psys_time = Time64ToSystemTime(time_64);

  if (ret_local_time) {
    TIME_ZONE_INFORMATION local_time_zone_info;
    SYSTEMTIME universal_time = *psys_time;

    if (GetTimeZoneInformation(&local_time_zone_info) == TIME_ZONE_ID_INVALID) {
      return false;
    }
    if (!SystemTimeToTzSpecificLocalTime(&local_time_zone_info,
                                    &universal_time,
                                    psys_time)) {
      return false;
    }
  }
  return true;
}

}  // namespace omaha

