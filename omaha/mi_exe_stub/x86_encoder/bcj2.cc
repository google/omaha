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

#include <string>
#include <windows.h>
#include <intsafe.h>
#include <shellapi.h>

#include "base/basictypes.h"
#include "omaha/mi_exe_stub/x86_encoder/bcj2_encoder.h"
#include "third_party/smartany/scoped_any.h"

int wmain(int argc, WCHAR* argv[], WCHAR* env[]) {
  UNREFERENCED_PARAMETER(env);

  if (argc < 3) {
    return 1;
  }

  // argv[1] is the input file, argv[2] is the output file.
  scoped_hfile file(::CreateFile(argv[1],
                                 GENERIC_READ,
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 0,
                                 NULL));
  if (!valid(file)) {
    return 2;
  }

  LARGE_INTEGER file_size_data;
  if (!::GetFileSizeEx(get(file), &file_size_data)) {
    return 3;
  }

  DWORD file_size = static_cast<DWORD>(file_size_data.QuadPart);
  DWORD bytes_read = 0;

  std::string out1;
  std::string out2;
  std::string out3;
  std::string out4;

  {
    std::string buffer(file_size, '\0');
    if (!::ReadFile(get(file), buffer.data(), file_size, &bytes_read, NULL) ||
        bytes_read != file_size) {
      return 4;
    }

    if (!omaha::Bcj2Encode(buffer, &out1, &out2, &out3, &out4)) {
      return 5;
    }
  }

  // The format of BCJ2 file is very primitive.
  // Header is 5 32-bit ints, with the following information:
  //   unpacked size
  //   size of stream 1
  //   size of stream 2
  //   size of stream 3
  //   size of stream 4
  const size_t output_buffer_header[] = {
    bytes_read,
    out1.size(),
    out2.size(),
    out3.size(),
    out4.size(),
  };

  std::string out0(sizeof(output_buffer_header), '\0');
  if (0 != memcpy_s(out0.data(),
                    out0.size(),
                    &output_buffer_header[0],
                    sizeof(output_buffer_header))) {
    return 6;
  }

  if ((out0.size() + out1.size() + out2.size() + out3.size() + out4.size()) >
      DWORD_MAX) {
    return 7;
  }

  reset(file,
        ::CreateFile(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
  if (!valid(file)) {
    return 8;
  }

  for (const auto& out : { std::move(out0),
                           std::move(out1),
                           std::move(out2),
                           std::move(out3),
                           std::move(out4) }) {
    if (out.empty()) {
      continue;
    }

    DWORD bytes_written = 0;
    if (!::WriteFile(get(file),
                     out.data(),
                     out.size(),
                     &bytes_written,
                     NULL) ||
        bytes_written != out.size()) {
      return 9;
    }
  }

  return 0;
}
