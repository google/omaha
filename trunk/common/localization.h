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
//
// localization.h
//
// Localization functions for date-time, strings, locales, and numbers

#ifndef OMAHA_COMMON_LOCALIZATION_H__
#define OMAHA_COMMON_LOCALIZATION_H__

#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/commontypes.h"

namespace omaha {

// Allows us to override LCIDs for unittests
void SetLcidOverride(const LCID & lcid_new);


//
// Date-time Functions
//

// Show the time in the specified locale's default format.
// If you want to use the user's default format, use LOCALE_USER_DEFAULT
// for the locale [eg - "5:15:34 pm" is the US default]
CString ShowDateForLocale(const time64 & t, const LCID & lcid);

// Show the time in the specified format for the specified locale.
// If you want to use the user's default format, use LOCALE_USER_DEFAULT
// for the locale [eg - "5:15:34 pm" is the US default]
CString ShowFormattedDateForLocale(const time64 & t, const LCID & lcid,
                                   const TCHAR * format);


// Show the time in the specified locale's default format.
// If you want to use the user's default format, use LOCALE_USER_DEFAULT
// for the locale [eg - "5:15:34 pm" is the US default]
CString ShowTimeForLocale(const time64 & t, const LCID & lcid);

// Show the time in the specified format for the specified locale.
// If you want to use the user's default format, use LOCALE_USER_DEFAULT
// for the locale [eg - "5:15:34 pm" is the US default]
CString ShowFormattedTimeForLocale(const time64 & t, const LCID & lcid,
                                   const TCHAR * format);

// Show the long date and time [ie - Tuesday, March 20, 2004 5:15pm]
CString ShowDateTimeForLocale(const time64 & t, const LCID & lcid);

// Get the long data and time in a (US English) format for logging
CString ShowDateTimeForLogging(const time64 & t);

//
// Number Functions
//

// Changes the number into a user viewable format for the current locale
CString Show(const int i);
CString Show(const uint32 u);
CString Show(const double & d, const int decimal_places);


//
// Locale Name / LCID / RFC 1766 conversions
//
HRESULT SetLocaleToRfc1766(const TCHAR * rfc1766_locale);
HRESULT SetLocaleToLCID(const LCID & lcid);

HRESULT GetLocaleAsLCID(LCID * lcid);
HRESULT GetLocaleAsRfc1766(CString * rfc1766_locale);

HRESULT GetNumberFormatForLCID(const LCID & lcid, NUMBERFMT * fmt,
                               TCHAR * fmt_decimal_buf,
                               size_t decimal_buf_len,
                               TCHAR * fmt_thousand_buf,
                               size_t thousand_buf_len);

}  // namespace omaha

#endif  // OMAHA_COMMON_LOCALIZATION_H__
