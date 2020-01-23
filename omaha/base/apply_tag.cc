// Copyright 2006-2009 Google Inc.
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
// Applies a tag to a signed file.

#include "omaha/base/apply_tag.h"

#include <intsafe.h>
#include <algorithm>
#include <regex>
#include <vector>

#include "omaha/base/utils.h"
#include "omaha/base/extractor.h"

namespace omaha {

// TODO(omaha3): There is duplication between this code and the Extractor class.
// Would be nice to eliminate or reduce the duplication.
const char kMagicBytes[]    = "Gact2.0Omaha";
const size_t kMagicBytesLen = arraysize(kMagicBytes) - 1;

const uint32 kPEHeaderOffset = 60;

ApplyTag::ApplyTag()
    : prev_tag_string_length_(0),
      append_(0) {}

bool ApplyTag::IsValidTagString(const char* tag_string) {
  ASSERT1(tag_string);
  return std::regex_match(tag_string, std::regex(kValidTagStringRegEx));
}

HRESULT ApplyTag::Init(const TCHAR* signed_exe_file,
                       const char* tag_string,
                       int tag_string_length,
                       const TCHAR* tagged_file,
                       bool append) {
  ASSERT1(signed_exe_file);
  ASSERT1(tag_string);
  ASSERT1(tagged_file);

  signed_exe_file_ = signed_exe_file;
  tagged_file_ = tagged_file;
  append_ = append;

  // Check the tag_string for invalid characters.
  if (!IsValidTagString(tag_string)) {
    return E_INVALIDARG;
  }

  for (int i = 0; i < tag_string_length; ++i) {
    tag_string_.push_back(tag_string[i]);
  }

  return S_OK;
}

HRESULT ApplyTag::EmbedTagString() {
  std::vector<byte> input_file_buffer;
  HRESULT hr = ReadEntireFileShareMode(signed_exe_file_,
                                       0,
                                       FILE_SHARE_READ,
                                       &input_file_buffer);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(!input_file_buffer.empty());
  VERIFY1(ReadExistingTag(&input_file_buffer));
  if (!append_ && prev_tag_string_length_) {
    // If there is a previous tag and the append flag is not set, then
    // we should error out.
    return APPLYTAG_E_ALREADY_TAGGED;
  }

  if (!CreateBufferToWrite()) {
    return E_FAIL;
  }

  buffer_data_.resize(input_file_buffer.size());

  copy(input_file_buffer.begin(),
       input_file_buffer.end(),
       buffer_data_.begin());

  // Applying tags require the file be signed with Authenticode and have a
  // padded certificate that contains kMagicBytes.
  if (!ApplyTagToBuffer()) {
    return APPLYTAG_E_NOT_SIGNED;
  }

  return WriteEntireFile(tagged_file_, buffer_data_);
}

uint32 ApplyTag::GetUint32(const void* p) {
  ASSERT1(p);

  const uint32* pu = reinterpret_cast<const uint32*>(p);
  return *pu;
}

void ApplyTag::PutUint32(uint32 i, void* p) {
  ASSERT1(p);

  uint32* pu = reinterpret_cast<uint32*>(p);
  *pu = i;
}

bool ApplyTag::ReadExistingTag(std::vector<byte>* binary) {
  ASSERT1(binary);

  int len = 0;
  TagExtractor tag;
  char* bin = reinterpret_cast<char*>(&binary->front());
  ASSERT1(bin);
  if (tag.ExtractTag(bin, binary->size(), NULL, &len)) {
    prev_tag_string_.resize(len);
    if (tag.ExtractTag(bin, binary->size(), &prev_tag_string_.front(), &len)) {
      // The extractor returns the actual length
      // of the string + 1 for the terminating null.
      prev_tag_string_length_ = len - 1;
    }
  }

  return true;
}

bool ApplyTag::CreateBufferToWrite() {
  ASSERT1(!append_ && !prev_tag_string_length_ || append_);
  ASSERT1(!tag_string_.empty());
  ASSERT1(!prev_tag_string_.size() ||
          prev_tag_string_.size() ==
          static_cast<size_t>(prev_tag_string_length_ + 1));

  // Build the tag buffer.
  // The format of the tag buffer is:
  // 000000-00000B: 12-byte magic (big-endian)
  // 00000C-00000D: unsigned 16-bit int string length (big-endian)
  // 00000E-??????: ASCII string
  size_t tag_string_len = tag_string_.size() + prev_tag_string_length_;
  size_t tag_header_len = kMagicBytesLen + 2;
  size_t tag_buffer_length = tag_string_len + tag_header_len;

  tag_buffer_.clear();
  tag_buffer_.resize(tag_buffer_length, 0);
  memcpy(&tag_buffer_.front(), kMagicBytes, kMagicBytesLen);
  tag_buffer_[kMagicBytesLen] =
      static_cast<char>((tag_string_len & 0xff00) >> 8);
  tag_buffer_[kMagicBytesLen+1] = static_cast<char>(tag_string_len & 0xff);

  if (prev_tag_string_length_ > 0) {
    copy(prev_tag_string_.begin(),
         prev_tag_string_.end(),
         tag_buffer_.begin() + tag_header_len);
  }

  copy(tag_string_.begin(),
       tag_string_.end(),
       tag_buffer_.begin() + tag_header_len + prev_tag_string_length_);
  ASSERT1(static_cast<int>(tag_buffer_.size()) == tag_buffer_length);

  return true;
}

bool ApplyTag::ApplyTagToBuffer() {
  uint32 peheader = GetUint32(&buffer_data_.front() + kPEHeaderOffset);
  uint32 kCertDirAddressOffset = 152;
  uint32 kCertDirInfoSize = 4 + 4;

  ASSERT1(peheader + kCertDirAddressOffset + kCertDirInfoSize <=
          buffer_data_.size());

  // Read certificate directory info.
  uint32 cert_dir_offset = GetUint32(&buffer_data_.front() + peheader +
                                     kCertDirAddressOffset);
  if (cert_dir_offset == 0) {
    return false;
  }

  uint32 cert_dir_len = GetUint32(&buffer_data_.front() + peheader +
                                  kCertDirAddressOffset + 4);
  ASSERT1(cert_dir_offset + cert_dir_len == buffer_data_.size());

  // Find kMagicBytes and add the tag buffer there.
  const byte* cert_dir_start = &buffer_data_.front() + cert_dir_offset;
  const byte* cert_dir_end = cert_dir_start + cert_dir_len;
  const byte* mc = std::search(cert_dir_start,
                               cert_dir_end,
                               kMagicBytes,
                               kMagicBytes + kMagicBytesLen);
  if (mc >= cert_dir_end) {
    return false;
  }

  // Copy the tag buffer.
  copy(tag_buffer_.begin(), tag_buffer_.end(),
       buffer_data_.begin() + (mc - &buffer_data_.front()));

  return true;
}

}  // namespace omaha
