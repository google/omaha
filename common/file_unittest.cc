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
//
// File unittest

#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/timer.h"
#include "omaha/common/tr_rand.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// TODO(omaha): test error-prone functions such as ReadLineAnsi

namespace {

#define kIEBrowserExe \
  _T("C:\\PROGRAM FILES\\Internet Explorer\\iexplore.exe")

#define kIEBrowserQuotedExe \
  _T("\"") kIEBrowserExe _T("\"")

}  // namespace

void SimpleTest(bool async) {
  File f;

  char buf[1000] = "test";
  uint32 len1 = 4;

  char buf2[1000] = "aaaa";
  uint32 len2 = 4;

  char buf3[1000] = "bbbb";
  uint32 len3 = 4;

  char buf4[1000] = "    ";
  uint32 len4 = 4;

  CString s(_T("test"));
  CString testfile(_T("testfile.1"));

  ASSERT_SUCCEEDED(f.Open(testfile, true, async));
  uint32 pos = 0;
  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf),
                             len1,
                             0,
                             NULL));
  pos += len1;

  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf3),
                             len3,
                             0,
                             NULL));
  pos += len3;

  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf3),
                             len3,
                             0,
                             NULL));
  pos += len3;

  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf3),
                             len3,
                             0,
                             NULL));
  pos += len3;

  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf3),
                             len3,
                             0,
                             NULL));
  pos += len3;

  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf3),
                             len3,
                             0,
                             NULL));
  pos += len3;

  ASSERT_SUCCEEDED(f.WriteAt(pos,
                             reinterpret_cast<byte*>(buf3),
                             len3,
                             0,
                             NULL));
  pos += len3;

  ASSERT_SUCCEEDED(f.ReadAt(0, reinterpret_cast<byte*>(buf4), len1, 0, NULL));
  ASSERT_STREQ(buf, buf4);

  ASSERT_SUCCEEDED(f.ReadAt(4, reinterpret_cast<byte*>(buf4), len3, 0, NULL));
  ASSERT_STREQ(buf3, buf4);

  ASSERT_SUCCEEDED(f.WriteAt(1, reinterpret_cast<byte*>(buf3), 2, 0, NULL));

  ASSERT_SUCCEEDED(f.WriteAt(20, reinterpret_cast<byte*>(buf), 2, 0, NULL));
  ASSERT_SUCCEEDED(f.ReadAt(20, reinterpret_cast<byte*>(buf), 2, 0, NULL));

  ASSERT_SUCCEEDED(f.WriteAt(30, reinterpret_cast<byte*>(buf), 2, 0, NULL));

  ASSERT_SUCCEEDED(f.SeekFromBegin(0));

  ASSERT_SUCCEEDED(f.ReadAt(30, reinterpret_cast<byte*>(buf), 2, 0, NULL));

  ASSERT_SUCCEEDED(f.Close());

  ASSERT_SUCCEEDED(f.Open(L"testfile.1", false, false));
  ASSERT_SUCCEEDED(f.ReadAt(0, reinterpret_cast<byte*>(buf), 16, 0, NULL));
  buf[17] = '\0';

  uint64 size_on_disk = 0;
  ASSERT_SUCCEEDED(f.GetSizeOnDisk(&size_on_disk));
  ASSERT_EQ(size_on_disk, 32);

  ASSERT_SUCCEEDED(f.Close());

  ASSERT_TRUE(File::Exists(testfile));
  ASSERT_SUCCEEDED(File::Remove(testfile));
  ASSERT_FALSE(File::Exists(testfile));
}

void FileWriteCreate(uint32 file_size) {
  Timer time(true);
  CString testfile;
  testfile.Format(L"testfile%u", file_size);

  File f;
  ASSERT_SUCCEEDED(f.Open(testfile, true, false));
  ASSERT_SUCCEEDED(f.SetLength(file_size, false));

  uint32 write_size = 512;

  byte *buf2 = new byte[write_size];

  for (uint32 j = 0; j < file_size - write_size; j += write_size) {
      for (uint32 i = 0; i < write_size; i++) {
        buf2[i] = static_cast<byte>(tr_rand() % 255);
      }
      ASSERT_SUCCEEDED(f.WriteAt(j, buf2, write_size, 0, NULL));
  }

  f.Sync();
  f.Close();
}

