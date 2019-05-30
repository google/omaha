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
#include <shellapi.h>
#include <memory>

#include "base/basictypes.h"
#include "third_party/smartany/scoped_any.h"

extern "C" {
#include "third_party/lzma/files/C/Bra.h"
}

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
  std::unique_ptr<uint8[]> buffer(new uint8[file_size]);
  DWORD bytes_read = 0;
  if (!::ReadFile(get(file), buffer.get(), file_size, &bytes_read, NULL) ||
      bytes_read != file_size) {
    return 4;
  }

  uint32 conversion_state;
  x86_Convert_Init(conversion_state);
  // processed might be less than bytes read. This is apparently OK, although
  // I don't understand why!
  const int encoding = 1;
  uint32 processed = static_cast<uint32>(
      x86_Convert(buffer.get(), bytes_read, 0, &conversion_state, encoding));
  reset(file, ::CreateFile(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0,
                           NULL));
  if (!valid(file)) {
    return 6;
  }

  DWORD bytes_written = 0;
  if (!::WriteFile(get(file), buffer.get(), bytes_read, &bytes_written, NULL)) {
    return 7;
  }

  return 0;
}
