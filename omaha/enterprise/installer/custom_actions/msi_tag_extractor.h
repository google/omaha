// Copyright 2013 Google Inc.
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


#ifndef OMAHA_ENTERPRISE_INSTALLER_CUSTOM_ACTIONS_MSI_TAG_EXTRACTOR_H_
#define OMAHA_ENTERPRISE_INSTALLER_CUSTOM_ACTIONS_MSI_TAG_EXTRACTOR_H_

#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"

namespace custom_action {

// This class is used to extract the tags at the end of MSI files.
// It is empirical observation that appending bytes at the end of the
// msi package will not break the authenticode.
//
// Tag specification:
//   - When reading multi-byte numbers, it's in big endian.
//   - Tag area begins with magic number 'Gact2.0Omaha'.
//   - The next 2-bytes is tag string length.
//   - Then follows the real tag string. The string is in format of:
//     "key1=value1&key2=value2". Both the key and the value must be
//     alpha-numeric ASCII strings for type 0 tag.
//
// A sample layout:
// +-------------------------------------+
// ~    ..............................   ~
// |    ..............................   |
// |    Other parts of MSI file          |
// +-------------------------------------+
// | Start of the certificate             |
// ~    ..............................   ~
// ~    ..............................   ~
// | Magic number 'Gact2.0Omaha'         | Tag starts
// | Tag length (2 bytes in big-endian)) |
// | tag string                          |
// +-------------------------------------+
//
// A real example (an MSI file tagged with 'brand=CDCD&key2=Test'):
// +-----------------------------------------------------------------+
// |  G   a   c   t   2   .   0   O   m   a   h   a  0x0 0x14 b   r  |
// |  a   n   d   =   C   D   C   D   &   k   e   y   2   =   T   e  |
// |  s   t                                                          |
// +-----------------------------------------------------------------+
class MsiTagExtractor {
 public:
  MsiTagExtractor();

  // Reads tag from file, parse it and store the key/value pair into member.
  bool ReadTagFromFile(const wchar_t* filename);

  // Gets the value of the given key from the MSI tag value mapping.
  bool GetValue(const char* key, std::string* value) const;

 private:
  bool ReadTagToBuffer(HANDLE file_handle, std::vector<char>* tag) const;
  bool ParseTagBuffer(const std::vector<char>& tag_buffer);
  bool ParseMagicNumber(const std::vector<char>& tag_buffer,
                        size_t* parse_position) const;
  uint16 ParseTagLength(const std::vector<char>& tag_buffer,
                        size_t* parse_position) const;
  void ParseSimpleAsciiStringMap(const std::vector<char>& tag_buffer,
                                 size_t* parse_position,
                                 size_t tag_length);
  void ParseKeyValueSubstring(const std::string& key_value_str);

  std::map<std::string, std::string> tag_map_;

  DISALLOW_COPY_AND_ASSIGN(MsiTagExtractor);
};

}  // namespace custom_action

#endif  // OMAHA_ENTERPRISE_INSTALLER_CUSTOM_ACTIONS_MSI_TAG_EXTRACTOR_H_
