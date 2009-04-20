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

#include "omaha/common/file_reader.h"
#include "omaha/common/debug.h"

namespace omaha {

FileReader::FileReader()
    : file_is_open_(false),
      buffered_byte_count_(0),
      current_position_(0),
      file_buffer_size_(0),
      is_unicode_(false) {}

FileReader::~FileReader() {
  if (file_is_open_) {
    file_.Close();
    file_is_open_ = false;
  }
}

HRESULT FileReader::Init(const TCHAR* file_name, size_t buffer_size) {
  ASSERT1(file_name);
  ASSERT1(buffer_size);
  file_buffer_size_ = buffer_size;
  file_buffer_.reset(new byte[file_buffer_size()]);
  HRESULT hr = file_.OpenShareMode(file_name, false, false, FILE_SHARE_WRITE |
                                                            FILE_SHARE_READ);
  file_is_open_ = SUCCEEDED(hr);
  is_unicode_ = false;

  if (FAILED(hr)) {
    return hr;
  }

  hr = file_.SeekToBegin();
  if (FAILED(hr)) {
    return hr;
  }

  const int unicode_header_length = 2;

  char buf[unicode_header_length] = {0};
  uint32 bytes_read = 0;
  hr = file_.Read(sizeof(buf), reinterpret_cast<byte*>(buf), &bytes_read);
  if (FAILED(hr)) {
    return hr;
  }

  if (bytes_read == sizeof(buf)) {
    char unicode_buf[unicode_header_length] = {0xff, 0xfe};
    is_unicode_ = (memcmp(buf, unicode_buf, sizeof(buf)) == 0);
  }

  if (!is_unicode_) {
    file_.SeekToBegin();
  }

  if (is_unicode_ && (buffer_size < sizeof(WCHAR))) {
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  return S_OK;
}

HRESULT FileReader::GetNextChar(bool peek, CString* next_char) {
  ASSERT1(next_char);
  next_char->Empty();

  // Do we need to read in more of the file?
  if (current_position_ >= buffered_byte_count_) {
    current_position_ = 0;
    if (FAILED(file_.Read(file_buffer_size(),
                          file_buffer_.get(),
                          &buffered_byte_count_))) {
      // There is no more of the file buffered.
      buffered_byte_count_ = 0;
    }
  }

  // Have we gone past the end of the file?
  if (current_position_ >= buffered_byte_count_) {
    return E_FAIL;
  }

  if (is_unicode_) {
    // Need to make sure there are at least 2 characters still in the buffer.
    // If we're right at the end of the buffer and there's only one character,
    // then we will need to read more from the file.

    if (current_position_ + 1 >= buffered_byte_count_) {
      // We need one more byte to make a WCHAR.
      // Due to the need to peek, we're going to take that byte and put it at
      // the beginning of the file_buffer_ and read in as many remaining bytes
      // as we can from the file.

      // Copy current (and last) byte to the beginning of the buffer.
      file_buffer_[0] = file_buffer_[current_position_];

      // Reset current_position.
      current_position_ = 0;

      if (SUCCEEDED(file_.Read(file_buffer_size() - 1,
                               file_buffer_.get() + 1,
                               &buffered_byte_count_))) {
        // Incrememt count to deal with byte we pre-filled at offset 0.
        buffered_byte_count_++;
      } else {
        // We've got a Unicode file with an extra byte.  We're going to drop the
        // byte and call it end of file.
        buffered_byte_count_ = 0;
        return E_FAIL;
      }
    }

    // Get the next character.
    char c1 = file_buffer_[current_position_];
    ++current_position_;
    char c2 = file_buffer_[current_position_];
    ++current_position_;

    if (peek) {
      // Reset the current position pointer backwards if we're peeking.
      current_position_ -= 2;
    }

    WCHAR c = (static_cast<WCHAR>(c2) << 8) | static_cast<WCHAR>(c1);

    *next_char = c;
  } else {
    char c = file_buffer_[current_position_];
    if (!peek) {
      ++current_position_;
    }
    *next_char = c;
  }

  return S_OK;
}

HRESULT FileReader::ReadLineString(CString* line) {
  ASSERT1(line);

  line->Empty();

  while (true) {
    CString current_char;
    HRESULT hr = GetNextChar(false, &current_char);
    // If we failed to get the next char, we're at the end of the file.
    // If the current line is empty, then fail out signalling we're done.
    // Otherwise, return the current line and we'll fail out on the next call to
    // ReadLine().
    if (FAILED(hr)) {
      if (line->IsEmpty()) {
        return hr;
      } else {
        return S_OK;
      }
    }

    // Have we reached end of line?
    if (current_char.Compare(_T("\r")) == 0) {
      // Seek ahead to see if the next char is "\n"
      CString next_char;
      GetNextChar(true, &next_char);
      if (next_char.Compare(_T("\n")) == 0) {
        // Get in the next char too.
        GetNextChar(false, &next_char);
      }
      break;
    } else if (current_char.Compare(_T("\n")) == 0) {
      break;
    }

    line->Append(current_char);
  }

  return S_OK;
}

HRESULT FileReader::ReadLineAnsi(size_t max_len, char* line) {
  ASSERT1(line);
  ASSERT1(max_len);

  size_t total_len = 0;

  while (true) {
    // Do we need to read in more of the file?
    if (current_position_ >= buffered_byte_count_) {
      current_position_ = 0;
      if (FAILED(file_.Read(file_buffer_size(),
                            file_buffer_.get(),
                            &buffered_byte_count_))) {
        // There is no more of the file buffered.
        buffered_byte_count_ = 0;
      }
    }

    // Have we gone past the end of the file?
    if (current_position_ >= buffered_byte_count_) {
      break;
    }

    // Get the next character.
    char c = file_buffer_[current_position_];
    ++current_position_;

    // Have we reached end of line?
    // TODO(omaha): if the line is terminated with a \r\n pair then perhaps
    // the code should skip the whole pair not only half of it.
    if (c == '\n' || c == '\r') {
      break;
    }

    // Fill up the passed in buffer for the line.
    if (total_len < max_len - 1) {
      line[total_len] = c;
      ++total_len;
    }
  }
  // Terminate the passed in buffer.
  ASSERT1(total_len < max_len);
  line[total_len] = '\0';

  // If we are out of bytes and we didn't read in any bytes.
  // then fail signaling end of file.
  if (!buffered_byte_count_ && !total_len) {
    return E_FAIL;
  }

  return S_OK;
}

}  // namespace omaha