void FileWriteTimeTest(uint32 file_size,
                       uint32 number_writes,
                       uint32 write_size) {
  Timer time(true);
  CString testfile;
  testfile.Format(L"testfile%u", file_size);

  File f;
  ASSERT_SUCCEEDED(f.Open(testfile, true, false));

  byte *buf = new byte[write_size];

  for (uint32 i = 0; i < number_writes; i++) {
    for (uint32 j = 0; j < write_size; j++) {
      buf[j] = static_cast<byte>(tr_rand() % 255);
    }
    uint32 pos = (tr_rand() * 65536 + tr_rand()) % (file_size - write_size);
    ASSERT_SUCCEEDED(f.WriteAt(pos, buf, write_size, 0, NULL));
  }

  delete[] buf;

  ASSERT_SUCCEEDED(f.Sync());
  ASSERT_SUCCEEDED(f.Close());
}

TEST(FileTest, File) {
  SimpleTest(false);

  int header = 123;
  int header2 = 0;

  File f2;
  ASSERT_SUCCEEDED(f2.Open(L"testfile.3", true, false));
  ASSERT_SUCCEEDED(f2.WriteAt(0,
                              reinterpret_cast<byte*>(&header),
                              sizeof(header),
                              0,
                              NULL));
  ASSERT_SUCCEEDED(f2.ReadAt(0,
                             reinterpret_cast<byte*>(&header2),
                             sizeof(header),
                             0,
                             NULL));
  ASSERT_EQ(header, header2);

  uint64 size_on_disk = 0;

  ASSERT_SUCCEEDED(f2.GetSizeOnDisk(&size_on_disk));
  ASSERT_EQ(size_on_disk, sizeof(header));
  ASSERT_SUCCEEDED(f2.Close());

  const int kLevel = 1;
  for (uint32 file_size = 2 * 1024 * 1024;
       file_size <= 2 * 1024 * 1024;
       file_size *= 2) {
    for (uint32 number_writes = 100;
         number_writes <= (300 * static_cast<uint32>(kLevel));
         number_writes *= 2) {
      uint32 write_size = 128;
      FileWriteTimeTest(file_size, number_writes, write_size);
    }
  }

  // Test File::Copy, File::Move, File::CopyWildcards
  {
    CString windows_dir;
    CString temp_dir;
    DWORD dw = ::GetEnvironmentVariable(
        L"SystemRoot",
        windows_dir.GetBufferSetLength(MAX_PATH),
        MAX_PATH);
    windows_dir.ReleaseBuffer();
    ASSERT_TRUE(dw);
    dw = ::GetEnvironmentVariable(L"TEMP",
                                  temp_dir.GetBufferSetLength(MAX_PATH),
                                  MAX_PATH);
    temp_dir.ReleaseBuffer();
    ASSERT_TRUE(dw);
    CString known_file1(windows_dir + L"\\NOTEPAD.EXE");
    CString known_file2(windows_dir + L"\\REGEDIT.EXE");
    CString temp_file1(temp_dir + L"\\FOO.TMP");
    CString temp_file2(temp_dir + L"\\BAR.TMP");
    uint32 known_size1 = 0;
    uint32 known_size2 = 0;
    uint32 temp_size1 = 0;

    // Start with neither file existing
    if (File::Exists(temp_file1))
      File::Remove(temp_file1);
    if (File::Exists(temp_file2))
      File::Remove(temp_file2);
    ASSERT_FALSE(File::Exists(temp_file1));
    ASSERT_FALSE(File::Exists(temp_file2));
    ASSERT_SUCCEEDED(File::GetFileSizeUnopen(known_file1, &known_size1));
    ASSERT_SUCCEEDED(File::GetFileSizeUnopen(known_file2, &known_size2));
    ASSERT_NE(known_size1, known_size2);

    // Copy to create a file, move it to 2nd file, then remove 2nd file
    ASSERT_SUCCEEDED(File::Copy(known_file1, temp_file1, false));
    ASSERT_TRUE(File::Exists(temp_file1));
    ASSERT_SUCCEEDED(File::Move(temp_file1, temp_file2, false));
    ASSERT_FALSE(File::Exists(temp_file1));
    ASSERT_TRUE(File::Exists(temp_file2));
    ASSERT_SUCCEEDED(File::Remove(temp_file2));
    ASSERT_FALSE(File::Exists(temp_file2));

    // Try copying a file on top of a file - with and without
    // replace_existing_file=true
    ASSERT_SUCCEEDED(File::Copy(known_file1, temp_file1, false));
    ASSERT_SUCCEEDED(File::GetFileSizeUnopen(temp_file1, &temp_size1));
    ASSERT_EQ(temp_size1, known_size1);
    ASSERT_SUCCEEDED(File::Copy(known_file2, temp_file1, false));
    ASSERT_TRUE(File::Exists(temp_file1));
    ASSERT_SUCCEEDED(File::GetFileSizeUnopen(temp_file1, &temp_size1));
    ASSERT_EQ(temp_size1, known_size1);
    ASSERT_SUCCEEDED(File::Copy(known_file2, temp_file1, true));
    ASSERT_TRUE(File::Exists(temp_file1));
    ASSERT_SUCCEEDED(File::GetFileSizeUnopen(temp_file1, &temp_size1));
    ASSERT_EQ(temp_size1, known_size2);
    ASSERT_SUCCEEDED(File::Remove(temp_file1));

    // Try copying a bunch of files
    CString known_file3(windows_dir + L"\\twunk_32.exe");
    CString known_file4(windows_dir + L"\\twunk_16.exe");
    CString temp_file3(temp_dir + L"\\twunk_32.exe");
    CString temp_file4(temp_dir + L"\\twunk_16.exe");
    if (File::Exists(temp_file3))
      File::Remove(temp_file3);
    if (File::Exists(temp_file4))
      File::Remove(temp_file4);
    ASSERT_TRUE(File::Exists(known_file3));
    ASSERT_TRUE(File::Exists(known_file4));
    ASSERT_FALSE(File::Exists(temp_file3));
    ASSERT_FALSE(File::Exists(temp_file4));
    ASSERT_SUCCEEDED(File::CopyWildcards(windows_dir,
                                         temp_dir,
                                         L"twunk*.exe",
                                         true));
    ASSERT_TRUE(File::Exists(temp_file3));
    ASSERT_TRUE(File::Exists(temp_file4));
    ASSERT_SUCCEEDED(File::Remove(temp_file3));
    ASSERT_SUCCEEDED(File::Remove(temp_file4));

    std::vector<CString> matching_files;
    ASSERT_SUCCEEDED(File::GetWildcards(windows_dir,
                                        L"twunk*.exe",
                                        &matching_files));
    ASSERT_EQ(matching_files.size(), 2);
    ASSERT_TRUE(matching_files[0] == known_file3 ||
                matching_files[0] == known_file4);
    ASSERT_TRUE(matching_files[1] == known_file3 ||
                matching_files[1] == known_file4);
    ASSERT_TRUE(matching_files[0] != matching_files[1]);
  }
}


