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

#include "omaha/base/string.h"

#include <cstddef>
#include <cstdlib>
#include <intsafe.h>
#include <wininet.h>        // For INTERNET_MAX_URL_LENGTH.
#include <algorithm>
#include <string>
#include <memory>

#include "omaha/base/commontypes.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"

using std::string;

namespace omaha {

namespace {
// Testing shows that only the following ASCII characters are
// considered spaces by GetStringTypeA: 9-13, 32, 160.
// Rather than call GetStringTypeA with no locale, as we used to,
// we look up the values directly in a precomputed array.

SELECTANY byte spaces[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  // 0-9
  1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  // 10-19
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20-29
  0, 0, 1, 0, 0, 0, 0, 0, 0, 0,  // 30-39
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40-49
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 50-59
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 60-69
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 70-79
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80-89
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90-99
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 100-109
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 110-119
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 120-129
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 130-139
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 140-149
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 150-159
  1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160-169
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 170-179
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 180-189
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 190-199
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 200-209
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 210-219
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 220-229
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 230-239
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 240-249
  0, 0, 0, 0, 0, 1,              // 250-255
};
}  // namespace

const TCHAR* const kFalse = _T("false");
const TCHAR* const kTrue  = _T("true");

bool IsSpaceW(WCHAR c) {
  // GetStringTypeW considers these characters to be spaces:
  // 9-13, 32, 133, 160, 5760, 8192-8203, 8232, 8233, 12288
  if (c < 256)
    return (c == 133 || IsSpaceA((char) (c & 0xff)));

  return (c >= 8192 && c <= 8203) || c == 8232 ||
    c == 8233 || c == 12288;
}

bool IsSpaceA(char c) {
  return spaces[static_cast<unsigned char>(c)] == 1;
}

void MakeLowerCString(CString & s) {
  int len = s.GetLength();
  String_FastToLower(s.GetBuffer());
  s.ReleaseBufferSetLength(len);
}

int Trim(TCHAR *s) {
  ASSERT(s, (L""));

  // First find end of leading spaces
  TCHAR *start = s;
  while (*start) {
    if (!IsSpace(*start))
      break;
    ++start;
  }

  // Now search for the end, remembering the start of the last spaces
  TCHAR *end = start;
  TCHAR *last_space = end;
  while (*end) {
    if (!IsSpace(*end))
      last_space = end + 1;
    ++end;
  }

  // Copy the part we want
  ptrdiff_t len = last_space - start;
  // lint -e{802}  Conceivably passing a NULL pointer
  memmove(s, start, len * sizeof(TCHAR));

  // 0 terminate
  s[len] = 0;

  // TODO(portability): this conversion is unsafe.
  return static_cast<int>(len);
}

void TrimString(CString& s, const TCHAR* delimiters) {
  s = s.Trim(delimiters);
}

// A block of text to separate lines, and back
void TextToLines(const CString& text, const TCHAR* delimiter, std::vector<CString>* lines) {
  ASSERT(delimiter, (L""));
  ASSERT(lines, (L""));

  // TODO(portability): this conversion is not safe.
  const int delimiter_len = static_cast<int>(::lstrlen(delimiter));
  ASSERT1(delimiter_len <= INT_MAX);

  int b = 0;
  int e = 0;

  for (b = 0; e != -1 && b < text.GetLength(); b = e + delimiter_len) {
    e = text.Find(delimiter, b);
    if (e != -1) {
      if (e - b > 0) {
        lines->push_back(text.Mid(b, e - b));
      } else {
        lines->push_back(_T(""));
      }
    } else {
      lines->push_back(text.Mid(b));
    }
  }
}

void LinesToText(const std::vector<CString>& lines, const TCHAR* delimiter, CString* text) {
  ASSERT(delimiter, (L""));
  ASSERT(text, (L""));

  size_t delimiter_len = ::lstrlen(delimiter);
  size_t len = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    len += lines[i].GetLength() + delimiter_len;
  }

  ASSERT1(len <= INT_MAX);

  // TODO(portability): cast is unsafe.
  text->Empty();
  text->Preallocate(static_cast<int>(len));
  for (std::vector<CString>::size_type i = 0; i < lines.size(); ++i) {
    text->Append(lines[i]);
    if (delimiter_len) {
      text->Append(delimiter);
    }
  }
}

// Take 1 single hexadecimal "digit" (as a character) and return its decimal value
// Returns -1 if given invalid hex digit
int HexDigitToDec(const TCHAR digit) {
  if (digit >= L'A' && digit <= L'F')
    return 10 + (digit - L'A');
  else if (digit >= L'a' && digit <= L'f')
    return 10 + (digit - L'a');
  else if (digit >= L'0' && digit <= L'9')
    return (digit - L'0');
  else
    return -1;
}

// Convert the 2 hex chars at positions <pos> and <pos>+1 in <s> to a char (<char_out>)
// Note: scanf was giving me troubles, so here's the manual version
// Extracted char gets written to <char_out>, which must be allocated by
// the caller; return true on success or false if parameters are incorrect
// or string does not have 2 hex digits at the specified position
// NOTE: <char_out> is NOT a string, just a pointer to a char for the result
bool ExtractChar(const CString & s, int pos, unsigned char * char_out) {
  // char_out may be NULL

  if (s.GetLength() < pos + 1) {
    return false;
  }

  if (pos < 0 || NULL == char_out) {
    ASSERT(0, (_T("invalid params: pos<0 or char_out is NULL")));
    return false;
  }

  TCHAR c1 = s.GetAt(pos);
  TCHAR c2 = s.GetAt(pos+1);

  int p1 = HexDigitToDec(c1);
  int p2 = HexDigitToDec(c2);

  if (p1 == -1 || p2 == -1) {
    return false;
  }

  *char_out = (unsigned char)(p1 * 16 + p2);
  return true;
}

WCHAR *ToWide (const char *s, int len) {
    ASSERT (s, (L""));
    WCHAR *w = new WCHAR [len+1]; if (!w) { return NULL; }
    // int rc = MultiByteToWideChar (CP_ACP, 0, s.GetString(), (int)s.GetLength()+1, w, s.GetLength()+1);
    // TODO(omaha): why would it ever be the case that rc > len?
    int rc = MultiByteToWideChar (CP_ACP, 0, s, len, w, len);
    if (rc > len) { delete [] w; return NULL; }
    // ASSERT (rc <= len, (L""));
    w[rc]=L'\0';
    return w;
}

// Converting the Ansi Multibyte String into unicode string. The multibyte
// string is encoded using the specified codepage.
// The code is pretty much like the U2W function, except the codepage can be
// any valid windows CP.
BOOL AnsiToWideString(const char *from, int length, UINT codepage, CString *to) {
  ASSERT(from, (L""));
  ASSERT(to, (L""));
  ASSERT1(length >= -1);
  // Figure out how long the string is
  int req_chars = MultiByteToWideChar(codepage, 0, from, length, NULL, 0);

  if (req_chars <= 0) {
    UTIL_LOG(LEVEL_WARNING, (_T("MultiByteToWideChar Failed ")));
    *to = AnsiToWideString(from, length);
    return FALSE;
  }

  TCHAR *buffer = to->GetBufferSetLength(req_chars);
  int conv_chars = MultiByteToWideChar(codepage, 0, from, length, buffer, req_chars);
  if (conv_chars == 0) {
    UTIL_LOG(LEVEL_WARNING, (_T("MultiByteToWideChar Failed ")));
    to->ReleaseBuffer(0);
    *to = AnsiToWideString(from, length);
    return FALSE;
  }

  // Something truly horrible happened.
  ASSERT (req_chars == conv_chars, (L"MBToWide returned unexpected value: GetLastError()=%d",GetLastError()));
  // If length was inferred, conv_chars includes the null terminator.
  // Adjust the length here to remove null termination,
  // because we use the length-qualified CString constructor,
  // which automatically adds null termination given an unterminated array.
  if (-1 == length) { --conv_chars; }
  to->ReleaseBuffer(conv_chars);
  return TRUE;
}

// CStringW(const char* from) did not cast all character properly
// so we write our own.
CString AnsiToWideString(const char *from, int length) {
  ASSERT(from, (L""));
  ASSERT1(length >= -1);
  if (length < 0) {
    // TODO(portability): cast is unsafe.
    length = static_cast<int>(strlen(from));
  }
  CString to;
  TCHAR *buffer = to.GetBufferSetLength(length);
  for (int i = 0; i < length; ++i)
      buffer[i] = static_cast<UINT8>(from[i]);
  to.ReleaseBuffer(length);
  return to;
}


