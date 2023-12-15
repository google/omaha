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

#ifndef OMAHA_BASE_STRING_H_
#define OMAHA_BASE_STRING_H_

#include <windows.h>
#include <stdint.h>

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"

namespace omaha {

#define STR_SIZE(str) (arraysize(str)-1)  // number of characters in char array (only for single-byte string literals!!!)
#define TSTR_SIZE(tstr) (arraysize(tstr)-1)  // like STR_SIZE but works on _T("string literal") ONLY!!!

// The number of replacements matches we expect, before we start allocating extra memory
// to process it. This is an optimizing constant
constexpr size_t kExpectedMaxReplaceMatches = 100;

// removes "http://", "ftp://", "mailto:" or "file://" (note that the "file" protocol is
// like: "file:///~/calendar", this method removes only the first two slashes
CString RemoveInternetProtocolHeader (const CString& url);

// Converts a file:// URI to a valid Windows path.
HRESULT ConvertFileUriToLocalPath(const CString& uri, CString* path_out);

// TODO(omaha): be consistent on use of int/uint32/int32 for lengths

// The input length of the string does not include the null terminator.
// Caller deletes the returned buffer.
WCHAR *ToWide (const char *s, int len);

// Convert Wide to ANSI directly. Use only when it is all ANSI
CStringA WideToAnsiDirect(const CString & in);

// Transform a unicode string into UTF8, used primarily by the webserver
CStringA WideToUtf8(const CString& w);
void WideToUtf8Vector(const CString& wstr, std::vector<uint8>* vec_out);

// Converts the UTF-8 encoded buffer to an in-memory Unicode (wide character)
// string.
// @param utf8 A non-NULL pointer to a UTF-8 encoded buffer that has at
// least num_bytes valid characters.
// @param num_bytes Number of bytes to process from utf8.
// @return The Unicode string represented by utf8 (or that part of it
// specified by num_bytes).  If the UTF-8 representation of the string started
// with a byte-order marker (BOM), it will be ignored and not included in the
// returned string.  On failure, the function returns the empty string.
CString Utf8ToWideChar(const char* utf8, uint32 num_bytes);
CString Utf8BufferToWideChar(const std::vector<uint8>& buffer);

// Dealing with Unicode BOM
bool StartsWithBOM(const TCHAR* string);
const TCHAR* StringAfterBOM(const TCHAR* string);

// Convert an ANSI string into Widechar string, according to the specified
// codepage. The input length can be -1, if the string is null terminated, and
// the actual length will be used internally.
BOOL AnsiToWideString(const char *from, int length, UINT codepage, CString *to);

// Convert char to Wchar directly
CString AnsiToWideString(const char *from, int length);

// "Absolute" is perhaps not the right term, this normalizes the Uri
// given http://www.google.com changes to correct http://www.google.com/
// given http://www.google.com// changes to correct http://www.google.com/
// given http://www.google.com/home.html returns the same
CString GetAbsoluteUri(const CString& uri);

// Reverse (big-endian<->little-endian) the shorts that make up
// Unicode characters in a byte array of Unicode chars
HRESULT ReverseUnicodeByteOrder(byte* unicode_string, int size_in_bytes);

// given http://google.com/bobby this returns http://google.com/
// If strip_leading is specified, it will turn
// http://www.google.com into http://google.com
#define kStrLeadingWww _T("www.")
// TODO(omaha): no default parameters
CString GetUriHostName(const CString& uri, bool strip_leading = false);
CString GetUriHostNameHostOnly(const CString& uri, bool strip_leading_www);

const char *stristr(const char *string, const char *pattern);
const WCHAR *stristrW(const WCHAR *string, const WCHAR *pattern);

// Return the length to use for the output buffer given to the base64 escape
// routines. Make sure to use the same value for do_padding in both.
// This function may return incorrect results if given input_len values that
// are extremely high, which should happen rarely.
int CalculateBase64EscapedLen(int input_len, bool do_padding);
// Use this version when calling Base64Escape without a do_padding arg.
int CalculateBase64EscapedLen(int input_len);

// ----------------------------------------------------------------------
// Base64Escape()
// WebSafeBase64Escape()
//    Encode "src" to "dest" using base64 encoding.
//    src is not null terminated, instead specify len.
//    'dest' should have at least CalculateBase64EscapedLen() length.
//    RETURNS the length of dest.
//    The WebSafe variation use '-' instead of '+' and '_' instead of '/'
//    so that we can place the out in the URL or cookies without having
//    to escape them.  It also has an extra parameter "do_padding",
//    which when set to false will prevent padding with "=".
// ----------------------------------------------------------------------
int WebSafeBase64Escape(const char *src, int slen, char *dest,
                        int szdest, bool do_padding);
void WebSafeBase64Escape(const CStringA& src, CStringA* dest);

void Base64Escape(const char *src, int szsrc,
                  CStringA* dest, bool do_padding);
void WebSafeBase64Escape(const char *src, int szsrc,
                         CStringA* dest, bool do_padding);

// ----------------------------------------------------------------------
// Base64Unescape()
//    Copies "src" to "dest", where src is in base64 and is written to its
//    ASCII equivalents. src is not null terminated, instead specify len.
//    I recommend that slen<len_dest, but we honour len_dest anyway.
//    RETURNS the length of dest.
//    The WebSafe variation use '-' instead of '+' and '_' instead of '/'.
//
//    The functions return the number of characters that are decoded in the
//    destination buffer or -1 in case of a decoding error.
// ----------------------------------------------------------------------
int Base64Unescape(const char *src, int slen, char *dest, int len_dest);
int Base64Unescape(const CStringA& src, CStringA* dest);
int WebSafeBase64Unescape(const char *src, int slen, char *dest, int szdest);

#ifdef UNICODE
#define IsSpace IsSpaceW
#else
#define IsSpace IsSpaceA
#endif

bool IsSpaceW(WCHAR c);
bool IsSpaceA(char c);

// Trims all characters in the delimiter string from both ends of the
// string s
void TrimString(CString& s, const TCHAR* delimiters);

// A block of text to separate lines, and back
void TextToLines(const CString& text, const TCHAR* delimiter, std::vector<CString>* lines);
// (LinesToText puts a delimiter at the end of the last line too)
void LinesToText(const std::vector<CString>& lines, const TCHAR* delimiter, CString* text);

// Make a CString lower case
void MakeLowerCString(CString & s);

int HexDigitToInt (WCHAR c);
bool IsHexDigit (WCHAR c);

// Converts to lower, but does so much faster if the string is ANSI
TCHAR * String_FastToLower(TCHAR * str);

// Replacement for the CRT toupper(c)
int String_ToUpper(int c);

// Replacement for the CRT toupper(c)
char String_ToUpperA(char c);

// Converts str to lowercase in place.
void String_ToLower(TCHAR* str);

// Converts str to uppercase in place.
void String_ToUpper(TCHAR* str);

bool String_IsUpper(TCHAR c);

// String comparison based on length
// Replacement for the CRT strncmp(i)
int String_StrNCmp(const TCHAR * str1, const TCHAR * str2, size_t len, bool ignore_case);

// Replacement for strncpy() - except ALWAYS ends string with null
TCHAR* String_StrNCpy(TCHAR* destination, const TCHAR* source, size_t len);

// check if str starts with start_str
bool String_StartsWith(const TCHAR *str, const TCHAR *start_str, bool ignore_case);

// check if str starts with start_str, for char *
bool String_StartsWithA(const char *str, const char *start_str, bool ignore_case);

// check if str ends with end_str
bool String_EndsWith(const TCHAR *str, const TCHAR *end_str, bool ignore_case);

// If the input string str doesn't already end with the string end_str,
// make it end with the string end_str.
CString String_MakeEndWith(const TCHAR *str, const TCHAR* end_str, bool ignore_case);

// converts an int to a string
CString String_Int64ToString(int64 value, int radix);

// converts an uint64 to a string
CString String_Uint64ToString(uint64 value, int radix);

// Convert numeric types to CString
CString sizet_to_str(const size_t & i);
CString itostr(const int i);
CString itostr(const uint32 i);

// converts a string to an  int
// Does not check for overflow
int32 String_StringToInt(const TCHAR * str);

int64 String_StringToInt64(const TCHAR * str);

// converts an double to a string
// specifies the number of digits after the decimal point
// TODO(omaha): Make this work for negative values
CString String_DoubleToString(double value, int point_digits);

// convert string to double
double String_StringToDouble (const TCHAR *s);

// Converts a character to a digit
// if the character is not a digit return -1
int32 String_CharToDigit(const TCHAR c);

// returns true if ASCII digit
bool String_IsDigit(const TCHAR c);

// Converts the digit to a character.
TCHAR String_DigitToChar(unsigned int n);

// Returns true if an identifier character: letter, digit, or "_"
bool String_IsIdentifierChar(const TCHAR c);

// Return the index of the first occurrence of s2 in s1, or -1 if none.
int String_FindString(const TCHAR *s1, const TCHAR *s2);
int String_FindString(const TCHAR *s1, const TCHAR *s2, int start_pos);

// Return the index of the first occurrence of c in s1, or -1 if none.
int String_FindChar(const TCHAR *str, const TCHAR c);
// start from index start_pos
int String_FindChar(const TCHAR *str, const TCHAR c, int start_pos);

bool String_Contains(const TCHAR *s1, const TCHAR *s2);

// Append the given character to the string if it doesn't already end with it.
// There must be room in the string to append the character if necessary.
void String_EndWithChar(TCHAR *str, TCHAR c);

// A special version of the replace function which takes advantage of CString properties
// to make it much faster when the string grows

// NOTE: it CANNOT match more than kMaxReplaceMatches instances within the string
// do not use this function if that is a possibility

// The maximum number of replacements to perform. Essentially infinite
const unsigned int kRepMax = std::numeric_limits<uint32_t>::max();
int ReplaceCString (CString & src, const TCHAR *from, unsigned int from_len,
                                   const TCHAR *to, unsigned int to_len,
                                   unsigned int max_matches);

// you really want to use the straight TCHAR version above. you know it
// on memory allocation error, returns the original string
int ReplaceCString (CString & src, const TCHAR *from, const TCHAR *to);

// Serializes a time64 to a string, and then loads it out again, this string it not for human consumption
time64 StringToTime(const CString & time);
CString TimeToString(const time64 & time);

bool IsAlphaA (const char c);
bool IsDigitA (const char c);

inline TCHAR Char_ToLower(TCHAR c) {
  const TCHAR* result = ::CharLower(reinterpret_cast<TCHAR*>(c));
  ASSERT1(HIWORD(result) == 0);
  return LOWORD(result);
}

// @returns the lowercase character (type is int to be consistent with the CRT)
int String_ToLowerChar(int c);

// Replacement for the CRT tolower(c)
char String_ToLowerCharAnsi(char c);

// Escapes and unescapes strings (shlwapi-based implementation).
// The indended usage for these APIs is escaping strings to make up
// URLs, for example building query strings.
//
// Pass false to the flag segment_only to escape the url. This will not
// cause the conversion of the # (%23), ? (%3F), and / (%2F) characters.
HRESULT StringEscape(const CString& str_in,
                     bool segment_only,
                     CString* str_out);

HRESULT StringUnescape(const CString& str_in, CString* str_out);

// Converts a string to an int, performs all the necessary
// checks to ensure that the string is correct.
// Tests for overflow and non-int strings.
bool String_StringToDecimalIntChecked(const TCHAR* str, int* value);

// Converts a string to a bool.
HRESULT String_StringToBool(const TCHAR* str, bool* value);

// Convert boolean to its string representation.
const TCHAR* String_BoolToString(bool value);

// Similar to ATL::CStringT::Replace() except that it ignores case.
CString String_ReplaceIgnoreCase(const CString& string,
                                 const CString& token,
                                 const CString& replacement);
// Converts a string to a Tristate enum.
bool String_StringToTristate(const TCHAR* str, Tristate* value);

// Extracts the name and value from a string that contains a name/value pair.
bool ParseNameValuePair(const CString& token, TCHAR separator,
                        CString* name, CString* value);

// Splits a command line buffer into two parts in place:
// first argument (which could be path to executable) and remaining arguments.
// Note that the same pointer can be used for both command_line and
// either of the remaining parameters.
bool SplitCommandLineInPlace(TCHAR *command_line,
                             TCHAR **first_argument,
                             TCHAR **remaining_arguments);

// Returns true if the unicode string only contains ascii values.
bool ContainsOnlyAsciiChars(const CString& str);
// Converts a buffer of bytes to a hex string.
CString BytesToHex(const uint8* bytes, size_t num_bytes);

// Converts a vector of bytes to a hex string.
CString BytesToHex(const std::vector<uint8>& bytes);

// Formats the specified message ID.
// It is similar to CStringT::FormatMessage() but it returns an empty string
// instead of throwing when the message ID cannot be loaded.
CString FormatResourceMessage(uint32 resource_id, ...);

// Formats an error code as an 8-digit HRESULT-style hex number or an unsigned
// integer depending on whether it matches the HRESULT failure format.
CString FormatErrorCode(DWORD error_code);

// Converts the unicode string into a utf8 encoded, urlencoded string.
// The resulting ascii string is returned in a wide CString.
HRESULT WideStringToUtf8UrlEncodedString(const CString& str, CString* out);

// Converts a string that is in the utf8 representation and is urlencoded
// into a unicode string.
HRESULT Utf8UrlEncodedStringToWideString(const CString& str, CString* out);

// ----------------------------------------------------------------------
// a2b_hex()
//  Description: Ascii-to-Binary hex conversion.  This converts
//         2*'num' hexadecimal characters to 'num' binary data.
//        Return value: 'num' bytes of binary data (via the 'to' argument)
// ----------------------------------------------------------------------
void a2b_hex(const char* from, unsigned char* to, size_t num);
void a2b_hex(const char* from, char* to, size_t num);
void a2b_hex(const char* from, std::string* to, size_t num);
std::string a2b_hex(const std::string& a);

// ----------------------------------------------------------------------
// b2a_hex()
//  Description: Binary-to-Ascii hex conversion.  This converts
//   'num' bytes of binary to a 2*'num'-character hexadecimal representation
//    Return value: 2*'num' characters of ascii text (via the 'to' argument)
// ----------------------------------------------------------------------
void b2a_hex(const unsigned char* from, char* to, size_t num);
void b2a_hex(const unsigned char* from, std::string* to, size_t num);

// ----------------------------------------------------------------------
// b2a_hex()
//  Description: Binary-to-Ascii hex conversion.  This converts
//   'num' bytes of binary to a 2*'num'-character hexadecimal representation
//    Return value: 2*'num' characters of ascii string
// ----------------------------------------------------------------------
std::string b2a_hex(const char* from, size_t num);

// Convenience function that calls a2b_hex on a CString and outputs the result
// as a vector of bytes.  Returns true if the conversion was successful; false
// if not.  (Failure usually implies that there was a non-hex digit in the
// string, or that conversion from wide to narrow failed.)
bool SafeHexStringToVector(const CStringA& str, std::vector<uint8>* vec_out);
bool SafeHexStringToVector(const CStringW& str, std::vector<uint8>* vec_out);

}  // namespace omaha

#endif  // OMAHA_BASE_STRING_H_
