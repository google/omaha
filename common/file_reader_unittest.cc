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

// TODO(omaha): improve unit test. For example, test we are handling correctly
// different types of line termination.

#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/file_reader.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class ReadingFilesTest : public testing::Test {
 protected:
  ReadingFilesTest() {
    temp_file_[0] = '\0';
  }

  virtual void SetUp() {
    // create a temporary file
    TCHAR temp_path[MAX_PATH] = {0};
    EXPECT_LT(::GetTempPath(arraysize(temp_path), temp_path),
              arraysize(temp_path));
    EXPECT_NE(::GetTempFileName(temp_path, _T("ut_"), 0, temp_file_), 0);
  }

  virtual void TearDown() {
    // remove the temporary file
    if (::lstrlen(temp_file_) > 0) {
      ASSERT_SUCCEEDED(File::Remove(temp_file_));
    }
  }

  void Compare(const std::vector<char*>& lines1,
               const std::vector<char*>& lines2,
               byte** orig_lines, int* lengths, int* copies) {
    // verify that all three things match
    //  * The lines read using the File class,
    //  * The lines read using the FileReader class,
    //  * The original data used to write the files
    EXPECT_EQ(lines1.size(), lines2.size());

    for (uint32 line_number = 0; line_number < lines1.size(); line_number++) {
      int index = 0;
      int data_length = lengths[line_number];
      int data_copies = copies[line_number];
      for (int copy = 0; copy < data_copies; copy++) {
        for (int index_data = 0; index_data < data_length; index_data++) {
          EXPECT_EQ(lines1[line_number][index], lines2[line_number][index]);
          EXPECT_EQ(lines1[line_number][index],
                    orig_lines[line_number][index_data]);
          index++;
        }
      }
      EXPECT_EQ(lines1[line_number][index], lines2[line_number][index]);
      EXPECT_EQ(lines1[line_number][index], '\0');
    }
  }

  void DeleteContainedArrays(std::vector<char*>* lines) {
    for (uint32 line_number = 0; line_number < lines->size(); line_number++) {
      delete[] (*lines)[line_number];
      (*lines)[line_number] = NULL;
    }
  }

  HRESULT WriteCStringW(File* file, CStringW* str) {
    return file->Write(reinterpret_cast<const byte*>(str->GetBuffer()),
                       str->GetLength() * sizeof(WCHAR),
                       NULL);
  }

  void TestReadFileStringUnicode(int buffer_size) {
    CStringW line1 = L"hello there, here's some data";
    CStringW line2 = L"i've got more\tdata over here.";
    CStringW eol1 = L"\r";
    CStringW eol2 = L"\r\n";
    CStringW eol3 = L"\n";

    std::vector<CString> expected_lines;
    expected_lines.push_back(CString(line1));
    expected_lines.push_back(CString(line2));
    expected_lines.push_back(CString(line1));
    expected_lines.push_back(CString(line1));
    expected_lines.push_back(CString(line2));

    File file_write;
    EXPECT_SUCCEEDED(file_write.Open(temp_file_, true, false));

    // Write the unicode marker to the beginning of the file.
    byte buf[2] = {0xff, 0xfe};
    EXPECT_SUCCEEDED(file_write.Write(buf, sizeof(buf), NULL));

    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &line1));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &eol1));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &line2));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &eol1));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &line1));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &eol3));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &line1));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &eol2));
    EXPECT_SUCCEEDED(WriteCStringW(&file_write, &line2));
    file_write.Close();

    FileReader reader;
    EXPECT_SUCCEEDED(reader.Init(temp_file_, buffer_size));

    std::vector<CString> read_lines;
    CString current_string;
    while (SUCCEEDED(reader.ReadLineString(&current_string))) {
      read_lines.push_back(current_string);
    }

    ASSERT_EQ(expected_lines.size(), read_lines.size());

    if (expected_lines.size() == read_lines.size()) {
      for (size_t i = 0; i < expected_lines.size(); ++i) {
        CString expected_str = expected_lines[i];
        CString read_str = read_lines[i];
        ASSERT_STREQ(expected_str, read_str);
      }
    }
  }

  TCHAR temp_file_[MAX_PATH];

  DISALLOW_EVIL_CONSTRUCTORS(ReadingFilesTest);
};