// Transform a unicode string into UTF8, as represented in an ASCII string
CStringA WideToUtf8(const CString& w) {
  // Add a cutoff. If it's all ascii, convert it directly
  const TCHAR* input = static_cast<const TCHAR*>(w.GetString());
  int input_len = w.GetLength(), i;
  for (i = 0; i < input_len; ++i) {
    if (input[i] > 127) {
      break;
    }
  }

  // If we made it to the end without breaking, then it's all ANSI, so do a quick convert
  if (i == input_len) {
    return WideToAnsiDirect(w);
  }

  // Figure out how long the string is
  int req_bytes = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);

  std::unique_ptr<char[]> utf8_buffer(new char[req_bytes]);

  int conv_bytes = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, utf8_buffer.get(), req_bytes, NULL, NULL);
  ASSERT1(req_bytes == conv_bytes);

  // conv_bytes includes the null terminator, when we read this in, don't read the terminator
  CStringA out(utf8_buffer.get(), conv_bytes - 1);

  return out;
}

// Transform a unicode string into UTF8, as represented by a byte vector.
void WideToUtf8Vector(const CString& wstr, std::vector<uint8>* vec_out) {
  ASSERT1(vec_out);

  if (wstr.IsEmpty()) {
    vec_out->clear();
    return;
  }

  CStringA str_as_utf8(WideToUtf8(wstr));

  ASSERT1(str_as_utf8.GetLength() > 0);
  vec_out->resize(str_as_utf8.GetLength());
  memcpy(&vec_out->front(), CStrBufA(str_as_utf8), vec_out->size());
}

CString Utf8ToWideChar(const char* utf8, uint32 num_bytes) {
  ASSERT1(utf8);
  if (num_bytes == 0) {
    return CString();
  }

  uint32 number_of_wide_chars = ::MultiByteToWideChar(CP_UTF8, 0, utf8, num_bytes, NULL, 0);
  number_of_wide_chars += 1;  // make room for NULL terminator

  CString ret_string;
  TCHAR* buffer = ret_string.GetBuffer(number_of_wide_chars);
  DWORD number_of_characters_copied = ::MultiByteToWideChar(CP_UTF8, 0, utf8, num_bytes, buffer, number_of_wide_chars);
  ASSERT1(number_of_characters_copied == number_of_wide_chars - 1);
  buffer[number_of_wide_chars - 1] = _T('\0');  // ensure there is a NULL terminator
  ret_string.ReleaseBuffer();

  // Strip the byte order marker if there is one in the document.
  if (ret_string[0] == kUnicodeBom) {
    ret_string = ret_string.Right(ret_string.GetLength() - 1);
  }

  if (number_of_characters_copied > 0) {
    return ret_string;
  }

  // Failure case
  return CString();
}

CString Utf8BufferToWideChar(const std::vector<uint8>& buffer) {
  CString result;
  if (!buffer.empty() && buffer.size() < INT_MAX) {
    result = Utf8ToWideChar(
        reinterpret_cast<const char*>(&buffer.front()),
                                      static_cast<int>(buffer.size()));
  }
  return result;
}

CString GetAbsoluteUri(const CString& uri) {
  int i = String_FindString(uri, _T("://"));
  if (i==-1) return uri;

  // add trailing / if none exists
  int j = String_FindChar(uri, L'/',i+3);
  if (j==-1) return (uri+NOTRANSL(_T("/")));

  // remove duplicate trailing slashes
  int len = uri.GetLength();
  if (len > 1 && uri.GetAt(len-1) == '/' && uri.GetAt(len-2) == '/') {
    CString new_uri(uri);
    int new_len = new_uri.GetLength();
    while (new_len > 1 && new_uri.GetAt(new_len-1) == '/' && new_uri.GetAt(new_len-2) == '/') {
      new_len--;
      new_uri = new_uri.Left(new_len);
    }
    return new_uri;
  }
  else return uri;
}

// requires that input have a PROTOCOL (http://) for proper behavior
// items with the "file" protocol are returned as is (what is the hostname in that case? C: ? doesn't make sense)
// TODO(omaha): loosen requirement
// includes http://, e.g. http://www.google.com/
CString GetUriHostName(const CString& uri, bool strip_leading) {
  if (String_StartsWith(uri,NOTRANSL(_T("file:")),true)) return uri;

  // correct any "errors"
  CString s(GetAbsoluteUri(uri));

  // Strip the leading "www."
  if (strip_leading)
  {
    int index_www = String_FindString(s, kStrLeadingWww);
    if (index_www != -1)
      ReplaceCString (s, kStrLeadingWww, _T(""));
  }

  int i = String_FindString(s, _T("://"));
  if(i==-1) return uri;
  int j = String_FindChar(s, L'/',i+3);
  if(j==-1) return uri;
  return s.Left(j+1);
}

// requires that input have a PROTOCOL (http://) for proper behavior
// TODO(omaha): loosen requirement
// removes the http:// and the extra slash '/' at the end.
// http://www.google.com/ -> www.google.com (or google.com if strip_leading = true)
CString GetUriHostNameHostOnly(const CString& uri, bool strip_leading) {
  CString s(GetUriHostName(uri,strip_leading));

  // remove protocol
  int i = String_FindString (s, _T("://"));
  if(i==-1) return s;
  CString ss(s.Right (s.GetLength() - i-3));

  // remove the last '/'
  int j = ss.ReverseFind('/');
  if (j == -1) return ss;
  return ss.Left(j);
}

CString RemoveInternetProtocolHeader (const CString& url) {
  int find_colon_slash_slash = String_FindString(url, NOTRANSL(L"://"));
  if( find_colon_slash_slash != -1 ) {
    // remove PROTOCOL://
    return url.Right(url.GetLength() - find_colon_slash_slash - 3);
  } else if (String_StartsWith(url, NOTRANSL(L"mailto:"), true)) {
    // remove "mailto:"
    return url.Right(url.GetLength() - 7);
  } else {
    // return as is
    return url;
  }
}

HRESULT ConvertFileUriToLocalPath(const CString& uri, CString* path_out) {
  ASSERT1(path_out);
  ASSERT1(uri.GetLength() < INTERNET_MAX_URL_LENGTH);

  if (uri.IsEmpty()) {
    return E_INVALIDARG;
  }

  DWORD buf_len = MAX_PATH;
  HRESULT hr = ::PathCreateFromUrl(uri,
                                   CStrBuf(*path_out, MAX_PATH),
                                   &buf_len,
                                   NULL);
  return hr;
}

bool String_EndsWith(const TCHAR *str, const TCHAR *end_str, bool ignore_case) {
  ASSERT(end_str, (L""));
  ASSERT(str, (L""));

  int str_len = lstrlen(str);
  int end_len = lstrlen(end_str);

  // Definitely false if the suffix is longer than the string
  if (end_len > str_len)
    return false;

  const TCHAR *str_ptr = str + str_len;
  const TCHAR *end_ptr = end_str + end_len;

  while (end_ptr >= end_str) {
    // Check for matching characters
    TCHAR c1 = *str_ptr;
    TCHAR c2 = *end_ptr;

    if (ignore_case) {
      c1 = Char_ToLower(c1);
      c2 = Char_ToLower(c2);
    }

    if (c1 != c2)
      return false;

    --str_ptr;
    --end_ptr;
  }

  // if we haven't failed out, it must be ok!
  return true;
}

CString String_MakeEndWith(const TCHAR* str, const TCHAR* end_str, bool ignore_case) {
  if (String_EndsWith(str, end_str, ignore_case)) {
    return str;
  } else {
    CString r(str);
    r += end_str;
    return r;
  }
}

// Convert Wide to ANSI directly. Use only when it is all ANSI
CStringA WideToAnsiDirect(const CString & in) {
  int in_len = in.GetLength();
  const TCHAR * in_buf = static_cast<const TCHAR*>(in.GetString());

  CStringA out;
  unsigned char * out_buf = (unsigned char *)out.GetBufferSetLength(in_len);

  for(int i = 0; i < in_len; ++i)
    out_buf[i] = static_cast<unsigned char>(in_buf[i]);

  out.ReleaseBuffer(in_len);
  return out;
}

// --------------------------------------------------------
// Reverse (big-endian<->little-endian) the shorts that make up
// Unicode characters in a byte array of Unicode chars
HRESULT ReverseUnicodeByteOrder(byte* unicode_string, int size_in_bytes) {
  ASSERT (unicode_string, (L""));

  // If odd # of bytes, just leave the last one alone
  for (int i = 0; i < size_in_bytes - 1; i += 2) {
    byte b = unicode_string[i];
    unicode_string[i] = unicode_string[i+1];
    unicode_string[i+1] = b;
  }

  return S_OK;
}