TEST(FileTest, FileChangeWatcher) {
  CString temp_dir;
  ASSERT_TRUE(::GetEnvironmentVariable(L"TEMP",
                                       temp_dir.GetBufferSetLength(MAX_PATH),
                                       MAX_PATH) != 0);
  temp_dir.ReleaseBuffer();
  temp_dir = String_MakeEndWith(temp_dir, _T("\\"), false /* ignore_case */);
  temp_dir = temp_dir + _T("omaha_unittest") + itostr(tr_rand() % 255);
  EXPECT_SUCCEEDED(CreateDir(temp_dir, 0));

  // watch the directory for changes
  FileWatcher watcher(temp_dir, false, FILE_NOTIFY_CHANGE_LAST_WRITE);
  EXPECT_SUCCEEDED(watcher.EnsureEventSetup());
  EXPECT_FALSE(watcher.HasChangeOccurred());

  //
  // verify that the watcher got set-up correctly the first time
  //

  // do something in the dir
  File f;
  int header1 = 94;
  ASSERT_SUCCEEDED(f.Open(temp_dir + _T("\\testfile.1"), true, false));
  ASSERT_SUCCEEDED(f.WriteAt(0, reinterpret_cast<byte*>(&header1),
                             sizeof(header1), 0, NULL));
  ASSERT_SUCCEEDED(f.Sync());
  ASSERT_SUCCEEDED(f.Close());

  // Did we noticed that something happened?
  EXPECT_TRUE(watcher.HasChangeOccurred());
  EXPECT_SUCCEEDED(watcher.EnsureEventSetup());
  EXPECT_FALSE(watcher.HasChangeOccurred());

  //
  // verify that the watcher got set-up correctly the second time
  //

  // do something in the dir
  byte header2 = 2;
  ASSERT_SUCCEEDED(f.Open(temp_dir + _T("\\testfile.2"), true, false));
  ASSERT_SUCCEEDED(f.WriteAt(0, reinterpret_cast<byte*>(&header2),
                             sizeof(header2), 0, NULL));
  ASSERT_SUCCEEDED(f.Sync());
  ASSERT_SUCCEEDED(f.Close());

  // Did we noticed that something happened?
  EXPECT_TRUE(watcher.HasChangeOccurred());
  EXPECT_SUCCEEDED(watcher.EnsureEventSetup());
  EXPECT_FALSE(watcher.HasChangeOccurred());

  EXPECT_SUCCEEDED(DeleteDirectory(temp_dir));
}

