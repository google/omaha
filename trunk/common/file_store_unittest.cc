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
// Unit test for the file_store.



#include <shlobj.h>
#include "base/basictypes.h"
#include "omaha/common/file_store.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR kFilePathPrefix[] = _T("unittest");
const TCHAR kFileName1[] = _T("fname1");
const TCHAR kFileName2[] = _T("fname2");
char kFileContent[] = "1234567890abcdefg";

TEST(FileStoreTest, FileStore) {
  // Create a temp dir
  TCHAR temp_path[MAX_PATH];
  *temp_path = 0;
  ASSERT_LT(0u, ::GetTempPath(MAX_PATH, temp_path));

  CString temp_dir;
  temp_dir.Format(_T("%s%s%x"), temp_path, kFilePathPrefix, ::GetTickCount());
  ASSERT_EQ(::SHCreateDirectoryEx(0, temp_dir, 0), ERROR_SUCCESS);
  ON_SCOPE_EXIT(DeleteDirectory, temp_dir);

  // Test the file store
  FileStore file_store;
  ASSERT_TRUE(file_store.Open(temp_dir));

  // Make sure the folder is empty
  uint32 value_count;
  ASSERT_TRUE(file_store.GetValueCount(&value_count));
  ASSERT_EQ(value_count, 0);
  CString value_name;
  ASSERT_FALSE(file_store.GetValueNameAt(0, &value_name));

  // Write 2 files
  std::vector<byte> buffer;
  ASSERT_TRUE(file_store.Write(kFileName1,
                               reinterpret_cast<byte*>(kFileContent),
                               arraysize(kFileContent)));
  ASSERT_TRUE(file_store.Exists(kFileName1));
  ASSERT_TRUE(file_store.GetValueCount(&value_count));
  ASSERT_EQ(value_count, 1);
  ASSERT_TRUE(file_store.GetValueNameAt(0, &value_name));
  ASSERT_TRUE(value_name == kFileName1);
  ASSERT_TRUE(file_store.Read(kFileName1, &buffer));
  ASSERT_TRUE(memcmp(kFileContent,
                     &buffer.front(),
                     arraysize(kFileContent)) == 0);

  ASSERT_TRUE(file_store.Write(kFileName2,
                               reinterpret_cast<byte*>(kFileContent),
                               arraysize(kFileContent)));
  ASSERT_TRUE(file_store.Exists(kFileName2));
  ASSERT_TRUE(file_store.GetValueCount(&value_count));
  ASSERT_EQ(value_count, 2);
  ASSERT_TRUE(file_store.GetValueNameAt(1, &value_name));
  ASSERT_TRUE(value_name == kFileName2);
  ASSERT_TRUE(file_store.Read(kFileName2, &buffer));
  ASSERT_TRUE(memcmp(kFileContent,
                     &buffer.front(),
                     arraysize(kFileContent)) == 0);

  // Remove files
  ASSERT_TRUE(file_store.Remove(kFileName1));
  ASSERT_FALSE(file_store.Exists(kFileName1));
  ASSERT_TRUE(file_store.GetValueCount(&value_count));
  ASSERT_EQ(value_count, 1);
  ASSERT_TRUE(file_store.Remove(kFileName2));
  ASSERT_FALSE(file_store.Exists(kFileName2));
  ASSERT_TRUE(file_store.GetValueCount(&value_count));
  ASSERT_EQ(value_count, 0);

  ASSERT_TRUE(file_store.Close());
}

}  // namespace omaha