// case insensitive strstr
// adapted from http://c.snippets.org/snip_lister.php?fname=stristr.c
const char *stristr(const char *string, const char *pattern)
{
  ASSERT (pattern, (L""));
  ASSERT (string, (L""));
  ASSERT (string && pattern, (L""));
  char *pattern_ptr, *string_ptr;
  const char *start;

  for (start = string; *start != 0; start++)
  {
    // find start of pattern in string
    for ( ; ((*start!=0) && (String_ToUpperA(*start) != String_ToUpperA(*pattern))); start++)
     ;
    if (0 == *start)
     return NULL;

    pattern_ptr = (char *)pattern;
    string_ptr = (char *)start;

    while (String_ToUpperA(*string_ptr) == String_ToUpperA(*pattern_ptr))
    {
      string_ptr++;
      pattern_ptr++;

      // if end of pattern then pattern was found
      if (0 == *pattern_ptr)
        return (start);
    }
  }

  return NULL;
}

// case insensitive Unicode strstr
// adapted from http://c.snippets.org/snip_lister.php?fname=stristr.c
const WCHAR *stristrW(const WCHAR *string, const WCHAR *pattern)
{
  ASSERT (pattern, (L""));
  ASSERT (string, (L""));
  ASSERT (string && pattern, (L""));
  const WCHAR *start;

  for (start = string; *start != 0; start++)
  {
    // find start of pattern in string
    for ( ; ((*start!=0) && (String_ToUpper(*start) != String_ToUpper(*pattern))); start++)
     ;
    if (0 == *start)
     return NULL;

    const WCHAR *pattern_ptr = pattern;
    const WCHAR *string_ptr = start;

    while (String_ToUpper(*string_ptr) == String_ToUpper(*pattern_ptr))
    {
      string_ptr++;
      pattern_ptr++;

      // if end of pattern then pattern was found
      if (0 == *pattern_ptr)
        return (start);
    }
  }

  return NULL;
}

int CalculateBase64EscapedLen(int input_len, bool do_padding) {
  // these formulae were copied from comments that used to go with the base64
  // encoding functions
  int intermediate_result = 8 * input_len + 5;
  ASSERT(intermediate_result > 0,(L""));     // make sure we didn't overflow
  int len = intermediate_result / 6;
  if (do_padding) len = ((len + 3) / 4) * 4;
  return len;
}

// Base64Escape does padding, so this calculation includes padding.
int CalculateBase64EscapedLen(int input_len) {
  return CalculateBase64EscapedLen(input_len, true);
}

// Base64Escape
//   Largely based on b2a_base64 in google/docid_encryption.c
//
//
int Base64EscapeInternal(const char *src, int szsrc,
                         char *dest, int szdest, const char *base64,
                         bool do_padding)
{
  ASSERT(base64, (L""));
  ASSERT(dest, (L""));
  ASSERT(src, (L""));

  static const char kPad64 = '=';

  if (szsrc <= 0) return 0;

  char *cur_dest = dest;
  const unsigned char *cur_src = reinterpret_cast<const unsigned char*>(src);

  // Three bytes of data encodes to four characters of cyphertext.
  // So we can pump through three-byte chunks atomically.
  while (szsrc > 2) { /* keep going until we have less than 24 bits */
    if( (szdest -= 4) < 0 ) return 0;
    cur_dest[0] = base64[cur_src[0] >> 2];
    cur_dest[1] = base64[((cur_src[0] & 0x03) << 4) + (cur_src[1] >> 4)];
    cur_dest[2] = base64[((cur_src[1] & 0x0f) << 2) + (cur_src[2] >> 6)];
    cur_dest[3] = base64[cur_src[2] & 0x3f];

    cur_dest += 4;
    cur_src += 3;
    szsrc -= 3;
  }

  /* now deal with the tail (<=2 bytes) */
  switch (szsrc) {
case 0:
  // Nothing left; nothing more to do.
  break;
case 1:
  // One byte left: this encodes to two characters, and (optionally)
  // two pad characters to round out the four-character cypherblock.
  if( (szdest -= 2) < 0 ) return 0;
  cur_dest[0] = base64[cur_src[0] >> 2];
  cur_dest[1] = base64[(cur_src[0] & 0x03) << 4];
  cur_dest += 2;
  if (do_padding) {
    if( (szdest -= 2) < 0 ) return 0;
    cur_dest[0] = kPad64;
    cur_dest[1] = kPad64;
    cur_dest += 2;
  }
  break;
case 2:
  // Two bytes left: this encodes to three characters, and (optionally)
  // one pad character to round out the four-character cypherblock.
  if( (szdest -= 3) < 0 ) return 0;
  cur_dest[0] = base64[cur_src[0] >> 2];
  cur_dest[1] = base64[((cur_src[0] & 0x03) << 4) + (cur_src[1] >> 4)];
  cur_dest[2] = base64[(cur_src[1] & 0x0f) << 2];
  cur_dest += 3;
  if (do_padding) {
    if( (szdest -= 1) < 0 ) return 0;
    cur_dest[0] = kPad64;
    cur_dest += 1;
  }
  break;
default:
  // Should not be reached: blocks of 3 bytes are handled
  // in the while loop before this switch statement.
  ASSERT(false, (L"Logic problem? szsrc = %S",szsrc));
  break;
  }
  // TODO(portability): cast is unsafe.
  return static_cast<int>(cur_dest - dest);
}

#define kBase64Chars  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

#define kWebSafeBase64Chars "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

int WebSafeBase64Escape(const char *src, int szsrc, char *dest,
  int szdest, bool do_padding) {
    ASSERT(dest, (L""));
    ASSERT(src, (L""));

    return Base64EscapeInternal(src, szsrc, dest, szdest,
      kWebSafeBase64Chars, do_padding);
  }

void Base64Escape(const char *src, int szsrc,
                  CStringA* dest, bool do_padding)
{
  ASSERT(src, (L""));
  ASSERT(dest,(L""));
  const int max_escaped_size = CalculateBase64EscapedLen(szsrc, do_padding);
  dest->Empty();
  const int escaped_len = Base64EscapeInternal(src, szsrc,
      dest->GetBufferSetLength(max_escaped_size + 1), max_escaped_size + 1,
    kBase64Chars,
    do_padding);
  ASSERT(max_escaped_size <= escaped_len,(L""));
  dest->ReleaseBuffer(escaped_len);
}

void WebSafeBase64Escape(const char *src, int szsrc,
                         CStringA *dest, bool do_padding)
{
  ASSERT(src, (L""));
  ASSERT(dest,(L""));
  const int max_escaped_size =
    CalculateBase64EscapedLen(szsrc, do_padding);
  dest->Empty();
  const int escaped_len = Base64EscapeInternal(src, szsrc,
    dest->GetBufferSetLength(max_escaped_size + 1), max_escaped_size + 1,
    kWebSafeBase64Chars,
    do_padding);
  ASSERT(max_escaped_size <= escaped_len,(L""));
  dest->ReleaseBuffer(escaped_len);
}

void WebSafeBase64Escape(const CStringA& src, CStringA* dest) {
  ASSERT(dest,(L""));
  int encoded_len = CalculateBase64EscapedLen(src.GetLength());
  std::unique_ptr<char[]> buf(new char[encoded_len]);
  int len = WebSafeBase64Escape(src,src.GetLength(), buf.get(), encoded_len, false);
  dest->SetString(buf.get(), len);
}