TEST(FileTest, Exists_UnQuoted) {
  EXPECT_TRUE(File::Exists(kIEBrowserExe));
  EXPECT_FALSE(File::Exists(_T("C:\\foo\\does not exist.exe")));
  EXPECT_FALSE(File::Exists(_T("Z:\\foo\\does not exist.exe")));
  if (ShouldRunLargeTest()) {
    EXPECT_FALSE(File::Exists(_T("\\\\foo\\does not exist.exe")));
  }
}

// File::Exists() expects unquoted paths.
TEST(FileTest, Exists_Quoted) {
  EXPECT_FALSE(File::Exists(kIEBrowserQuotedExe));
  EXPECT_FALSE(File::Exists(_T("\"C:\\foo\\does not exist.exe\"")));
  EXPECT_FALSE(File::Exists(_T("\"Z:\\foo\\does not exist.exe\"")));
  if (ShouldRunLargeTest()) {
    EXPECT_FALSE(File::Exists(_T("\"\\\\foo\\does not exist.exe\"")));
  }
}

// File::Exists() handles trailing spaces but not leading whitespace, tabs, or
// enclosed paths.
TEST(FileTest, Exists_ExtraWhitespace) {
  EXPECT_TRUE(File::Exists(kIEBrowserExe _T(" ")));
  EXPECT_TRUE(File::Exists(kIEBrowserExe _T("    ")));
  EXPECT_FALSE(File::Exists(_T(" ") kIEBrowserExe));
  EXPECT_FALSE(File::Exists(kIEBrowserExe _T("\t")));

  EXPECT_FALSE(File::Exists(kIEBrowserQuotedExe _T(" ")));
  EXPECT_FALSE(File::Exists(kIEBrowserQuotedExe _T("    ")));
  EXPECT_FALSE(File::Exists(_T(" ") kIEBrowserQuotedExe));
  EXPECT_FALSE(File::Exists(kIEBrowserQuotedExe _T("\t")));

  EXPECT_FALSE(File::Exists(_T("\"") kIEBrowserExe _T(" \"")));
  EXPECT_FALSE(File::Exists(_T("\"") kIEBrowserExe _T("    \"")));
  EXPECT_FALSE(File::Exists(_T("\" ") kIEBrowserExe _T("\"") ));
  EXPECT_FALSE(File::Exists(_T("\"") kIEBrowserExe _T("\t\"")));

  EXPECT_FALSE(File::Exists(_T("\"") kIEBrowserExe _T(" \" ")));
}

}  // namespace omaha
