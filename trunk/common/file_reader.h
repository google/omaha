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

#ifndef OMAHA_COMMON_FILE_READER_H_
#define OMAHA_COMMON_FILE_READER_H_

#include <windows.h>
#include <tchar.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/file.h"

namespace omaha {

// Allows one to read files quickly and easily line by line.
class FileReader {
 public:
  FileReader();
  ~FileReader();

  // Specifies the underlying file name and the size of internal read buffer.
  HRESULT Init(const TCHAR* file_name, size_t buffer_size);

  // Reads one line of text from the file, taking advantage of the internal
  // file buffer to optimize I/O reads. It reads at most max_len - 1 characters
  // and it always terminates the line.
  HRESULT ReadLineAnsi(size_t max_len, char* line);
  HRESULT ReadLineString(CString* line);

 private:
  HRESULT GetNextChar(bool peek, CString* next_char);
  size_t file_buffer_size() const { return file_buffer_size_; }

  File file_;
  bool file_is_open_;
  size_t buffered_byte_count_;          // How many bytes are in the buffer.
  size_t current_position_;             // An index into the buffer.
  scoped_array<byte> file_buffer_;      // A buffer (cache) of the file.
  size_t file_buffer_size_;             // How much of the file to slurp
                                        // in on each read.
  bool is_unicode_;

  DISALLOW_EVIL_CONSTRUCTORS(FileReader);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_FILE_READER_H_
