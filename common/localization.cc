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
// localization.cpp
//
// Localization functions for date-time, strings, locales, and numbers

#include "omaha/common/localization.h"

#include <windows.h>  // SetThreadLocale
#include <mlang.h>
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"

namespace omaha {

// The maximum length for a date
#define kDateLengthMax 200

// The maximum length for chracter seperators
#define kDecimalSeparatorMaxLen 5

// Allow the unittest to override.
static LCID lcid_override = MAKELCID(-1, -1);

void SetLcidOverride(const LCID & lcid_new) {
  lcid_override = lcid_new;
}

// We should call this, it allows for our override
LCID GetLiveLcid() {
  if (lcid_override != MAKELCID(-1, -1))
    return lcid_override;

  return GetUserDefaultLCID();
}


CString ShowDateInternal(const time64 & t, const LCID & lcid,
                         // Either type needs to be 0 or format needs to be
                         // NULL; both cannot be set simultaneously:
                         const DWORD type, const TCHAR * format ) {
  SYSTEMTIME time = Time64ToLocalTime(t);
  TCHAR buf[kDateLengthMax] = {_T('\0')};
  int num = ::GetDateFormat(lcid, type, &time, format, buf, kDateLengthMax);
  ASSERT(num > 0, (_T("[localization::ShowDateInternal] - GetDateFormat ")
                   _T("failed")));

  return CString(buf);
}

CString ShowDateForLocale(const time64 & t, const LCID & lcid) {
  return ShowDateInternal(t, lcid, DATE_SHORTDATE, NULL);
}

CString ShowFormattedDateForLocale(const time64 & t, const LCID & lcid,
                                   const TCHAR * format) {
  return ShowDateInternal(t, lcid, 0, format);
}


CString ShowTimeInternal(const time64 & t, const LCID & lcid,
                         // Either type needs to be 0 or format needs to be
                         // NULL; both cannot be set simultaneously:
                         const DWORD type, const TCHAR * format) {
  ASSERT(IsValidTime(t), (_T("[localization::ShowTimeInternal - Invalid ")
                          _T("time %llu"), t));

  SYSTEMTIME time = Time64ToLocalTime(t);
  TCHAR buf[kDateLengthMax] = {_T('\0')};
  int num = ::GetTimeFormat(lcid, type, &time, format, buf, kDateLengthMax);
  ASSERT(num > 0, (_T("[localization::ShowTimeInternal - GetTimeFormat ")
                   _T("failed")));

  return CString(buf);
}

CString ShowTimeForLocale(const time64 & t, const LCID & lcid) {
  return ShowTimeInternal(t, lcid, TIME_NOSECONDS, NULL);
}

CString ShowFormattedTimeForLocale(const time64 & t, const LCID & lcid,
                                   const TCHAR * format) {
  return ShowTimeInternal(t, lcid, 0, format);
}

// Show the long date and time [ie - Tuesday, March 20, 2004 5:15pm]
CString ShowDateTimeForLocale(const time64 & t, const LCID & lcid) {
  return ShowDateForLocale(t, lcid) + _T(" ") + ShowTimeForLocale(t, lcid);
}

// Get the long data and time in a (US English) format for logging
CString ShowDateTimeForLogging(const time64 & t) {
  if (t == 0) {
     return CString();
  }
  const LCID lcid = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  return ShowDateTimeForLocale(t, lcid);
}

// Convert a number in string format to formatted string
static CString Show(const CString & in, const int decimal_places) {
  NUMBERFMT nf = {0};
  TCHAR decimal_seperator[8] = {_T('\0')};
  TCHAR thousands_seperator[8] = {_T('\0')};
  GetNumberFormatForLCID(GetLiveLcid(), &nf,
                         decimal_seperator, arraysize(decimal_seperator),
                         thousands_seperator, arraysize(thousands_seperator));
  nf.NumDigits = decimal_places;

  TCHAR buf[kDateLengthMax] = {_T('\0')};
  int num = GetNumberFormat(GetLiveLcid(),  // locale (current user locale)
    0,                                      // options
    in,                                     // input string (see MSDN for chars)
    &nf,                                    // formatting information
    buf,                                    // formatted string buffer
    kDateLengthMax);                        // size of buffer

  ASSERT(num > 0, (_T("GetNumberFormat failed: %s?"), in.GetString()));

  return CString(buf);
}

// If we have a formatted number containing a decimal, and we want it to be
// an int
CString TrimDecimal(const CString & in) {
  // Get the decimal seperator -- cache it as this is very slow
  static LCID last_user_default_lcid = MAKELCID(-1, -1);
  static TCHAR buf[kDecimalSeparatorMaxLen] = {_T('\0')};

  LCID current_lcid = GetLiveLcid();
  if (last_user_default_lcid != current_lcid) {
    int num = GetLocaleInfo(GetLiveLcid(),
                            LOCALE_SDECIMAL,
                            buf,
                            kDecimalSeparatorMaxLen);
    ASSERT(num > 0, (L"GetLocaleInfo(.., LOCALE_SDECIMAL, ..) failed?"));
    last_user_default_lcid = current_lcid;
  }

  CString sep(buf);

  // Trim it if necessary
  int pos = String_FindString(in, sep);
  if (pos != -1)
    return in.Left(pos);

  return in;
}

// Number Functions
// Changes the number into a user viewable format for the current locale

// TODO(omaha): Rename these functions into ShowNumberForLocale.
CString Show(const int i) {
  return TrimDecimal(Show(itostr(i), 0));
}

CString Show(const uint32 u) {
  return TrimDecimal(Show(itostr(u), 0));
}

CString Show(const double & d, const int decimal_places) {
  return Show(String_DoubleToString(d, decimal_places), decimal_places);
}

HRESULT SetLocaleToRfc1766(const TCHAR * rfc1766_locale) {
  ASSERT1(rfc1766_locale != NULL);

  // Convert the RFC 1766 locale (eg, "fr-CA" for Canadian French to a
  // Windows LCID (eg, 0x0c0c for Canadian French)
  CComPtr<IMultiLanguage2> pIM;
  RET_IF_FAILED(pIM.CoCreateInstance(__uuidof(CMultiLanguage)));

  LCID lcid = 0;
  CComBSTR rfc1766_locale_bstr(rfc1766_locale);
  RET_IF_FAILED(pIM->GetLcidFromRfc1766(&lcid, (BSTR)rfc1766_locale_bstr));

  return SetLocaleToLCID(lcid);
}

HRESULT SetLocaleToLCID(const LCID & lcid) {
  // Initialize the locales
  //   (in an attempt to cut down on our memory footprint, don't call
  //    the libc version of setlocale)
  if (!::SetThreadLocale(lcid)) {
    UTIL_LOG(LEVEL_ERROR, (_T("Unable to SetThreadLocale to lcid 0x%x"),
                           lcid));
    return E_FAIL;
  }

  return S_OK;
}

HRESULT GetLocaleAsLCID(LCID * lcid) {
  ASSERT1(lcid != NULL);

  *lcid = GetThreadLocale();
  return S_OK;
}

HRESULT GetLocaleAsRfc1766(CString * rfc1766_locale) {
  ASSERT1(rfc1766_locale != NULL);

  LCID lcid = 0;
  HRESULT hr = GetLocaleAsLCID(&lcid);
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IMultiLanguage2> pIM;
  RET_IF_FAILED(pIM.CoCreateInstance(__uuidof(CMultiLanguage)));

  CComBSTR bstr;
  RET_IF_FAILED(pIM->GetRfc1766FromLcid(lcid, &bstr));

  *rfc1766_locale = bstr;
  return hr;
}

HRESULT GetNumberFormatForLCID(const LCID & lcid, NUMBERFMT * fmt,
                               TCHAR * fmt_decimal_buf,
                               size_t decimal_buf_len,  // including null char
                               TCHAR * fmt_thousand_buf,
                               size_t thousand_buf_len) {  // including null
  ASSERT1(fmt);

  TCHAR buf[64] = {_T('\0')};
  size_t buf_len = arraysize(buf);

  HRESULT hr = S_OK;
  int retval = GetLocaleInfo(lcid, LOCALE_IDIGITS, buf, buf_len);

  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[localization::GetNumberFormatForLCID - ")
                             _T("Failed to load LOCALE_IDIGITS]")));
    hr = E_FAIL;
  } else {
    fmt->NumDigits = String_StringToInt(buf);
  }

  retval = GetLocaleInfo(lcid, LOCALE_ILZERO, buf, buf_len);
  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[App::Impl::InitializeLocaleSettings - ")
                             _T("Failed to load LOCALE_ILZERO]")));
    hr = E_FAIL;
  } else {
    fmt->LeadingZero = String_StringToInt(buf);
  }

  retval = GetLocaleInfo(lcid, LOCALE_INEGNUMBER, buf, buf_len);
  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[App::Impl::InitializeLocaleSettings - ")
                             _T("Failed to load LOCALE_INEGNUMBER]")));
    hr = E_FAIL;
  } else {
    fmt->NegativeOrder = String_StringToInt(buf);
  }

  retval = GetLocaleInfo(lcid, LOCALE_SGROUPING, buf, buf_len);
  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[App::Impl::InitializeLocaleSettings - ")
                             _T("Failed to load LOCALE_SGROUPING]")));
    hr = E_FAIL;
  } else {
    // A string terminated in ';0' is equivalent to the substring without
    // the ';0', so just truncate the ';0' from the string
    int semicolon_idx = String_ReverseFindChar(buf, _T(';'));
    if (retval > semicolon_idx && buf[semicolon_idx + 1] == _T('0')) {
      buf[semicolon_idx] = _T('\0');
    }

    if (String_FindChar(buf, _T(';')) != -1) {
      // NUMBERFMT only allows values 0-9 or 32 for number grouping.  If
      // this locale has variable-length grouping rules (as indicated by
      // the presence of ';[1-9]'), pass in the only variable-length
      // grouping rule NUMBERFMT understands: 32.  Note that '3;0' is
      // considered a fixed-length grouping rule and handled above.
      // This is a HACK.
      fmt->Grouping = 32;
    } else {
      fmt->Grouping = String_StringToInt(buf);
    }
  }

  // GetLocaleInfo doesn't write more than 4 chars for this field (per MSDN)
  retval = GetLocaleInfo(lcid, LOCALE_SDECIMAL, fmt_decimal_buf,
                         decimal_buf_len);
  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[App::Impl::InitializeLocaleSettings - ")
                             _T("Failed to load LOCALE_SDECIMAL]")));
    hr = E_FAIL;
  } else {
    fmt->lpDecimalSep = fmt_decimal_buf;
  }

  // GetLocaleInfo doesn't write more than 4 chars for this field (per MSDN)
  retval = GetLocaleInfo(lcid, LOCALE_STHOUSAND, fmt_thousand_buf,
                         thousand_buf_len);
  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[App::Impl::InitializeLocaleSettings - ")
                             _T("Failed to load LOCALE_STHOUSAND]")));
    hr = E_FAIL;
  } else {
    fmt->lpThousandSep = fmt_thousand_buf;
  }

  retval = GetLocaleInfo(lcid, LOCALE_INEGNUMBER, buf, buf_len);
  if (!retval) {
    CORE_LOG(LEVEL_WARNING, (_T("[App::Impl::InitializeLocaleSettings - ")
                             _T("Failed to load LOCALE_INEGNUMBER]")));
    hr = E_FAIL;
  } else {
    fmt->NegativeOrder = String_StringToInt(buf);
  }

  return hr;
}

}  // namespace omaha

