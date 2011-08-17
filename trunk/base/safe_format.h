// Copyright 2010 Google Inc.
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
// As of Sept 2010, the implementation of Format() in ATL's CStringT uses a
// fixed internal buffer of 1024 bytes; any output beyond that point will
// be silently truncated.  (AppendFormat() is affected by the same bug.)
// A bug has been filed with Microsoft; in the meantime, we provide the
// following safer implementations of Format/AppendFormat.  These functions
// currently support up to MAX_INT characters and will return an HRESULT
// error code on overflow or invalid parameters.

#ifndef OMAHA_BASE_SAFE_FORMAT_H_
#define OMAHA_BASE_SAFE_FORMAT_H_

#include <atlstr.h>
#include <strsafe.h>

namespace omaha {

void SafeCStringWFormatV(CStringW* dest_str,
                         LPCWSTR format_str,
                         va_list arg_list);

void SafeCStringAFormatV(CStringA* dest_str,
                         LPCSTR format_str,
                         va_list arg_list);

void SafeCStringWFormat(CStringW* dest_str, LPCWSTR format_str, ...);

void SafeCStringAFormat(CStringA* dest_str, LPCSTR format_str, ...);

void SafeCStringWAppendFormat(CStringW* dest_str, LPCWSTR format_str, ...);

void SafeCStringAAppendFormat(CStringA* dest_str, LPCSTR format_str, ...);

}  // namespace omaha

#ifdef UNICODE
#define SafeCStringFormatV       SafeCStringWFormatV
#define SafeCStringFormat        SafeCStringWFormat
#define SafeCStringAppendFormat  SafeCStringWAppendFormat
#else
#define SafeCStringFormatV       SafeCStringAFormatV
#define SafeCStringFormat        SafeCStringAFormat
#define SafeCStringAppendFormat  SafeCStringAAppendFormat
#endif  // UNICODE

#endif  // OMAHA_BASE_SAFE_FORMAT_H_