// ----------------------------------------------------------------------
// int Base64Unescape() - base64 decoder
//
// Check out
// http://www.cis.ohio-state.edu/htbin/rfc/rfc2045.html for formal
// description, but what we care about is that...
//   Take the encoded stuff in groups of 4 characters and turn each
//   character into a code 0 to 63 thus:
//           A-Z map to 0 to 25
//           a-z map to 26 to 51
//           0-9 map to 52 to 61
//           +(- for WebSafe) maps to 62
//           /(_ for WebSafe) maps to 63
//   There will be four numbers, all less than 64 which can be represented
//   by a 6 digit binary number (aaaaaa, bbbbbb, cccccc, dddddd respectively).
//   Arrange the 6 digit binary numbers into three bytes as such:
//   aaaaaabb bbbbcccc ccdddddd
//   Equals signs (one or two) are used at the end of the encoded block to
//   indicate that the text was not an integer multiple of three bytes long.
// ----------------------------------------------------------------------
int Base64UnescapeInternal(const char *src, int len_src,
                           char *dest, int len_dest, const char* unbase64) {
  ASSERT (unbase64, (L""));
  ASSERT (src, (L""));

  static const char kPad64 = '=';

  int decode;
  int destidx = 0;
  int state = 0;
  // Used an unsigned char, since ch is used as an array index (into unbase64).
  unsigned char ch = 0;
  while (len_src-- && (ch = *src++) != '\0')  {
    if (IsSpaceA(ch))  // Skip whitespace
      continue;

    if (ch == kPad64)
      break;

    decode = unbase64[ch];
    if (decode == 99)  // A non-base64 character
      return (-1);

    // Four cyphertext characters decode to three bytes.
    // Therefore we can be in one of four states.
    switch (state) {
      case 0:
        // We're at the beginning of a four-character cyphertext block.
        // This sets the high six bits of the first byte of the
        // plaintext block.
        if (dest) {
          if (destidx >= len_dest)
            return (-1);
          // lint -e{734} Loss of precision
          dest[destidx] = static_cast<char>(decode << 2);
        }
        state = 1;
        break;
      case 1:
        // We're one character into a four-character cyphertext block.
        // This sets the low two bits of the first plaintext byte,
        // and the high four bits of the second plaintext byte.
        // However, if this is the end of data, and those four
        // bits are zero, it could be that those four bits are
        // leftovers from the encoding of data that had a length
        // of one mod three.
        if (dest) {
          if (destidx >= len_dest)
            return (-1);
          // lint -e{734} Loss of precision
          dest[destidx]   |=  decode >> 4;
          if (destidx + 1 >= len_dest) {
            if (0 != (decode & 0x0f))
              return (-1);
            else
              ;
          } else {
            // lint -e{734} Loss of precision
            dest[destidx+1] = static_cast<char>((decode & 0x0f) << 4);
          }
        }
        destidx++;
        state = 2;
        break;
      case 2:
        // We're two characters into a four-character cyphertext block.
        // This sets the low four bits of the second plaintext
        // byte, and the high two bits of the third plaintext byte.
        // However, if this is the end of data, and those two
        // bits are zero, it could be that those two bits are
        // leftovers from the encoding of data that had a length
        // of two mod three.
        if (dest) {
          if (destidx >= len_dest)
            return (-1);
          // lint -e{734} Loss of precision
          dest[destidx]   |=  decode >> 2;
          if (destidx +1 >= len_dest) {
            if (0 != (decode & 0x03))
              return (-1);
            else
              ;
          } else {
            // lint -e{734} Loss of precision
            dest[destidx+1] = static_cast<char>((decode & 0x03) << 6);
          }
        }
        destidx++;
        state = 3;
        break;
      case 3:
        // We're at the last character of a four-character cyphertext block.
        // This sets the low six bits of the third plaintext byte.
        if (dest) {
          if (destidx >= len_dest)
            return (-1);
          // lint -e{734} Loss of precision
          dest[destidx] |= decode;
        }
        destidx++;
        state = 0;
        break;

    default:
      ASSERT (false, (L""));
      break;
    }
  }

  // We are done decoding Base-64 chars.  Let's see if we ended
  //      on a byte boundary, and/or with erroneous trailing characters.
  if (ch == kPad64) {               // We got a pad char
    if ((state == 0) || (state == 1))
      return (-1);  // Invalid '=' in first or second position
    if (len_src == 0) {
      if (state == 2)  // We run out of input but we still need another '='
        return (-1);
      // Otherwise, we are in state 3 and only need this '='
    } else {
      if (state == 2) {  // need another '='
        while ((ch = *src++) != '\0' && (len_src-- > 0)) {
          if (!IsSpaceA(ch))
            break;
        }
        if (ch != kPad64)
          return (-1);
      }
      // state = 1 or 2, check if all remain padding is space
      while ((ch = *src++) != '\0' && (len_src-- > 0)) {
        if (!IsSpaceA(ch))
          return(-1);
      }
    }
  } else {
    // We ended by seeing the end of the string.  Make sure we
    //      have no partial bytes lying around.  Note that we
    //      do not require trailing '=', so states 2 and 3 are okay too.
    if (state == 1)
      return (-1);
  }

  return (destidx);
}

int Base64Unescape(const char *src, int len_src, char *dest, int len_dest) {
  ASSERT(dest, (L""));
  ASSERT(src, (L""));

  static const char UnBase64[] = {
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      62/*+*/, 99,      99,      99,      63/*/ */,
     52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
     60/*8*/, 61/*9*/, 99,      99,      99,      99,      99,      99,
     99,       0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
      7/*H*/,  8/*I*/,  9/*J*/, 10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
     15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
     23/*X*/, 24/*Y*/, 25/*Z*/, 99,      99,      99,      99,      99,
     99,      26/*a*/, 27/*b*/, 28/*c*/, 29/*d*/, 30/*e*/, 31/*f*/, 32/*g*/,
     33/*h*/, 34/*i*/, 35/*j*/, 36/*k*/, 37/*l*/, 38/*m*/, 39/*n*/, 40/*o*/,
     41/*p*/, 42/*q*/, 43/*r*/, 44/*s*/, 45/*t*/, 46/*u*/, 47/*v*/, 48/*w*/,
     49/*x*/, 50/*y*/, 51/*z*/, 99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99,
     99,      99,      99,      99,      99,      99,      99,      99
  };

  // The above array was generated by the following code
  // #include <sys/time.h>
  // #include <stdlib.h>
  // #include <string.h>
  // main()
  // {
  //   static const char Base64[] =
  //     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  //   char *pos;
  //   int idx, i, j;
  //   printf("    ");
  //   for (i = 0; i < 255; i += 8) {
  //     for (j = i; j < i + 8; j++) {
  //       pos = strchr(Base64, j);
  //       if ((pos == NULL) || (j == 0))
  //         idx = 99;
  //       else
  //         idx = pos - Base64;
  //       if (idx == 99)
  //         printf(" %2d,     ", idx);
  //       else
  //         printf(" %2d/*%c*/,", idx, j);
  //     }
  //     printf("\n    ");
  //   }
  // }

  return Base64UnescapeInternal(src, len_src, dest, len_dest, UnBase64);
}

int Base64Unescape(const CStringA& src, CStringA* dest) {
  ASSERT1(dest);

  int len = src.GetLength();
  int unescape_len = Base64Unescape(src, len, dest->GetBuffer(len + 1), len);
  dest->ReleaseBufferSetLength(unescape_len >= 0 ? unescape_len : 0);
  return unescape_len;
}

int WebSafeBase64Unescape(const char *src, int szsrc, char *dest, int szdest) {
  ASSERT(dest, (L""));
  ASSERT(src, (L""));

  static const char UnBase64[] = {
    99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      62/*-*/, 99,      99,
      52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
      60/*8*/, 61/*9*/, 99,      99,      99,      99,      99,      99,
      99,       0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
      7/*H*/,  8/*I*/,  9/*J*/, 10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
      15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
      23/*X*/, 24/*Y*/, 25/*Z*/, 99,      99,      99,      99,      63/*_*/,
      99,      26/*a*/, 27/*b*/, 28/*c*/, 29/*d*/, 30/*e*/, 31/*f*/, 32/*g*/,
      33/*h*/, 34/*i*/, 35/*j*/, 36/*k*/, 37/*l*/, 38/*m*/, 39/*n*/, 40/*o*/,
      41/*p*/, 42/*q*/, 43/*r*/, 44/*s*/, 45/*t*/, 46/*u*/, 47/*v*/, 48/*w*/,
      49/*x*/, 50/*y*/, 51/*z*/, 99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99,
      99,      99,      99,      99,      99,      99,      99,      99
  };
  // The above array was generated by the following code
  // #include <sys/time.h>
  // #include <stdlib.h>
  // #include <string.h>
  // main()
  // {
  //   static const char Base64[] =
  //     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  //   char *pos;
  //   int idx, i, j;
  //   printf("    ");
  //   for (i = 0; i < 255; i += 8) {
  //     for (j = i; j < i + 8; j++) {
  //       pos = strchr(Base64, j);
  //       if ((pos == NULL) || (j == 0))
  //         idx = 99;
  //       else
  //         idx = pos - Base64;
  //       if (idx == 99)
  //         printf(" %2d,     ", idx);
  //       else
  //         printf(" %2d/*%c*/,", idx, j);
  //     }
  //     printf("\n    ");
  //   }
  // }

  return Base64UnescapeInternal(src, szsrc, dest, szdest, UnBase64);
}

