// Copyright 2009 Google Inc.
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
// BCJ encodes a file to increase its compressibility.

#include <windows.h>
#include <intsafe.h>
#include <shellapi.h>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/mi_exe_stub/x86_encoder/bcj2_encoder.h"
#include "third_party/smartany/scoped_any.h"

int wmain(int argc, WCHAR* argv[], WCHAR* env[]) {
  UNREFERENCED_PARAMETER(env);

  if (argc < 3) {
    return 1;
  }

  // argv[1] is the input file, argv[2] is the output file.
  scoped_hfile file(::CreateFile(argv[1], GENERIC_READ, 0,
                                 NULL, OPEN_EXISTING, 0, NULL));
  if (!valid(file)) {
    return 2;
  }

  LARGE_INTEGER file_size_data;
  if (!::GetFileSizeEx(get(file), &file_size_data)) {
    return 3;
  }

  DWORD file_size = static_cast<DWORD>(file_size_data.QuadPart);
  scoped_array<uint8> buffer(new uint8[file_size]);
  DWORD bytes_read = 0;
  if (!::ReadFile(get(file), buffer.get(), file_size, &bytes_read, NULL) ||
      bytes_read != file_size) {
    return 4;
  }

  std::string out1;
  std::string out2;
  std::string out3;
  std::string out4;
  if (!omaha::Bcj2Encode(std::string(reinterpret_cast<char*>(buffer.get()),
                                     file_size),
                         &out1, &out2, &out3, &out4)) {
    return 5;
  }

  // The format of BCJ2 file is very primitive.
  // Header is 5 32-bit ints, with the following information:
  //   unpacked size
  //   size of stream 1
  //   size of stream 2
  //   size of stream 3
  //   size of stream 4
  const size_t output_buffer_length = 5 * sizeof(uint32) +  // NOLINT
      out1.size() + out2.size() + out3.size() + out4.size();
  if (output_buffer_length > DWORD_MAX) {
    return 13;
  }

  size_t buffer_remaining = output_buffer_length;
  scoped_array<uint8> output_buffer(new uint8[output_buffer_length]);
  uint8* p = output_buffer.get();
  *reinterpret_cast<uint32*>(p) = bytes_read;
  p += sizeof(uint32);                                                // NOLINT
  buffer_remaining -= sizeof(uint32);                                 // NOLINT
  *reinterpret_cast<uint32*>(p) = static_cast<uint32>(out1.size());
  p += sizeof(uint32);                                                // NOLINT
  buffer_remaining -= sizeof(uint32);                                 // NOLINT
  *reinterpret_cast<uint32*>(p) = static_cast<uint32>(out2.size());
  p += sizeof(uint32);                                                // NOLINT
  buffer_remaining -= sizeof(uint32);                                 // NOLINT
  *reinterpret_cast<uint32*>(p) = static_cast<uint32>(out3.size());
  p += sizeof(uint32);                                                // NOLINT
  buffer_remaining -= sizeof(uint32);                                 // NOLINT
  *reinterpret_cast<uint32*>(p) = static_cast<uint32>(out4.size());
  p += sizeof(uint32);                                                // NOLINT
  buffer_remaining -= sizeof(uint32);                                 // NOLINT
  if (!out1.empty()) {
    if (0 != memcpy_s(p, buffer_remaining, &out1[0], out1.size())) {
      return 9;
    }
    p += out1.size();
    buffer_remaining -= out1.size();
  }
  if (!out2.empty()) {
    if (0 != memcpy_s(p, buffer_remaining, &out2[0], out2.size())) {
      return 10;
    }
    p += out2.size();
    buffer_remaining -= out2.size();
  }
  if (!out3.empty()) {
    if (0 != memcpy_s(p, buffer_remaining, &out3[0], out3.size())) {
      return 11;
    }
    p += out3.size();
    buffer_remaining -= out3.size();
  }
  if (!out4.empty()) {
    if (0 != memcpy_s(p, buffer_remaining, &out4[0], out4.size())) {
      return 12;
    }
    p += out4.size();
    buffer_remaining -= out4.size();
  }
  if (p != output_buffer.get() + output_buffer_length ||
      0 != buffer_remaining) {
    return 8;
  }

  reset(file, ::CreateFile(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0,
                           NULL));
  if (!valid(file)) {
    return 6;
  }

  DWORD bytes_written = 0;
  if (!::WriteFile(get(file),
                   output_buffer.get(),
                   static_cast<DWORD>(output_buffer_length),
                   &bytes_written, NULL)) {
    return 7;
  }

  return 0;
}
