// Copyright 2017 Google Inc.
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
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/id_util.h"

#include <atlconv.h>

#include <stdint.h>

#include <algorithm>
#include <cctype>

#include "omaha/base/security/sha256.h"
#include "omaha/base/string.h"
#include "omaha/net/cup_ecdsa_utils.h"

namespace {

// Converts a normal hexadecimal string into the alphabet used by extensions.
// We use the characters 'a'-'p' instead of '0'-'f' to avoid ever having a
// completely numeric host, since some software interprets that as an IP
// address.
static void ConvertHexadecimalToIDAlphabet(std::string* id) {
  for (size_t i = 0; i < id->size(); ++i) {
    std::string digit_string(1, (*id)[i]);
    TCHAR digit(CA2T(digit_string.c_str())[0]);
    (*id)[i] = 'a' +
               (omaha::IsHexDigit(digit) ? omaha::HexDigitToInt(digit) : 0);
  }
}

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
char ToLowerChar(char c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

}  // namespace

namespace crx_file {
namespace id_util {

// First 16 bytes of SHA256 hashed public key.
const size_t kIdSize = 16;

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

std::string ToLowerASCII(const std::string& s) {
  std::string ret(s.size(), '\0');
  std::transform(s.begin(), s.end(), ret.begin(), ToLowerChar);
  return ret;
}

std::string GenerateId(const std::string& input) {
  uint8_t hash[kIdSize] = {};
  std::vector<uint8_t> hash_out;
  VERIFY1(
      omaha::internal::SafeSHA256Hash(input.data(), input.size(), &hash_out));
  std::copy_n(hash_out.begin(), arraysize(hash), hash);
  return GenerateIdFromHex(HexEncode(hash, arraysize(hash)));
}

std::string GenerateIdFromHex(const std::string& input) {
  std::string output = ToLowerASCII(input);
  ConvertHexadecimalToIDAlphabet(&output);
  return output;
}

bool IdIsValid(const std::string& id) {
  // Verify that the id is legal.
  if (id.size() != (crx_file::id_util::kIdSize * 2)) {
    return false;
  }

  // We only support lowercase IDs, because IDs can be used as URL components
  // (where GURL will lowercase it).
  std::string temp = ToLowerASCII(id);
  for (size_t i = 0; i < temp.size(); i++)
    if (temp[i] < 'a' || temp[i] > 'p')
      return false;

  return true;
}

}  // namespace id_util
}  // namespace crx_file