bool IsHexDigit (WCHAR c) {
  return (((c >= L'a') && (c <= L'f'))
     || ((c >= L'A') && (c <= L'F'))
     || ((c >= L'0') && (c <= L'9')));
}

int HexDigitToInt (WCHAR c) {
  return ((c >= L'a') ? ((c - L'a') + 10) :
          (c >= L'A') ? ((c - L'A') + 10) :
          (c - L'0'));
}

// TODO(omaha): currently set not to use IsCharUpper because that is relatively slow
// this is used in the QUIB; consider if we need to use IsCharUpper or a replacement
bool String_IsUpper(TCHAR c) {
  return (c >= 'A' && c <= 'Z');
  // return (IsCharUpper (c));
}

// Replacement for the CRT toupper(c)
int String_ToUpper(int c) {
  // If it's < 128, then convert is ourself, which is far cheaper than the system conversion
  if (c < 128)
    return String_ToUpperA(static_cast<char>(c));

  TCHAR * p_c = reinterpret_cast<TCHAR *>(static_cast<INT_PTR>(c));
  int conv_c = static_cast<int>(reinterpret_cast<INT_PTR>(::CharUpper(p_c)));
  return conv_c;
}

// Replacement for the CRT toupper(c)
char String_ToUpperA(char c) {
  if (c >= 'a' && c <= 'z') return (c - ('a' - 'A'));
  return c;
}

void String_ToLower(TCHAR* str) {
  ASSERT1(str);
  ::CharLower(str);
}

void String_ToUpper(TCHAR* str) {
  ASSERT1(str);
  ::CharUpper(str);
}

// String comparison based on length
// Replacement for the CRT strncmp(i)
int String_StrNCmp(const TCHAR * str1, const TCHAR * str2, size_t len, bool ignore_case) {
  ASSERT(str2, (L""));
  ASSERT(str1, (L""));

  TCHAR c1, c2;

  if (len == 0)
    return 0;

  // compare each char
  // TODO(omaha): If we use a lot of case sensitive compares consider having 2 loops.
  do {
    c1 = *str1++;
    c2 = *str2++;
    if (ignore_case) {
      c1 = (TCHAR)String_ToLowerChar((int)(c1));  // lint !e507  Suspicious truncation
      c2 = (TCHAR)String_ToLowerChar((int)(c2));  // lint !e507
    }
  } while ( (--len) && c1 && (c1 == c2) );

  return (int)(c1 - c2);
}

// TODO(omaha): Why do we introduce this behaviorial difference?
// Replacement for strncpy() - except ALWAYS ends string with null
TCHAR* String_StrNCpy(TCHAR* destination, const TCHAR* source, size_t len) {
  ASSERT (source, (L""));
  ASSERT (destination, (L""));

  TCHAR* result = destination;

  ASSERT (0 != len, (L""));     // Too short a destination for even the null character

  while (*source && len) {
    *destination++ = *source++;
    len--;
  }

  // If we ran out of space, back up one
  if (0 == len) {
    destination--;
  }

  // Null-terminate the string
  *destination = _T('\0');

  return result;
}

// check if a string starts with another string
bool String_StartsWith(const TCHAR *str, const TCHAR *start_str,
                            bool ignore_case) {
  ASSERT(start_str, (L""));
  ASSERT(str, (L""));

  while (0 != *str) {
    // Check for matching characters
    TCHAR c1 = *str;
    TCHAR c2 = *start_str;

    // Reached the end of start_str?
    if (0 == c2)
      return true;

    if (ignore_case) {
      c1 = (TCHAR)String_ToLowerChar((int)(c1));  // lint !e507  Suspicious truncation
      c2 = (TCHAR)String_ToLowerChar((int)(c2));  // lint !e507  Suspicious truncation
    }

    if (c1 != c2)
      return false;

    ++str;
    ++start_str;
  }

  // If str is shorter than start_str, no match.  If equal size, match.
  return 0 == *start_str;
}

// check if a string starts with another string
bool String_StartsWithA(const char *str, const char *start_str, bool ignore_case) {
  ASSERT(start_str, (L""));
  ASSERT(str, (L""));

  while (0 != *str) {
    // Check for matching characters
    char c1 = *str;
    char c2 = *start_str;

    // Reached the end of start_str?
    if (0 == c2)
      return true;

    if (ignore_case) {
      c1 = String_ToLowerCharAnsi(c1);
      c2 = String_ToLowerCharAnsi(c2);
    }

    if (c1 != c2)
      return false;

    ++str;
    ++start_str;
  }

  // If str is shorter than start_str, no match.  If equal size, match.
  return 0 == *start_str;
}

// the wrapper version below actually increased code size as of 5/31/04
// perhaps because the int64 version is larger and in some EXE/DLLs we only need the int32 version

// converts a string to an int
// Does not check for overflow
// is the direct int32 version significantly faster for our usage?
// int32 String_StringToInt(const TCHAR * str) {
//    ASSERT(str, (L""));
//    return static_cast<int32>(String_StringToInt64 (str));
// }

// converts a string to an int
// Does not check for overflow
int32 String_StringToInt(const TCHAR * str) {
  ASSERT(str, (L""));

  int c;              // current char
  int32 total;         // current total
  int sign;           // if '-', then negative, otherwise positive

  // remove spaces
  while ( *str == _T(' '))
      ++str;

  c = (int)*str++;
  sign = c;           // save sign indication
  if (c == _T('-') || c == _T('+'))
      c = (int)*str++;    // skip sign

  total = 0;

  while ((c = String_CharToDigit(static_cast<TCHAR>(c))) != -1 ) {
      total = 10 * total + c;     // accumulate digit
      c = *str++;    // get next char
  }

  if (sign == '-')
    return -total;
  else
    return total;   // return result, negated if necessary
}

// converts a string to an int64
// Does not check for overflow
int64 String_StringToInt64(const TCHAR * str) {
  ASSERT(str, (L""));

  int c;  // current char
  int64 total;  // current total
  int sign;

  while (*str == ' ') ++str;  // skip space

  c = (int)*str++;
  sign = c;           /* save sign indication */
  if (c == '-' || c == '+')
    c = (int)*str++;

  total = 0;

  while ((c = String_CharToDigit(static_cast<TCHAR>(c))) != -1) {
    total = 10 * total + c;     /* accumulate digit */
    c = *str++;    /* get next char */
  }

  if (sign == '-')
    return -total;
  else
    return total;
}

// A faster version of the ::CharLower command. We first check if all characters are in low ANSI
// If so, we can convert it ourselves [which is about 10x faster]
// Otherwise, ask the system to do it for us.
TCHAR * String_FastToLower(TCHAR * str) {
  ASSERT(str, (L""));

  TCHAR * p = str;
  while (*p) {
    // If we can't process it ourselves, then do it with the API
    if (*p > 127)
      return ::CharLower(str);
    ++p;
  }

  // If we're still here, do it ourselves
  p = str;
  while (*p) {
    // Lower case it
    if (*p >= L'A' && *p <= 'Z')
      *p |= 0x20;
    ++p;
  }

  return str;
}

// Convert a size_t to a CString
CString sizet_to_str(const size_t & i) {
  CString out;
  SafeCStringFormat(&out, NOTRANSL(_T("%Iu")),i);
  return out;
}

// Convert an int to a CString
CString itostr(const int i) {
  return String_Int64ToString(i, 10);
}

// Convert a uint to a CString
CString itostr(const uint32 i) {
  return String_Int64ToString(i, 10);
}

// converts an int to a string
// Does not check for overflow
CString String_Int64ToString(int64 value, int radix) {
  ASSERT(radix > 0, (L""));

  // Space big enough for it in binary, plus the sign
  TCHAR temp[66];

  bool negative = false;
  if (value < 0) {
    negative = true;
    value = -value;
  }

  int pos = 0;

  // Add digits in reverse order
  do {
    TCHAR digit = (TCHAR) (value % radix);
    if (digit > 9)
      temp[pos] = L'a' + digit - 10;
    else
      temp[pos] = L'0' + digit;

    pos++;
    value /= radix;
  } while (value > 0);

  if (negative)
    temp[pos++] = L'-';

  // Reverse it before making a CString out of it
  int start = 0, end = pos - 1;
  while (start < end) {
    TCHAR t = temp[start];
    temp[start] = temp[end];
    temp[end] = t;

    end--;
    start++;
  }

  return CString(temp, pos);
}

