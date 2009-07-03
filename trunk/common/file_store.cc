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
// Defines class FileStore

#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/file.h"
#include "omaha/common/file_store.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

// Open the store
bool FileStore::Open(const TCHAR* file_path) {
  file_path_ = String_MakeEndWith(file_path, _T("\\"), false);
  return true;
}

// Close the store
bool FileStore::Close() {
  file_path_.Empty();
  return true;
}

// Clear the store
bool FileStore::Clear() {
  if (File::Exists(file_path_) && File::IsDirectory(file_path_)) {
    return SUCCEEDED(DeleteDirectoryFiles(file_path_));
  } else {
    return true;
  }
}

// Read a value from the store
bool FileStore::Read(const TCHAR* name, std::vector<byte>* data) const {
  ASSERT1(name);
  ASSERT1(data);

  return Exists(name) && SUCCEEDED(ReadEntireFile(file_path_ + name, 0, data));
}

// Write a value to the store
bool FileStore::Write(const TCHAR* name, byte* data, int data_size) {
  ASSERT1(name);
  ASSERT1(data);
  ASSERT1(data_size);

  std::vector<byte> buffer(data_size);
  memcpy(&buffer.front(), data, data_size);

  return SUCCEEDED(WriteEntireFile(file_path_ + name, buffer));
}

// Check to see a named value exists in the store
bool FileStore::Exists(const TCHAR* name) const {
  ASSERT1(name);

  return File::Exists(file_path_ + name);
}

// Remove a value from the store
bool FileStore::Remove(const TCHAR* name) {
  ASSERT1(name);

  return SUCCEEDED(File::Remove(file_path_ + name));
}

// Get the number of values for this store
bool FileStore::GetValueCount(uint32* value_count) {
  ASSERT1(value_count);

  std::vector<CString> matching_paths;

  if (FAILED(File::GetWildcards(file_path_, _T("*"), &matching_paths))) {
    return false;
  }

  *value_count = matching_paths.size();

  return true;
}

// Get the value name for the given value name index
bool FileStore::GetValueNameAt(uint32 index, CString* value_name) {
  ASSERT1(value_name);

  std::vector<CString> matching_paths;

  if (FAILED(File::GetWildcards(file_path_, _T("*"), &matching_paths))) {
    return false;
  }
  if (index >= matching_paths.size()) {
    return false;
  }

  *value_name = matching_paths[index].Mid(file_path_.GetLength());

  return true;
}

}  // namespace omaha

