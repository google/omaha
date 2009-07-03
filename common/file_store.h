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

#ifndef OMAHA_COMMON_FILE_STORE_H__
#define OMAHA_COMMON_FILE_STORE_H__

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

class FileStore {
 public:
  // Open the store
  bool Open(const TCHAR* file_path);

  // Close the store
  bool Close();

  // Clear the store
  bool Clear();

  // Read a value from the store
  bool Read(const TCHAR* name, std::vector<byte>* data) const;

  // Write a value to the store
  bool Write(const TCHAR* name, byte* data, int data_size);

  // Check to see a named value exists in the store
  bool Exists(const TCHAR* name) const;

  // Remove a value from the store
  bool Remove(const TCHAR* name);

  // Get the number of values for this store
  bool GetValueCount(uint32* value_count);

  // Get the value name for the given value name index
  bool GetValueNameAt(uint32 index, CString* value_name);

 private:
  CString file_path_;      // Full path to the file store
};

}  // namespace omaha

#endif  // OMAHA_COMMON_FILE_STORE_H__