// converts an uint64 to a string
// Does not check for overflow
CString String_Uint64ToString(uint64 value, int radix) {
  ASSERT1(radix > 0);

  CString ret;

  const uint32 kMaxUint64Digits = 65;

  // Space big enough for it in binary
  TCHAR* temp = ret.GetBufferSetLength(kMaxUint64Digits);

  int pos = 0;

  // Add digits in reverse order
  do {
    TCHAR digit = static_cast<TCHAR>(value % radix);
    if (digit > 9) {
      temp[pos] = _T('a') + digit - 10;
    } else {
      temp[pos] = _T('0') + digit;
    }

    pos++;
    value /= radix;
  } while (value > 0 && pos < kMaxUint64Digits);

  ret.ReleaseBuffer(pos);

  // Reverse it before making a CString out of it
  ret.MakeReverse();

  return ret;
}

// converts an double to a string specifies the number of digits after
// the decimal point
CString String_DoubleToString(double value, int point_digits) {
  int64 int_val = (int64) value;

  // Deal with integer part
  CString result(String_Int64ToString(int_val, 10));

  if (point_digits > 0) {
    result.AppendChar(L'.');

    // get the fp digits
    double rem_val = value - int_val;
    if (rem_val < 0)
      rem_val = -rem_val;

    // multiply w/ the requested number of significant digits
    // construct the string in place
    for(int i=0; i<point_digits; i++) {
      // TODO(omaha): I have seen 1.2 turn into 1.1999999999999, and generate that string.
      // We should round better. For now, I'll add a quick fix to favor high
      rem_val += 1e-12;
      rem_val *= 10;
      // Get the ones digit
      int64 int_rem_dig = std::min(10LL, static_cast<int64>(rem_val));
      result += static_cast<TCHAR>(int_rem_dig + L'0');
      rem_val = rem_val - int_rem_dig;
    }
  }

  return result;
}

double String_StringToDouble (const TCHAR *s) {
  ASSERT(s, (L""));

  double value, power;
  int i = 0, sign;

  while (IsSpaceW(s[i])) i++;

  // get sign
  sign = (s[i] == '-') ? -1 : 1;
  if (s[i] == '+' || s[i] == '-') i++;

  for (value = 0.0; s[i] >= '0' && s[i] <= '9'; i++)
    value = 10.0 * value + (s[i] - '0');

  if (s[i] == '.') i++;

  for (power = 1.0; s[i] >= '0' && s[i] <= '9'; i++) {
    value = 10.0 * value + (s[i] - '0');
    power *= 10.0;
  }

  return sign * value / power;
}

// Converts a character to a digit
// if the character is not a digit return -1 (same as CRT)
int32 String_CharToDigit(const TCHAR c) {
  return ((c) >= '0' && (c) <= '9' ? (c) - '0' : -1);
}

bool String_IsDigit (const TCHAR c) {
  return ((c) >= '0' && (c) <= '9');
}

TCHAR String_DigitToChar(unsigned int n) {
  ASSERT1(n < 10);
  return static_cast<TCHAR>(_T('0') + n % 10);
}

// Returns true if an identifier character: letter, digit, or "_"
bool String_IsIdentifierChar(const TCHAR c) {
  return ((c >= _T('A') && c <= _T('Z')) ||
          (c >= _T('a') && c <= _T('z')) ||
          (c >= _T('0') && c <= _T('9')) ||
          c == _T('_'));
}

int String_FindString(const TCHAR *s1, const TCHAR *s2) {
  ASSERT(s2, (L""));
  ASSERT(s1, (L""));

  // Naive implementation, but still oodles better than ATL's implementation
  // (which deals with variable character widths---we don't).

  const TCHAR *found = _tcsstr(s1, s2);
  if (NULL == found)
    return -1;

  // TODO(portability): cast is unsafe.
  return static_cast<int>(found - s1);
}

int String_FindString(const TCHAR *s1, const TCHAR *s2, int start_pos) {
  ASSERT(s2, (L""));
  ASSERT(s1, (L""));

  // Naive implementation, but still oodles better than ATL's implementation
  // (which deals with variable character widths---we don't).

  int skip = start_pos;

  const TCHAR *s = s1;
  while (skip && *s) {
    ++s;
    --skip;
  }
  if (!(*s))
    return -1;

  const TCHAR *found = _tcsstr(s, s2);
  if (NULL == found)
    return -1;

  // TODO(portability): cast is unsafe.
  return static_cast<int>(found - s1);
}

int String_FindChar(const TCHAR *str, const TCHAR c) {
  ASSERT (str, (L""));
  const TCHAR *s = str;
  while (*s) {
    if (*s == c) {
      // TODO(portability): cast is unsafe.
      return static_cast<int>(s - str);
    }
    ++s;
  }

  return -1;
}

int String_FindChar(const TCHAR *str, const TCHAR c, int start_pos) {
  ASSERT (str, (L""));
  int n = 0;
  const TCHAR *s = str;
  while (*s) {
    if (n++ >= start_pos && *s == c) {
      // TODO(portability): cast is unsafe.
      return static_cast<int>(s - str);
    }
    ++s;
  }

  return -1;
}

bool String_Contains(const TCHAR *s1, const TCHAR *s2) {
  ASSERT(s2, (L""));
  ASSERT(s1, (L""));

  return -1 != String_FindString(s1, s2);
}

int ReplaceCString (CString & src, const TCHAR *from, const TCHAR *to) {
  ASSERT(to, (L""));
  ASSERT(from, (L""));

  return ReplaceCString(src, from, lstrlen(from), to, lstrlen(to), kRepMax);
}

// A special version of the replace function which takes advantage of CString properties
// to make it much faster when the string grows
// 1) It will resize the string in place if possible. Even if it has to 'grow' the string
// 2) It will cutoff after a maximum number of matches
// 3) It expects sizing data to be passed to it
int ReplaceCString (CString & src, const TCHAR *from, unsigned int from_len,
                                   const TCHAR *to, unsigned int to_len,
                                   unsigned int max_matches) {
  ASSERT (from, (L""));
  ASSERT (to, (L""));
  ASSERT (from[0] != '\0', (L""));
  int i = 0, j = 0;
  unsigned int matches = 0;

  // Keep track of the matches, it's easier than recalculating them
  unsigned int match_pos_stack[kExpectedMaxReplaceMatches];

  // We might need to dynamically allocate space for the matches
  bool dynamic_allocate = false;
  unsigned int * match_pos = (unsigned int*)match_pos_stack;
  unsigned int max_match_size = kExpectedMaxReplaceMatches;

  // Is the string getting bigger?
  bool longer = to_len > from_len;

  // don't compute the lengths unless we know we need to
  int src_len = src.GetLength();
  int cur_len = src_len;

  // Trick: We temporarily add 1 extra character to the string. The first char from the from
  // string. This way we can avoid searching for NULL, since we are guaranteed to find it
  TCHAR * buffer = src.GetBufferSetLength(src_len+1);
  const TCHAR from_0 = from[0];
  buffer[src_len] = from[0];

  while (i < cur_len) {
    // If we have too many matches, then re-allocate to a dynamic buffer that is
    // twice as big as the one we are currently using
    if (longer && (matches == max_match_size)) {
      // Double the buffer size, and copy it over
      unsigned int * temp = new unsigned int[max_match_size * 2];
      memcpy(temp, match_pos, matches * sizeof(unsigned int));
      if (dynamic_allocate)
        delete [] match_pos;  // lint !e424  Inappropriate deallocation
      match_pos = temp;

      max_match_size *= 2;
      dynamic_allocate = true;
    }

    // If we have the maximum number of matches already, then stop
    if (matches >= max_matches) {
      break;
    }

    // For each potential match
    // Note: oddly enough, this is the most expensive line in the function under normal usage. So I am optimizing the heck out of it
    TCHAR * buf_ptr = buffer + i;
    while (*buf_ptr != from_0) { ++buf_ptr; }

    // TODO(portability): cast is unsafe.
    i = static_cast<int>(buf_ptr - buffer);

    // We're done!
    if (i >= cur_len)
      break;

    // buffer is not NULL terminated, we replaced the NULL above
    while (i < cur_len && buffer[i] && buffer[i] == from[j]) {
      ++i; ++j;
      if (from[j] == '\0') {  // found match

        if (!longer) {  // modify in place

          memcpy ((byte *)(buffer+i) - (sizeof (TCHAR) * from_len), (byte *)to, sizeof (TCHAR) * to_len);
          // if there are often a lot of replacements, it would be faster to create a new string instead
          // of using memmove

          // TODO(omaha): - memmove will cause n^2 behavior in strings with multiple matches since it will be moved many times...
          if (to_len < from_len) { memmove ((byte *)(buffer+i) - (sizeof (TCHAR) * (from_len - to_len)),
                                          (byte *)(buffer+i), (src_len - i + 1) * sizeof (TCHAR)); }

          i -= (from_len - to_len);
          cur_len -= (from_len - to_len);
        }
        else
          match_pos[matches] = i - from_len;

        ++matches;

        break;
      }
    }

    j = 0;
  }

  if (to_len <= from_len)
    src_len -= matches * (from_len - to_len);

  // if the new string is longer we do another pass now that we know how long the new string needs to be
  if (matches && to_len > from_len) {
    src.ReleaseBuffer(src_len);

    int new_len = src_len + matches * (to_len - from_len);
    buffer = src.GetBufferSetLength(new_len);

    // It's easier to assemble it backwards...
    int temp_end = new_len;
    for(i = matches-1; i >= 0; --i) {
      // Figure out where the trailing portion isthe trailing portion
      int len = src_len - match_pos[i] - from_len;
      int start  = match_pos[i] + from_len;
      int dest   = temp_end - len;
      memmove(buffer+dest, buffer+start, (len) * sizeof(TCHAR));

      // copy the new item
      memcpy(buffer + dest - to_len, to, to_len * sizeof(TCHAR));

      // Update the pointers
      temp_end = dest - to_len;
      src_len = match_pos[i];

    }
    src_len = new_len;
  }

  src.ReleaseBuffer(src_len);
  if (dynamic_allocate)
    delete [] match_pos;  // lint !e673  Possibly inappropriate deallocation

  return matches;
}

