// Copyright 2007-2010 Google Inc.
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
// Path-utility unit tests.

#include <vector>
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(PathTest, IsAbsolutePath) {
  ASSERT_TRUE(IsAbsolutePath(L"C:\\Foo.bar"));
  ASSERT_TRUE(IsAbsolutePath(L"\\\\user-laptop1\\path"));
  ASSERT_FALSE(IsAbsolutePath(L"windows\\system32"));
}

// EnclosePath is overzealous and quotes a path even though no spaces exists.
TEST(PathTest, EnclosePath) {
  CString path;
  EnclosePath(&path);
  EXPECT_STREQ(_T(""), path);

  path = _T("");
  EnclosePath(&path);
  EXPECT_STREQ(_T(""), path);

  path = _T("a");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"a\""), path);

  path = _T("a b");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"a b\""), path);

  path = " a b ";
  EnclosePath(&path);
  EXPECT_STREQ(_T("\" a b \""), path);

  path = _T("\"a b\"");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"a b\""), path);

  path = _T("c:\\Windows\\notepad.exe");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"c:\\Windows\\notepad.exe\""), path);

  path = _T("c:\\Program Files\\Google\\Common\\Google Update");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"c:\\Program Files\\Google\\Common\\Google Update\""),
               path);
}

// EnclosePath encloses a string that has a quote only at the beginning or end.
TEST(PathTest, EnclosePath_OnlyOneQuote) {
  ExpectAsserts expect_asserts;
  CString path = _T("\"a b");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"\"a b\""), path);

  path = _T("a b\"");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"a b\"\""), path);
}

// EnclosePath does not look at the middle of the string.
TEST(PathTest, EnclosePath_QuoteInMiddle) {
  CString path = _T("a\" b");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"a\" b\""), path);
}

TEST(PathTest, EnclosePath_SingleQuotes) {
  CString path = _T("'foo'");
  EnclosePath(&path);
  EXPECT_STREQ(_T("\"'foo'\""), path);
}

TEST(PathTest, EnclosePathIfExe) {
  CString original_path;
  CString new_path;
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("a");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("a b");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = " a b ";
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("\"a b\"");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("c:\\Windows\\notepad.exe");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(_T("\"c:\\Windows\\notepad.exe\""), new_path);

  original_path = _T("c:\\Program Files\\Google\\Update");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("c:\\Progra Files\\Google\\Update\\1.1.1.1\\goopdate.dll");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(original_path, new_path);

  original_path = _T("c:\\Prog F\\Googl\\Update\\GoogleUpdate.exe");
  new_path = EnclosePathIfExe(original_path);
  EXPECT_STREQ(_T("\"c:\\Prog F\\Googl\\Update\\GoogleUpdate.exe\""), new_path);
}

TEST(PathTest, ConcatenatePath) {
  CString expected_path("C:\\first\\part\\second\\part");

  CString start("C:\\first\\part");
  CString start_slash("C:\\first\\part\\");
  CString end("second\\part");
  CString end_slash("\\second\\part");

  EXPECT_STREQ(expected_path, ConcatenatePath(start, end));
  EXPECT_STREQ(expected_path, ConcatenatePath(start_slash, end));
  EXPECT_STREQ(expected_path, ConcatenatePath(start, end_slash));
  EXPECT_STREQ(expected_path, ConcatenatePath(start_slash, end_slash));
}

TEST(PathTest, ConcatenatePath_PathTooLong) {
  CString two_hundred_char_root_path(_T("C:\\reallllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllly\\loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong"));  // NOLINT
  CString two_hundred_char_path(_T("realllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllly\\loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong"));  // NOLINT
  EXPECT_EQ(200, two_hundred_char_root_path.GetLength());
  CString fifty_eight_char_name(
      _T("filenaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaame"));
  EXPECT_EQ(58, fifty_eight_char_name.GetLength());
  CString fifty_nine_char_name(
      _T("filenaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaame"));
  EXPECT_EQ(59, fifty_nine_char_name.GetLength());
  CString sixty_char_name(
      _T("filenaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaame"));
  EXPECT_EQ(60, sixty_char_name.GetLength());

  // Adding the '\' makes it 259 chars.
  CString result = ConcatenatePath(two_hundred_char_root_path,
                                   fifty_eight_char_name);
  EXPECT_STREQ(_T("C:\\reallllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllly\\loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong\\filenaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaame"),  // NOLINT
               result);

  // Adding the '\' makes it 260 chars.
  ExpectAsserts expect_asserts;
  result = ConcatenatePath(two_hundred_char_root_path,
                                   fifty_nine_char_name);
  EXPECT_TRUE(result.IsEmpty());

  // Adding the '\' makes it 261 chars.
  result = ConcatenatePath(two_hundred_char_root_path, sixty_char_name);
  EXPECT_TRUE(result.IsEmpty());

  // Test for buffer overflow on long strings.
}

