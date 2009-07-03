// Copyright 2007-2009 Google Inc.
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

#ifndef OMAHA_COMMON_APPLY_TAG_H__
#define OMAHA_COMMON_APPLY_TAG_H__

#include <atlbase.h>
#include <atlstr.h>

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/error.h"

namespace omaha {

// This regular expression represents the valid characters allowed in the
// binary tag.
// When changing this regular expression make sure that string patterns on
// the server are also updated.
const char* const kValidTagStringRegEx = "^[-%{}/&=.,_a-zA-Z0-9_]*$";

// Stamps the tag_string into the signed_exe_file.
// Appends the tag_string to the existing tag if append is
// true, else errors out. Note that we do not support a
// overwrite.
// The tagging is done by adding bytes to the signature
// directory in the PE flie.
// The modified signature directory looks something like this
// <Signature>Gact.<tag_len><tag_string>
// There are no restrictions on the tag_string, it is just treated
// as a sequence of bytes.
class ApplyTag {
 public:
  ApplyTag();
  HRESULT Init(const TCHAR* signed_exe_file,
               const char* tag_string,
               int tag_string_length,
               const TCHAR* tagged_file,
               bool append);
  HRESULT EmbedTagString();

 private:
  static uint32 GetUint32(const void* p);
  static void PutUint32(uint32 i, void* p);
  bool ReadExistingTag(std::vector<byte>* binary);
  bool CreateBufferToWrite();
  bool ApplyTagToBuffer(int* output_len);
  bool IsValidTagString(const char* tag_string);

  // The string to be tagged into the binary.
  std::vector<char> tag_string_;

  // Existing tag string inside the binary.
  std::vector<char> prev_tag_string_;

  // This is prev_tag_string_.size - 1, to exclude the terminating null.
  int prev_tag_string_length_;

  // Length of the certificate inside the binary.
  int prev_cert_length_;

  // The input binary to be tagged.
  CString signed_exe_file_;

  // The output binary name.
  CString tagged_file_;

  // Whether to append the tag string to the existing one.
  bool append_;

  // Internal buffer to hold the appended string.
  std::vector<char> tag_buffer_;

  // The output buffer that contains the original binary
  // data with the tagged information.
  std::vector<char> buffer_data_;

  DISALLOW_EVIL_CONSTRUCTORS(ApplyTag);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_APPLY_TAG_H__