// The internal format is a int64.
time64 StringToTime(const CString & time) {
  return static_cast<time64>(String_StringToInt64(time));
}

// See above comment from StringToTime.
// Just show it as a INT64 for now
// NOTE: this will truncating it to INT64, which may lop off some times in the future
CString TimeToString(const time64 & time) {
  return String_Int64ToString(static_cast<int64>(time), 10);
}

bool IsAlphaA (const char c) {
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

bool IsDigitA (const char c) {
  return (c >= '0' && c <= '9');
}

char String_ToLowerCharAnsi(char c) {
  if (c >= 'A' && c <= 'Z') return (c + ('a' - 'A'));
  return c;
}

int String_ToLowerChar(int c) {
  // If it's < 128, then convert is ourself, which is far cheaper than the system conversion
  if (c < 128)
    return String_ToLowerCharAnsi(static_cast<char>(c));

  return Char_ToLower(static_cast<TCHAR>(c));
}

void String_EndWithChar(TCHAR *str, TCHAR c) {
  ASSERT (str, (L""));
  int len = lstrlen(str);
  if (len == 0 || str[len - 1] != c) {
    str[len] = c;
    str[len + 1] = 0;
  }
}

bool StartsWithBOM(const TCHAR* string) {
  ASSERT(string, (L""));
  wchar_t c = string[0];
  if (c == 0xFFFE || c == 0xFEFF)
    return true;
  else
    return false;
}

const TCHAR* StringAfterBOM(const TCHAR* string) {
  ASSERT(string, (L""));
  return &string[StartsWithBOM(string) ? 1 : 0];
}

bool String_StringToDecimalIntChecked(const TCHAR* str, int* value) {
  ASSERT1(str);
  ASSERT1(value);

  if (_set_errno(0)) {
    return false;
  }

  TCHAR* end_ptr = NULL;
  *value = _tcstol(str, &end_ptr, 10);
  ASSERT1(end_ptr);

  if (errno) {
    ASSERT1(ERANGE == errno);
    // Overflow or underflow.
    return false;
  } else if (*value == 0) {
    // The value returned could be an error code. tcsltol returns
    // zero when it cannot convert the string. However we need to
    // distinguish a real zero. Thus check to see if end_ptr is not the start
    // of the string (str is not an empty string) and is pointing to a '\0'.
    // If not, we have an error.
    if ((str == end_ptr) || (*end_ptr != '\0')) {
      return false;
    }
  } else if (*end_ptr != '\0') {
    // The end_ptr is pointing at a character that is
    // not the end of the string. Only part of the string could be converted.
    return false;
  }

  return true;
}

HRESULT String_StringToBool(const TCHAR* str, bool* value) {
  ASSERT1(str);
  ASSERT1(value);

  // This method now performs a case-insentitive
  // culture aware compare. We should however be ok as we are only comparing
  // latin characters.
  if (_tcsicmp(kFalse, str) == 0) {
    *value = false;
  } else if (_tcsicmp(kTrue, str) == 0) {
    *value = true;
  } else {
    // we found another string. should error out.
    return E_FAIL;
  }
  return S_OK;
}

const TCHAR* String_BoolToString(bool value) {
  return value ? kTrue : kFalse;
}

CString String_ReplaceIgnoreCase(const CString& string,
                                 const CString& token,
                                 const CString& replacement) {
  int token_length = token.GetLength();
  if (!token_length) {
    return string;
  }

  CString string_lowercase(string);
  CString token_lowercase(token);
  string_lowercase.MakeLower();
  token_lowercase.MakeLower();

  CString output(string);
  int replacement_length = replacement.GetLength();

  int index = 0;
  int output_index = 0;

  for (int new_index = 0;
       (new_index = string_lowercase.Find(token_lowercase, index)) != -1;
       index = new_index + token_length) {
    output_index += new_index - index;
    output.Delete(output_index, token_length);
    output.Insert(output_index, replacement);
    output_index += replacement_length;
  }

  return output;
}

// Escape and unescape strings (shlwapi-based implementation).
// The intended usage for these APIs is escaping strings to make up
// URLs, for example building query strings.
//
// Pass false to the flag segment_only to escape the url. This will not
// cause the conversion of the # (%23), ? (%3F), and / (%2F) characters.

// Characters that must be encoded include any characters that have no
// corresponding graphic character in the US-ASCII coded character
// set (hexadecimal 80-FF, which are not used in the US-ASCII coded character
// set, and hexadecimal 00-1F and 7F, which are control characters),
// blank spaces, "%" (which is used to encode other characters),
// and unsafe characters (<, >, ", #, {, }, |, \, ^, ~, [, ], and ').
//
// The input and output strings can't be longer than INTERNET_MAX_URL_LENGTH

HRESULT StringEscape(const CString& str_in,
                     bool segment_only,
                     CString* str_out) {
  ASSERT1(str_out);
  ASSERT1(str_in.GetLength() < INTERNET_MAX_URL_LENGTH);

  DWORD buf_len = INTERNET_MAX_URL_LENGTH + 1;
  HRESULT hr = ::UrlEscape(str_in, str_out->GetBufferSetLength(buf_len), &buf_len,
    segment_only ? URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY : URL_ESCAPE_PERCENT);
  if (SUCCEEDED(hr)) {
    str_out->ReleaseBuffer();
    ASSERT1(buf_len <= INTERNET_MAX_URL_LENGTH);
  }
  return hr;
}

HRESULT StringUnescape(const CString& str_in, CString* str_out) {
  ASSERT1(str_out);
  ASSERT1(str_in.GetLength() < INTERNET_MAX_URL_LENGTH);

  DWORD buf_len = INTERNET_MAX_URL_LENGTH + 1;
  HRESULT hr = ::UrlUnescape(const_cast<TCHAR*>(str_in.GetString()),
    str_out->GetBufferSetLength(buf_len), &buf_len, 0);
  if (SUCCEEDED(hr)) {
    str_out->ReleaseBuffer(buf_len + 1);
    ASSERT1(buf_len <= INTERNET_MAX_URL_LENGTH);
  }
  return hr;
}

bool String_StringToTristate(const TCHAR* str, Tristate* value) {
  ASSERT1(str);
  ASSERT1(value);

  int numerical_value = 0;
  if (!String_StringToDecimalIntChecked(str, &numerical_value)) {
    return false;
  }

  switch (numerical_value) {
    case 0:
      *value = TRISTATE_FALSE;
      break;
    case 1:
      *value = TRISTATE_TRUE;
      break;
    case 2:
      *value = TRISTATE_NONE;
      break;
    default:
      return false;
  }

  return true;
}

// Extracts the name and value from a string that contains a name/value pair.
bool ParseNameValuePair(const CString& token,
                        TCHAR separator,
                        CString* name,
                        CString* value) {
  ASSERT1(name);
  ASSERT1(value);

  int separator_index = token.Find(separator);
  if ((separator_index == -1) ||  // Not a name-value pair.
      (separator_index == 0) ||  // No name was supplied.
      (separator_index == (token.GetLength() - 1))) {  // No value was supplied.
    return false;
  }

  *name = token.Left(separator_index);
  *value = token.Right(token.GetLength() - separator_index - 1);

  ASSERT1(token.GetLength() == name->GetLength() + value->GetLength() + 1);

  // It's not possible for the name to contain the separator.
  ASSERT1(-1 == name->Find(separator));
  if (-1 != value->Find(separator)) {
    // The value contains the separator.
    return false;
  }

  return true;
}

bool SplitCommandLineInPlace(TCHAR *command_line,
                             TCHAR **first_argument_parameter,
                             TCHAR **remaining_arguments_parameter) {
  if (!command_line ||
      !first_argument_parameter ||
      !remaining_arguments_parameter) {
    return false;
  }

  TCHAR end_char;
  TCHAR *&first_argument = *first_argument_parameter;
  TCHAR *&remaining_arguments = *remaining_arguments_parameter;
  if (_T('\"') == *command_line) {
    end_char = _T('\"');
    first_argument = remaining_arguments = command_line + 1;
  } else {
    end_char = _T(' ');
    first_argument = remaining_arguments = command_line;
  }
  // Search for the end of the first argument
  while (end_char != *remaining_arguments && '\0' != *remaining_arguments) {
    ++remaining_arguments;
  }
  if (end_char == *remaining_arguments) {
    *remaining_arguments = '\0';
    do {
      // Skip the spaces between the first argument and the remaining arguments.
      ++remaining_arguments;
    } while (_T(' ') == *remaining_arguments);
  }
  return true;
}

bool ContainsOnlyAsciiChars(const CString& str) {
  for (int i = 0; i < str.GetLength(); ++i) {
    if (str[i] > 0x7F) {
      return false;
    }
  }
  return true;
}
CString BytesToHex(const uint8* bytes, size_t num_bytes) {
  CString result;
  if (bytes && num_bytes < INT_MAX/sizeof(TCHAR)) {
    result.Preallocate(static_cast<int>(num_bytes * sizeof(TCHAR)));
    static const TCHAR* const kHexChars = _T("0123456789abcdef");
    for (size_t i = 0; i != num_bytes; ++i) {
      result.AppendChar(kHexChars[(bytes[i] >> 4)]);
      result.AppendChar(kHexChars[(bytes[i] & 0xf)]);
    }
  }
  return result;
}

CString BytesToHex(const std::vector<uint8>& bytes) {
  CString result;
  if (!bytes.empty()) {
    result.SetString(BytesToHex(&bytes.front(), bytes.size()));
  }
  return result;
}

CString FormatResourceMessage(uint32 resource_id, ...) {
  CString format;
  const bool is_loaded = !!format.LoadString(resource_id);

  if (!is_loaded) {
    return CString();
  }

  va_list arg_list;
  va_start(arg_list, resource_id);

  CString formatted;
  formatted.FormatMessageV(format, &arg_list);

  va_end(arg_list);

  return formatted;
}

CString FormatErrorCode(DWORD error_code) {
  CString error_code_string;
  if (FAILED(error_code)) {
    SafeCStringFormat(&error_code_string, _T("0x%08x"), error_code);
  } else {
    SafeCStringFormat(&error_code_string, _T("%u"), error_code);
  }
  return error_code_string;
}

HRESULT WideStringToUtf8UrlEncodedString(const CString& str, CString* out) {
  ASSERT1(out);

  out->Empty();
  if (str.IsEmpty()) {
    return S_OK;
  }

  // Utf8 encode the Utf16 string first. Next urlencode it.
  CStringA utf8str = WideToUtf8(str);
  ASSERT1(!utf8str.IsEmpty());
  DWORD buf_len = INTERNET_MAX_URL_LENGTH;
  CStringA escaped_utf8_name;
  HRESULT hr = ::UrlEscapeA(utf8str,
                            CStrBufA(escaped_utf8_name, buf_len),
                            &buf_len,
                            0);
  ASSERT1(buf_len <= INTERNET_MAX_URL_LENGTH);
  ASSERT1(escaped_utf8_name.GetLength() == static_cast<int>(buf_len));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[UrlEscapeA failed][0x%08x]"), hr));
    return hr;
  }

  *out = CString(escaped_utf8_name);
  return S_OK;
}