TEST(PathTest, ConcatenatePath_EmptyString) {
  EXPECT_STREQ(_T("bar.exe"), ConcatenatePath(_T(""), _T("bar.exe")));
  EXPECT_STREQ(_T("foo"), ConcatenatePath(_T("foo"), _T("")));
  // This is not what I would expect, but it is what the API does.
  EXPECT_STREQ(_T("\\"), ConcatenatePath(_T(""), _T("")));
}

// TODO(omaha): The expected and actual values are reversed throughout.

TEST(PathTest, ShortPathToLongPath) {
  CString expected_path("C:\\Program Files");
  CString short_path("C:\\Progra~1");

  CString long_path;
  ASSERT_SUCCEEDED(ShortPathToLongPath(short_path, &long_path));
  ASSERT_STREQ(expected_path, long_path);
}

TEST(PathTest, FindFilesTest) {
  GUID guid = GUID_NULL;
  ASSERT_SUCCEEDED(::CoCreateGuid(&guid));

  TCHAR path[MAX_PATH] = {0};
  ASSERT_NE(::GetTempPath(MAX_PATH, path), 0);

  CString dir = ConcatenatePath(path, GuidToString(guid));
  EXPECT_FALSE(dir.IsEmpty());
  EXPECT_FALSE(File::Exists(dir));

  // Test with non-existent dir.
  std::vector<CString> files;
  EXPECT_FAILED(FindFiles(dir, _T("*.txt"), &files));
  EXPECT_EQ(files.size(), 0);

  // Test with empty dir.
  EXPECT_NE(::CreateDirectory(dir, NULL), 0);
  files.clear();
  EXPECT_EQ(FindFiles(dir, _T("asdf.txt"), &files), 0x80070002);
  EXPECT_EQ(files.size(), 0);

  CString filename1 = _T("one.txt");
  CString filepath1 = ConcatenatePath(dir, filename1);
  CString filename2 = _T("two_en.txt");
  CString filepath2 = ConcatenatePath(dir, filename2);

  // Test with dir containing one file.
  scoped_hfile handle1(::CreateFile(filepath1, GENERIC_READ, FILE_SHARE_READ,
                                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    NULL));
  EXPECT_TRUE(valid(handle1));
  files.clear();
  EXPECT_SUCCEEDED(FindFiles(dir, _T("*.txt"), &files));
  EXPECT_EQ(files.size(), 1);
  EXPECT_STREQ(files[0], filename1);

  files.clear();
  EXPECT_SUCCEEDED(FindFiles(dir, _T("o*.txt"), &files));
  EXPECT_EQ(files.size(), 1);
  EXPECT_STREQ(files[0], filename1);

  files.clear();
  EXPECT_SUCCEEDED(FindFiles(dir, filepath1, &files));
  EXPECT_EQ(files.size(), 1);
  EXPECT_STREQ(files[0], filename1);

  files.clear();
  EXPECT_EQ(FindFiles(dir, _T("t*.txt"), &files), 0x80070002);
  EXPECT_EQ(files.size(), 0);

  // Test with dir containing two files.
  scoped_hfile handle2(::CreateFile(filepath2, GENERIC_READ, FILE_SHARE_READ,
                                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    NULL));
  EXPECT_TRUE(valid(handle2));
  files.clear();
  EXPECT_SUCCEEDED(FindFiles(dir, _T("*.txt"), &files));
  EXPECT_EQ(files.size(), 2);
  EXPECT_STREQ(files[0], filename1);
  EXPECT_STREQ(files[1], filename2);

  files.clear();
  EXPECT_SUCCEEDED(FindFiles(dir, _T("o*.txt"), &files));
  EXPECT_EQ(files.size(), 1);
  EXPECT_STREQ(files[0], filename1);

  files.clear();
  EXPECT_SUCCEEDED(FindFiles(dir, _T("t*.txt"), &files));
  EXPECT_EQ(files.size(), 1);
  EXPECT_STREQ(files[0], filename2);

  files.clear();
  EXPECT_EQ(FindFiles(dir, _T("asdf.txt"), &files), 0x80070002);
  EXPECT_EQ(files.size(), 0);

  reset(handle1);
  EXPECT_SUCCEEDED(File::Remove(filepath1));
  reset(handle2);
  EXPECT_SUCCEEDED(File::Remove(filepath2));
  EXPECT_NE(::RemoveDirectory(dir), 0);
}

namespace detail {

struct Directory {
  Directory() {}
  explicit Directory(const CString& name) : dir_name(name) {}