TEST_F(ReadingFilesTest, ReadFile1) {
  // write the data to the file
  File file_write;
  EXPECT_SUCCEEDED(file_write.Open(temp_file_, true, false));
  byte data1[] = {'a', 'b', 'c', 'z', '!', '&', '\t'};
  int data1_copies = 1;
  EXPECT_SUCCEEDED(file_write.WriteN(data1, arraysize(data1), data1_copies,
                                     NULL));
  byte return_data = '\n';
  EXPECT_SUCCEEDED(file_write.WriteN(&return_data, 1, 1, NULL));

  byte data2[] = {'d', 'a', 'v', 'e', ' ', ' ', '\t', '\\'};
  int data2_copies = 2;
  EXPECT_SUCCEEDED(file_write.WriteN(data2, arraysize(data2), data2_copies,
                                     NULL));
  file_write.Close();

  // read in the file line by line using the File class
  std::vector<char*> lines1;
  File file_read1;
  ASSERT_SUCCEEDED(file_read1.Open(temp_file_, false, false));
  while (true) {
    uint32 bytes_read;
    scoped_array<char> line(new char[256]);
    if (FAILED(file_read1.ReadLineAnsi(256, line.get(), &bytes_read))) {
      break;
    }
    lines1.push_back(line.release());
  }

  file_read1.Close();

  // read in the file line by line using the FileReader class
  std::vector<char*> lines2;
  FileReader file_read2;
  size_t buffer_size = 512;
  ASSERT_SUCCEEDED(file_read2.Init(temp_file_, buffer_size));
  while (true) {
    scoped_array<char> line(new char[256]);
    if (FAILED(file_read2.ReadLineAnsi(256, line.get()))) {
      break;
    }
    lines2.push_back(line.release());
  }

  // Verify that everything matches
  byte* (orig_lines[]) = {data1, data2};
  int copies[] = {data1_copies, data2_copies};
  int lengths[] = {arraysize(data1), arraysize(data2)};
  Compare(lines1, lines2, orig_lines, lengths, copies);

  // Free the allocated memory
  DeleteContainedArrays(&lines1);
  DeleteContainedArrays(&lines1);
}

// Two readers should be able to read from the same file.
TEST_F(ReadingFilesTest, ReadFileShare) {
  File file;
  EXPECT_SUCCEEDED(file.Open(temp_file_, true, false));
  file.Close();

  const size_t kBufferSize = 0x100;

  FileReader reader1;
  EXPECT_SUCCEEDED(reader1.Init(temp_file_, kBufferSize));

  FileReader reader2;
  EXPECT_SUCCEEDED(reader2.Init(temp_file_, kBufferSize));
}

HRESULT WriteCStringA(File* file, CStringA* str) {
  return file->Write(reinterpret_cast<const byte*>(str->GetBuffer()),
                     str->GetLength(),
                     NULL);
}

TEST_F(ReadingFilesTest, ReadFileStringAnsi) {
  CStringA line1 = "hello there, here's some data";
  CStringA line2 = "i've got more\tdata over here.";
  CStringA eol1 = "\r";
  CStringA eol2 = "\r\n";
  CStringA eol3 = "\n";

  std::vector<CString> expected_lines;
  expected_lines.push_back(CString(line1));
  expected_lines.push_back(CString(line2));
  expected_lines.push_back(CString(line1));
  expected_lines.push_back(CString(line1));
  expected_lines.push_back(CString(line2));

  File file_write;
  EXPECT_SUCCEEDED(file_write.Open(temp_file_, true, false));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &line1));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &eol1));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &line2));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &eol1));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &line1));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &eol3));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &line1));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &eol2));
  EXPECT_SUCCEEDED(WriteCStringA(&file_write, &line2));
  file_write.Close();

  FileReader reader;
  EXPECT_SUCCEEDED(reader.Init(temp_file_, 20));

  std::vector<CString> read_lines;
  CString current_string;
  while (SUCCEEDED(reader.ReadLineString(&current_string))) {
    read_lines.push_back(current_string);
  }

  ASSERT_EQ(expected_lines.size(), read_lines.size());

  if (expected_lines.size() == read_lines.size()) {
    for (size_t i = 0; i < expected_lines.size(); ++i) {
      CString expected_str = expected_lines[i];
      CString read_str = read_lines[i];
      ASSERT_STREQ(expected_str, read_str);
    }
  }
}

TEST_F(ReadingFilesTest, ReadFileStringUnicodeSmallBuffer) {
  TestReadFileStringUnicode(20);
}

TEST_F(ReadingFilesTest, ReadFileStringUnicodeHugeBuffer) {
  TestReadFileStringUnicode(4096);
}

TEST_F(ReadingFilesTest, ReadFileStringUnicodeSmallOddBufferBuffer) {
  TestReadFileStringUnicode(19);
}

TEST_F(ReadingFilesTest, ReadFileStringUnicodeTinyOddBuffer) {
  TestReadFileStringUnicode(3);
}

TEST_F(ReadingFilesTest, ReadFileStringUnicodeTinyEvenBuffer) {
  TestReadFileStringUnicode(2);
}

TEST_F(ReadingFilesTest, ReadFileStringUnicodeOneByteBuffer) {
  File file_write;
  EXPECT_SUCCEEDED(file_write.Open(temp_file_, true, false));

  // Write the unicode marker to the beginning of the file.
  byte buf[2] = {0xff, 0xfe};
  EXPECT_SUCCEEDED(file_write.Write(buf, sizeof(buf), NULL));

  file_write.Close();

  FileReader reader;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER),
            reader.Init(temp_file_, 1));
}

}  // namespace omaha

