// Copyright 2005-2009 Google Inc.
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
// Unittests for pe_utils


#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/pe_utils.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(PEUtilsTest, PEUtils) {
  // Get some known directories
  CString windows_dir;
  CString temp_dir;
  DWORD dw = ::GetEnvironmentVariable(L"SystemRoot",
                                      CStrBuf(windows_dir, MAX_PATH), MAX_PATH);
  ASSERT_TRUE(dw);
  dw = ::GetEnvironmentVariable(L"TEMP", CStrBuf(temp_dir, MAX_PATH), MAX_PATH);
  ASSERT_TRUE(dw);

  // Get a known executable to play with
  CString notepad(windows_dir + L"\\NOTEPAD.EXE");
  ASSERT_TRUE(File::Exists(notepad));

  CString temp_exe(temp_dir + L"\\pe_utils_test.exe");
  if (File::Exists(temp_exe)) {
    File::Remove(temp_exe);
  }
  ASSERT_FALSE(File::Exists(temp_exe));
  ASSERT_SUCCEEDED(File::Copy(notepad, temp_exe, true));

  // Stomp on its checksum and check the result
  const unsigned int kChk1 = 0xFEE1BAD;
  const unsigned int kChk2 = 0x600DF00D;

  unsigned int checksum = 0;

  // Test Get/SetPEChecksum
  ASSERT_SUCCEEDED(SetPEChecksum(temp_exe, kChk1));
  ASSERT_SUCCEEDED(GetPEChecksum(temp_exe, &checksum));
  ASSERT_EQ(kChk1, checksum);

  ASSERT_SUCCEEDED(SetPEChecksum(temp_exe, kChk2));
  ASSERT_SUCCEEDED(GetPEChecksum(temp_exe, &checksum));
  ASSERT_EQ(kChk2, checksum);

  // Test GetPEChecksumFromBuffer/SetPEChecksumToBuffer
  std::vector<byte> buffer;
  ASSERT_SUCCEEDED(ReadEntireFile(temp_exe, 0, &buffer));

  int buffer_data_len = buffer.size();
  uint8 *buffer_data = reinterpret_cast<uint8*>(&buffer.front());

  ASSERT_SUCCEEDED(SetPEChecksumToBuffer(buffer_data, buffer_data_len, kChk1));
  ASSERT_SUCCEEDED(GetPEChecksumFromBuffer(buffer_data,
                                           buffer_data_len,
                                           &checksum));
  ASSERT_EQ(kChk1, checksum);

  ASSERT_SUCCEEDED(SetPEChecksumToBuffer(buffer_data, buffer_data_len, kChk2));
  ASSERT_SUCCEEDED(GetPEChecksumFromBuffer(buffer_data,
                                           buffer_data_len,
                                           &checksum));
  ASSERT_EQ(kChk2, checksum);

  // Clean up
  ASSERT_SUCCEEDED(File::Remove(temp_exe));
}

}  // namespace omaha