  CString dir_name;
  std::vector<Directory> sub_dirs;
  std::vector<CString> files;
};

void ConvertDirectoryStructureToFiles(const Directory& directory,
                                      const CString& dir_path,
                                      std::vector<CString>* files) {
  ASSERT_TRUE(files != NULL);
  ASSERT_HRESULT_SUCCEEDED(CreateDir(dir_path, NULL));

  for (size_t i = 0; i < directory.files.size(); ++i) {
    const CString& file = ConcatenatePath(dir_path, directory.files[i]);
    scoped_hfile handle(::CreateFile(file, GENERIC_READ, FILE_SHARE_READ,
                                      NULL, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL));
    EXPECT_TRUE(valid(handle));
    files->push_back(file);
  }

  for (size_t i = 0; i < directory.sub_dirs.size(); ++i) {
    ConvertDirectoryStructureToFiles(
        directory.sub_dirs[i],
        ConcatenatePath(dir_path, directory.sub_dirs[i].dir_name),
        files);
  }
}

void CreateTestDirectoryStructure(Directory* root_dir) {
  EXPECT_TRUE(root_dir != NULL);
  GUID guid = GUID_NULL;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&guid));

  TCHAR path[MAX_PATH] = {0};
  ASSERT_NE(0, ::GetTempPath(MAX_PATH, path));

  CString dir = ConcatenatePath(path, GuidToString(guid));
  EXPECT_FALSE(dir.IsEmpty());
  EXPECT_FALSE(File::Exists(dir));

  root_dir->dir_name = dir;
  root_dir->files.push_back(_T("test1.txt"));
  root_dir->files.push_back(_T("test2.txt"));

  Directory sub_dir1_level1(_T("sub_dir1_level1"));
  sub_dir1_level1.files.push_back(_T("sub_dir1_level1_test1.txt"));
  Directory sub_dir1_level2(_T("sub_dir1_level2"));
  sub_dir1_level2.files.push_back(_T("sub_dir1_level2_test1.txt"));
  sub_dir1_level1.sub_dirs.push_back(sub_dir1_level2);
  root_dir->sub_dirs.push_back(sub_dir1_level1);

  Directory sub_dir2_level1(_T("sub_dir2_level1"));
  sub_dir2_level1.files.push_back(_T("sub_dir2_level1_test1.txt"));
  root_dir->sub_dirs.push_back(sub_dir2_level1);
}
}  // detail.

TEST(PathTest, FindFileRecursiveTest) {
  detail::Directory dir;
  detail::CreateTestDirectoryStructure(&dir);

  std::vector<CString> expected_files;
  detail::ConvertDirectoryStructureToFiles(dir, dir.dir_name, &expected_files);

  // Call the test method.
  std::vector<CString> files;
  ASSERT_HRESULT_SUCCEEDED(FindFileRecursive(dir.dir_name,
                                             _T("*test*.txt"),
                                             &files));

  // Validate the results.
  ASSERT_EQ(expected_files.size(), files.size());
  for (size_t i = 0; i < expected_files.size(); ++i) {
    EXPECT_STREQ(expected_files[i], files[i]);
  }

  // Cleanup.
  ASSERT_HRESULT_SUCCEEDED(DeleteDirectory(dir.dir_name));
}

TEST(PathTest, FindFileRecursiveTest_Empty) {
  GUID guid = GUID_NULL;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&guid));

  TCHAR path[MAX_PATH] = {0};
  ASSERT_NE(0, ::GetTempPath(MAX_PATH, path));

  CString dir = ConcatenatePath(path, GuidToString(guid));
  EXPECT_FALSE(dir.IsEmpty());
  EXPECT_FALSE(File::Exists(dir));

  ASSERT_HRESULT_SUCCEEDED(CreateDir(dir, NULL));

  // Call the test method.
  std::vector<CString> files;
  ASSERT_HRESULT_SUCCEEDED(FindFileRecursive(dir, _T("*test*.txt"),
                                             &files));

  // Validate results.
  ASSERT_EQ(0, files.size());

  // Cleanup.
  ASSERT_HRESULT_SUCCEEDED(DeleteDirectory(dir));
}

TEST(PathTest, FindFileRecursiveTest_DirNotCreated) {
  GUID guid = GUID_NULL;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&guid));

  TCHAR path[MAX_PATH] = {0};
  ASSERT_NE(0, ::GetTempPath(MAX_PATH, path));

  CString dir = ConcatenatePath(path, GuidToString(guid));
  EXPECT_FALSE(dir.IsEmpty());
  EXPECT_FALSE(File::Exists(dir));

  // Call the test method.
  std::vector<CString> files;
  ASSERT_HRESULT_FAILED(FindFileRecursive(dir, _T("*test*.txt"), &files));
}

}  // namespace omaha

