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


#include "omaha/mi_exe_stub/tar.h"
#include <strsafe.h>

#define USTAR_MAGIC "ustar"
#define USTAR_OFFSET 257
#define USTAR_DONE "\0\0\0\0\0"

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];    // NOLINT
#define arraysize(array) (sizeof(ArraySizeHelper(array)))


Tar::Tar(const char *target_dir, HANDLE file_handle, bool delete_when_done)
    : target_directory_name_(target_dir),
      file_handle_(file_handle),
      delete_when_done_(delete_when_done),
      callback_(NULL),
      callback_context_(NULL) {}

Tar::~Tar() {
  for (int i = 0; i != files_to_delete_.GetSize(); ++i) {
    DeleteFile(files_to_delete_[i]);
  }
}

bool Tar::ExtractToDir() {
  bool done = false;
  do {
    if (!ExtractOneFile(&done)) {
      return false;
    }
  } while (!done);
  return true;
}

bool Tar::ExtractOneFile(bool *done) {
  USTARHeader header;
  DWORD bytes_handled;
  bool result = true;
  BOOL file_result;

  file_result = ReadFile(file_handle_, &header, sizeof(USTARHeader),
    &bytes_handled, NULL);
  if (!file_result || bytes_handled != sizeof(USTARHeader)) {
    return false;
  }
  if (0 == memcmp(header.magic, USTAR_DONE, arraysize(USTAR_DONE) - 1)) {
    // We're probably done, since we read the final block of all zeroes.
    *done = true;
    return true;
  }
  if (0 != memcmp(header.magic, USTAR_MAGIC, arraysize(USTAR_MAGIC) - 1)) {
    return false;
  }
  CString new_filename(target_directory_name_);
  new_filename += "\\";
  new_filename += header.name;
  HANDLE new_file = CreateFile(new_filename, GENERIC_WRITE, 0, NULL,
    CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
  if (new_file == INVALID_HANDLE_VALUE) {
    return false;
  }
  // We don't check for conversion errors because the input data is fixed at
  // build time, so it'll either always work or never work, and we won't ship
  // one that never works.
  DWORD tar_file_size = strtol(header.size, NULL, 8);
  DWORD next_offset = SetFilePointer(file_handle_, 0, NULL, FILE_CURRENT) +
      tar_file_size + (512 - tar_file_size & 0x1ff);
  while (tar_file_size > 0) {
    const int COPY_BUFFER_SIZE = 256 * 1024;
    char copy_buffer[COPY_BUFFER_SIZE];
    DWORD bytes_to_handle = COPY_BUFFER_SIZE;
    if (bytes_to_handle > tar_file_size) {
      bytes_to_handle = tar_file_size;
    }
    file_result = ReadFile(file_handle_, copy_buffer, bytes_to_handle,
      &bytes_handled, NULL);
    if (!file_result) {
      result = false;
      break;
    } else {
      file_result = WriteFile(new_file, copy_buffer, bytes_to_handle,
        &bytes_handled, NULL);
      if (!file_result) {
        result = false;
        break;
      } else {
        tar_file_size -= bytes_to_handle;
      }
    }
  }
  CloseHandle(new_file);
  if (result) {
    if (delete_when_done_) {
      files_to_delete_.Add(new_filename);
    }
    if (callback_ != NULL) {
      callback_(callback_context_, new_filename);
    }
  }

  if (INVALID_SET_FILE_POINTER != next_offset) {
    SetFilePointer(file_handle_, next_offset, NULL, FILE_BEGIN);
  }

  return result;
}
