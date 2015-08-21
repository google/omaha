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

#include "base/rand_util.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/string.h"
#include "omaha/base/shell.h"
#include "omaha/base/timer.h"
#include "omaha/base/utils.h"
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

void FileWriteTimeTest(uint32 file_size,
                       uint32 number_writes,
                       uint32 write_size) {
  Timer time(true);
  CString testfile;
  testfile.Format(L"testfile%u", file_size);

  File f;
  ASSERT_SUCCEEDED(f.Open(testfile, true, false));

  uint8* buf = new byte[write_size];

  for (uint32 i = 0; i < number_writes; i++) {
    ASSERT_TRUE(RandBytes(buf, sizeof(*buf * write_size)));
    uint32 random_value = 0;
    ASSERT_TRUE(RandBytes(&random_value, sizeof(random_value)));
    const uint32 pos = random_value % (file_size - write_size);
    ASSERT_SUCCEEDED(f.WriteAt(pos, buf, write_size, 0, NULL));
  }

  delete[] buf;

  ASSERT_SUCCEEDED(f.Sync());
  ASSERT_SUCCEEDED(f.Close());

  EXPECT_SUCCEEDED(File::Remove(testfile));
}

TEST(FileTest, File) {
  const TCHAR* const kTestFileName = L"testfile.3";

  SimpleTest(false);

  int header = 123;
  int header2 = 0;

  File f2;
  ASSERT_SUCCEEDED(f2.Open(kTestFileName, true, false));
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

  // Test File::Copy, File::Move
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
  }

  EXPECT_SUCCEEDED(File::Remove(kTestFileName));
}

// Tests CopyWildcards and GetWildcards. Creates a few temporary files in
// temporary directories, copy the files around using wildcards, compare
// with the list of files retrieved using wildcards, and clean up at the end.
TEST(FileTest, CopyWildcards) {
  const wchar_t kPrefix[] = L"tst";

  const CString source_dir = GetUniqueTempDirectoryName();
  const CString dest_dir = GetUniqueTempDirectoryName();

  EXPECT_SUCCEEDED(CreateDir(source_dir, NULL));
  EXPECT_SUCCEEDED(CreateDir(dest_dir, NULL));

  const int kNumFiles = 3;
  std::vector<CString> source_files;
  std::vector<CString> expected_dest_files;
  for (int i = 0; i != kNumFiles; ++i) {
    const CString filename = GetTempFilenameAt(source_dir, kPrefix);
    EXPECT_TRUE(File::Exists(filename));
    source_files.push_back(filename);
    expected_dest_files.push_back(ConcatenatePath(
        dest_dir,
        GetFileFromPath(filename)));
  }

  const CString pattern_to_match = CString(kPrefix) + L"*";
  ASSERT_SUCCEEDED(File::CopyWildcards(source_dir,
                                       dest_dir,
                                       pattern_to_match,
                                       false));
  for (int i = 0; i != kNumFiles; ++i) {
    EXPECT_TRUE(File::Exists(expected_dest_files[i]));
  }

  std::vector<CString> actual_dest_files;
  ASSERT_SUCCEEDED(File::GetWildcards(dest_dir,
                                      pattern_to_match,
                                      &actual_dest_files));
  ASSERT_EQ(static_cast<uint32>(expected_dest_files.size()),
            static_cast<uint32>(actual_dest_files.size()));

  for (int i = 0; i != kNumFiles; ++i) {
    EXPECT_NE(actual_dest_files.end(),
              std::find(actual_dest_files.begin(),
                        actual_dest_files.end(),
                        expected_dest_files[i]));
  }

  EXPECT_SUCCEEDED(DeleteDirectory(source_dir));
  EXPECT_SUCCEEDED(DeleteDirectory(dest_dir));
}


TEST(FileTest, FileChangeWatcher) {
  CString temp_dir;
  ASSERT_TRUE(::GetEnvironmentVariable(L"TEMP",
                                       temp_dir.GetBufferSetLength(MAX_PATH),
                                       MAX_PATH) != 0);
  temp_dir.ReleaseBuffer();
  temp_dir = String_MakeEndWith(temp_dir, _T("\\"), false /* ignore_case */);
  int random_value= 0;
  EXPECT_TRUE(RandBytes(&random_value, sizeof(random_value)));
  temp_dir = temp_dir + _T("omaha_unittest") + itostr(random_value % 255);
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

TEST(FileTest, AreFilesIdentical) {
  CString windows_dir;
  ASSERT_TRUE(::GetEnvironmentVariable(_T("SystemRoot"),
                                       CStrBuf(windows_dir, MAX_PATH),
                                       MAX_PATH));

  CString known_file1(windows_dir + _T("\\NOTEPAD.EXE"));
  CString known_file2(windows_dir + _T("\\REGEDIT.EXE"));

  EXPECT_TRUE(File::AreFilesIdentical(known_file1, known_file1));
  EXPECT_FALSE(File::AreFilesIdentical(known_file1, known_file2));
}

}  // namespace omaha
