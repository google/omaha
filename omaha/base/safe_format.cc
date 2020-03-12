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

#include "omaha/base/safe_format.h"
#include <intsafe.h>
#include <algorithm>
#include "omaha/base/debug.h"

namespace omaha {

namespace {

// Define a templated wrapper for StringCchVPrintf_() that will call the
// appropriate W/A version based on template parameter.
template <typename CharType>
HRESULT InternalStringCchVPrintf(CharType* dest_buffer,
                                 size_t dest_size,
                                 const CharType* format_str,
                                 va_list arg_list);

template <>
HRESULT InternalStringCchVPrintf<char>(char* dest_buffer,
                                       size_t dest_size,
                                       const char* format_str,
                                       va_list arg_list) {
  return ::StringCchVPrintfA(dest_buffer, dest_size, format_str, arg_list);
}

template <>
HRESULT InternalStringCchVPrintf<wchar_t>(wchar_t* dest_buffer,
                                          size_t dest_size,
                                          const wchar_t* format_str,
                                          va_list arg_list) {
  return ::StringCchVPrintfW(dest_buffer, dest_size, format_str, arg_list);
}

// Define a templated wrapper for strlen() that will call the appropriate
// W/A version based on template parameter.
template <typename CharType>
size_t InternalStrlen(const CharType* str);

template <>
size_t InternalStrlen<char>(const char* str) {
  return ::strlen(str);
}

template <>
size_t InternalStrlen<wchar_t>(const wchar_t* str) {
  return ::wcslen(str);
}

// InternalCStringVPrintf() wraps InternalStringCchVPrintf() to accept a
// CStringT as the output parameter and resize it until the latter succeeds
// or we hit the StrSafe.h limit.
template <typename CharType, typename CharTraits>
HRESULT InternalCStringVPrintf(
               ATL::CStringT<CharType, CharTraits>& dest_str,
               const CharType* format_str,
               va_list arg_list) {
  size_t buf_length = std::max(InternalStrlen(format_str),
                               static_cast<size_t>(256));

  if (buf_length > INT_MAX) {
    return E_INVALIDARG;
  }

  for (;;) {
    CStrBufT<CharType> str_buf(dest_str, static_cast<int>(buf_length));
    HRESULT hr = InternalStringCchVPrintf(static_cast<CharType*>(str_buf),
                                    buf_length - 1,
                                    format_str,
                                    arg_list);
    if (hr != STRSAFE_E_INSUFFICIENT_BUFFER) {
      return hr;
    }
    if (buf_length >= STRSAFE_MAX_CCH) {
      return STRSAFE_E_INVALID_PARAMETER;
    }
    buf_length = std::min(buf_length * 2, static_cast<size_t>(STRSAFE_MAX_CCH));
  }
}

// InternalCStringVPrintf() will have an overflow bug if STRSAFE_MAX_CCH ever
// becomes larger than MAX_SIZE_T / 2.  Ensure at compile time that this is so.
COMPILE_ASSERT(STRSAFE_MAX_CCH <= SIZE_MAX / 2, strsafe_limit_has_changed);

}  // namespace

// Define the non-templated API calls.

void SafeCStringWFormatV(CStringW* dest_str,
                         LPCWSTR format_str,
                         va_list arg_list) {
  ASSERT1(dest_str);
  ASSERT1(format_str);
  VERIFY_SUCCEEDED(InternalCStringVPrintf(*dest_str, format_str, arg_list));
}

void SafeCStringAFormatV(CStringA* dest_str,
                         LPCSTR format_str,
                         va_list arg_list) {
  ASSERT1(dest_str);
  ASSERT1(format_str);
  VERIFY_SUCCEEDED(InternalCStringVPrintf(*dest_str, format_str, arg_list));
}

void SafeCStringWFormat(CStringW* dest_str, LPCWSTR format_str, ...) {
  ASSERT1(dest_str);
  ASSERT1(format_str);

  va_list arg_list;
  va_start(arg_list, format_str);
  VERIFY_SUCCEEDED(InternalCStringVPrintf(*dest_str, format_str, arg_list));
  va_end(arg_list);
}

void SafeCStringAFormat(CStringA* dest_str, LPCSTR format_str, ...) {
  ASSERT1(dest_str);
  ASSERT1(format_str);

  va_list arg_list;
  va_start(arg_list, format_str);
  VERIFY_SUCCEEDED(InternalCStringVPrintf(*dest_str, format_str, arg_list));
  va_end(arg_list);
}

void SafeCStringWAppendFormat(CStringW* dest_str, LPCWSTR format_str, ...) {
  ASSERT1(dest_str);
  ASSERT1(format_str);

  va_list arg_list;
  va_start(arg_list, format_str);

  CStringW append_str;
  VERIFY_SUCCEEDED(InternalCStringVPrintf(append_str, format_str, arg_list));
  dest_str->Append(append_str);

  va_end(arg_list);
}

void SafeCStringAAppendFormat(CStringA* dest_str, LPCSTR format_str, ...) {
  ASSERT1(dest_str);
  ASSERT1(format_str);

  va_list arg_list;
  va_start(arg_list, format_str);

  CStringA append_str;
  VERIFY_SUCCEEDED(InternalCStringVPrintf(append_str, format_str, arg_list));
  dest_str->Append(append_str);

  va_end(arg_list);
}

}  // namespace omaha

