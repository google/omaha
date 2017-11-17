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

#ifndef OMAHA_THIRD_PARTY_CHROME_FILES_SRC_COMPONENTS_CRX_FILE_ID_UTIL_H_
#define OMAHA_THIRD_PARTY_CHROME_FILES_SRC_COMPONENTS_CRX_FILE_ID_UTIL_H_

#include <stddef.h>

#include <string>

namespace base {
class FilePath;
}

namespace crx_file {
namespace id_util {

// The number of bytes in a legal id.
extern const size_t kIdSize;

// From https://cs.chromium.org/chromium/src/base/strings
// /string_number_conversions.cc.
// Returns a hex string representation of a binary buffer. The returned hex
// string will be in upper case. This function does not check if |size| is
// within reasonable limits since it's written with trusted data in mind.  If
// you suspect that the data you want to format might be large, the absolute
// max size for |size| should be:
//   std::numeric_limits<size_t>::max() / 2
std::string HexEncode(const void* bytes, size_t size);

// Generates an extension ID from arbitrary input. The same input string will
// always generate the same output ID.
std::string GenerateId(const std::string& input);

// Generates an ID from a HEX string. The same input string will always generate
// the same output ID.
std::string GenerateIdFromHex(const std::string& input);

// Checks if |id| is a valid extension-id. Extension-ids are used for anything
// that comes in a CRX file, including apps, extensions, and components.
bool IdIsValid(const std::string& id);

}  // namespace id_util
}  // namespace crx_file

#endif  // OMAHA_THIRD_PARTY_CHROME_FILES_SRC_COMPONENTS_CRX_FILE_ID_UTIL_H_