HRESULT Utf8UrlEncodedStringToWideString(const CString& str, CString* out) {
  ASSERT1(out);

  out->Empty();
  if (str.IsEmpty()) {
    return S_OK;
  }

  // The value is a utf8 encoded url escaped string that is stored as a
  // unicode string. Because of this, it should contain only ascii chars.
  if (!ContainsOnlyAsciiChars(str)) {
    UTIL_LOG(LE, (_T("[String contains non ascii chars]")));
    return E_INVALIDARG;
  }

  CStringA escaped_utf8_val = WideToAnsiDirect(str);
  DWORD buf_len = INTERNET_MAX_URL_LENGTH;
  CStringA unescaped_val;
  HRESULT hr = ::UrlUnescapeA(const_cast<char*>(escaped_utf8_val.GetString()),
                              CStrBufA(unescaped_val, buf_len),
                              &buf_len,
                              0);
  ASSERT1(unescaped_val.GetLength() == static_cast<int>(buf_len));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[UrlUnescapeA failed][0x%08x]"), hr));
    return hr;
  }
  ASSERT1(buf_len == static_cast<DWORD>(unescaped_val.GetLength()));
  ASSERT1(buf_len <= INTERNET_MAX_URL_LENGTH);
  CString app_name = Utf8ToWideChar(unescaped_val,
                                    unescaped_val.GetLength());
  if (app_name.IsEmpty()) {
    return E_INVALIDARG;
  }

  *out = app_name;
  return S_OK;
}

static char hex_value[256] = {
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  // '0'..'9'
  0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 'A'..'F'
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 'a'..'f'
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static char hex_char[] = "0123456789abcdef";

// This is a templated function so that T can be either a char*
// or a string.  This works because we use the [] operator to access
// individual characters at a time.
template <typename T>
static void a2b_hex_t(const char* a, T b, size_t num) {
  for (size_t i = 0; i < num; i++) {
    b[i] = (hex_value[a[i * 2] & 0xFF] << 4)
         + (hex_value[a[i * 2 + 1] & 0xFF]);
  }
}

// This is a templated function so that T can be either a char*
// or a string.  This works because we use the [] operator to access
// individual characters at a time.
template <typename T>
static void b2a_hex_t(const unsigned char* b, T a, size_t num) {
  for (size_t i = 0; i < num; i++) {
    a[i * 2 + 0] = hex_char[b[i] >> 4];
    a[i * 2 + 1] = hex_char[b[i] & 0xf];
  }
}

void b2a_hex(const unsigned char* b, char* a, size_t num) {
  b2a_hex_t<char*>(b, a, num);
}

void a2b_hex(const char* a, unsigned char* b, size_t num) {
  a2b_hex_t<unsigned char*>(a, b, num);
}

void a2b_hex(const char* a, char* b, size_t num) {
  a2b_hex_t<char*>(a, b, num);
}

string b2a_hex(const char* b, size_t len) {
  string result;
  result.resize(len << 1);
  b2a_hex_t<string&>(reinterpret_cast<const unsigned char*>(b), result, len);
  return result;
}

string a2b_hex(const string& a) {
  string result;
  a2b_hex(a.c_str(), &result, a.size()/2);

  return result;
}

void b2a_hex(const unsigned char* from, string* to, size_t num) {
  to->resize(num << 1);
  b2a_hex_t<string&>(from, *to, num);
}

void a2b_hex(const char* from, string* to, size_t num) {
  to->resize(num);
  a2b_hex_t<string&>(from, *to, num);
}

template <typename T> bool CheckHexString(const T& str) {
  if (str.IsEmpty()) {
    return false;
  }

  const int kStrLen = str.GetLength();
  if (kStrLen % 2 != 0) {
    return false;
  }

  for (int i = 0; i < kStrLen; ++i) {
    if (!IsHexDigit(str[i])) {
      return false;
    }
  }

  return true;
}

bool SafeHexStringToVector(const CStringA& str, std::vector<uint8>* vec_out) {
  ASSERT1(vec_out);

  if (!CheckHexString(str)) {
    return false;
  }

  vec_out->resize(str.GetLength() / 2);
  a2b_hex(str.GetString(), &vec_out->front(), vec_out->size());
  return true;
}

bool SafeHexStringToVector(const CStringW& str, std::vector<uint8>* vec_out) {
  // Check for input validity before we do a Wide->ANSI conversion.
  if (!CheckHexString(str)) {
    return false;
  }

  return SafeHexStringToVector(WideToAnsiDirect(str), vec_out);
}

}  // namespace omaha
