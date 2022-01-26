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
// Unit test for the extractor and the ApplyTag class.
//
// TODO(omaha): eliminate the dependency on the hardcoded "GoogleUpdate.exe"
// program name.

#include "omaha/base/extractor.h"

#include <shlobj.h>
#include <memory>

#include "omaha/base/app_util.h"
#include "omaha/base/apply_tag.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR kFilePath[] = _T(".");
const TCHAR kFileName[] = MAIN_EXE_BASE_NAME _T("Setup_repair.exe");
const char kTagString[] = "1234567890abcdefg";
const char kAppendTagString[] = "..AppendedStr";

TEST(ExtractorTest, EmbedExtract) {
  // Test the extractor.
  TagExtractor extractor;
  ASSERT_FALSE(extractor.IsFileOpen());

  CString signed_exe_file;
  signed_exe_file.Format(_T("%s\\%s\\%s"),
                         app_util::GetCurrentModuleDirectory(),
                         kFilePath, kFileName);
  ASSERT_TRUE(extractor.OpenFile(signed_exe_file));

  // Zero-length tag string in the original exe file.
  int tag_buffer_size = 0;
  ASSERT_FALSE(extractor.ExtractTag(NULL, &tag_buffer_size));
  extractor.CloseFile();

  // Create a temp dir.
  CString temp_path = app_util::GetTempDir();
  ASSERT_FALSE(temp_path.IsEmpty());

  // Embed the tag string.
  CString tagged_file;
  tagged_file.Format(_T("%s%s"), temp_path, kFileName);
  omaha::ApplyTag tag;
  ASSERT_HRESULT_SUCCEEDED(tag.Init(signed_exe_file,
                                    kTagString,
                                    static_cast<int>(strlen(kTagString)),
                                    tagged_file,
                                    false));
  ASSERT_SUCCEEDED(tag.EmbedTagString());
  ON_SCOPE_EXIT(::DeleteFile, tagged_file);

  // Extract the tag string.
  tag_buffer_size = 0;
  ASSERT_TRUE(extractor.OpenFile(tagged_file));
  ASSERT_TRUE(extractor.ExtractTag(NULL, &tag_buffer_size));
  ASSERT_EQ(tag_buffer_size, arraysize(kTagString));

  char tag_buffer[arraysize(kTagString)] = {0};
  ASSERT_TRUE(extractor.ExtractTag(tag_buffer, &tag_buffer_size));
  ASSERT_EQ(tag_buffer_size, arraysize(kTagString));
  ASSERT_EQ(memcmp(tag_buffer, kTagString, arraysize(kTagString)), 0);
  extractor.CloseFile();
}

TEST(ExtractorTest, EmbedAppendExtract) {
  // Test the extractor.
  TagExtractor extractor;
  ASSERT_FALSE(extractor.IsFileOpen());

  CString signed_exe_file;
  signed_exe_file.Format(_T("%s\\%s\\%s"),
                         app_util::GetCurrentModuleDirectory(),
                         kFilePath, kFileName);
  ASSERT_TRUE(extractor.OpenFile(signed_exe_file));

  // Zero-length tag string in the original exe file.
  int tag_buffer_size = 0;
  ASSERT_FALSE(extractor.ExtractTag(NULL, &tag_buffer_size));
  extractor.CloseFile();

  // Create a temp dir.
  CString temp_path = app_util::GetTempDir();
  ASSERT_FALSE(temp_path.IsEmpty());

  // Embed the tag string.
  CString tagged_file;
  tagged_file.Format(_T("%s%d%s"), temp_path, 1, kFileName);
  omaha::ApplyTag tag;
  ASSERT_HRESULT_SUCCEEDED(tag.Init(signed_exe_file,
                                    kTagString,
                                    static_cast<int>(strlen(kTagString)),
                                    tagged_file,
                                    false));
  ASSERT_SUCCEEDED(tag.EmbedTagString());
  ON_SCOPE_EXIT(::DeleteFile, tagged_file);

  // Append another tag string.
  CString tagged_appended_file;
  tagged_appended_file.Format(_T("%s%d%s"), temp_path, 2, kFileName);
  omaha::ApplyTag tag1;

  ASSERT_HRESULT_SUCCEEDED(tag1.Init(tagged_file,
                                     kAppendTagString,
                                     static_cast<int>(strlen(kAppendTagString)),
                                     tagged_appended_file,
                                     true));
  ASSERT_SUCCEEDED(tag1.EmbedTagString());
  ON_SCOPE_EXIT(::DeleteFile, tagged_appended_file);

  // Append another tag string.
  CString tagged_appended_file2;
  tagged_appended_file2.Format(_T("%s%d%s"), temp_path, 3, kFileName);
  omaha::ApplyTag tag2;
  ASSERT_HRESULT_SUCCEEDED(tag2.Init(tagged_appended_file,
                                     kAppendTagString,
                                     static_cast<int>(strlen(kAppendTagString)),
                                     tagged_appended_file2,
                                     true));
  ASSERT_SUCCEEDED(tag2.EmbedTagString());
  ON_SCOPE_EXIT(::DeleteFile, tagged_appended_file2);

  // Extract the tag string.
  tag_buffer_size = 0;
  CStringA expected_tag_string(kTagString);
  expected_tag_string += kAppendTagString;
  expected_tag_string += kAppendTagString;
  int expected_tag_string_len = expected_tag_string.GetLength() + 1;
  ASSERT_TRUE(extractor.OpenFile(tagged_appended_file2));
  ASSERT_TRUE(extractor.ExtractTag(NULL, &tag_buffer_size));
  ASSERT_EQ(tag_buffer_size, expected_tag_string_len);

  std::unique_ptr<char[]> tag_buffer(new char[expected_tag_string_len]);
  ASSERT_TRUE(extractor.ExtractTag(tag_buffer.get(), &tag_buffer_size));
  ASSERT_EQ(tag_buffer_size, expected_tag_string_len);
  ASSERT_EQ(memcmp(tag_buffer.get(),
                   expected_tag_string,
                   expected_tag_string_len),
            0);
  extractor.CloseFile();
}

TEST(ExtractorTest, AlreadyTaggedError) {
  // Test the extractor.
  TagExtractor extractor;
  ASSERT_FALSE(extractor.IsFileOpen());

  CString signed_exe_file;
  signed_exe_file.Format(_T("%s\\%s\\%s"),
                         app_util::GetCurrentModuleDirectory(),
                         kFilePath, kFileName);
  ASSERT_TRUE(extractor.OpenFile(signed_exe_file));

  // Zero-length tag string in the original exe file.
  int tag_buffer_size = 0;
  ASSERT_FALSE(extractor.ExtractTag(NULL, &tag_buffer_size));
  extractor.CloseFile();

  // Create a temp dir.
  CString temp_path = app_util::GetTempDir();
  ASSERT_FALSE(temp_path.IsEmpty());

  // Embed the tag string.
  CString tagged_file;
  tagged_file.Format(_T("%s%d%s"), temp_path, 1, kFileName);
  omaha::ApplyTag tag1;
  ASSERT_HRESULT_SUCCEEDED(tag1.Init(signed_exe_file,
                                     kTagString,
                                     static_cast<int>(strlen(kTagString)),
                                     tagged_file,
                                     false));
  ASSERT_SUCCEEDED(tag1.EmbedTagString());
  ON_SCOPE_EXIT(::DeleteFile, tagged_file);

  CString tagged_appended_file;
  tagged_appended_file.Format(_T("%s%d%s"), temp_path, 2, kFileName);
  omaha::ApplyTag tag2;
  ASSERT_HRESULT_SUCCEEDED(tag2.Init(tagged_file,
                                     kAppendTagString,
                                     static_cast<int>(strlen(kAppendTagString)),
                                     tagged_appended_file,
                                     false));
  ASSERT_EQ(tag2.EmbedTagString(), APPLYTAG_E_ALREADY_TAGGED);
  ON_SCOPE_EXIT(::DeleteFile, tagged_appended_file);
  extractor.CloseFile();
}

TEST(ApplyTagTest, InvalidCharsTest) {
  // Accepted Regex = [-%{}/\a&=._]*
  CString signed_exe_file;
  signed_exe_file.Format(_T("%s\\%s\\%s"),
                         app_util::GetCurrentModuleDirectory(),
                         kFilePath, kFileName);
  CString tagged_file(_T("out.txt"));

  const char* const input_str = "abcd";
  omaha::ApplyTag tag1;
  ASSERT_HRESULT_SUCCEEDED(tag1.Init(signed_exe_file,
                                     input_str,
                                     static_cast<int>(strlen(input_str)),
                                     tagged_file,
                                     false));

  const char* const input_str2 = "abcd$%#";
  omaha::ApplyTag tag2;
  ASSERT_HRESULT_FAILED(tag2.Init(signed_exe_file,
                                  input_str2,
                                  static_cast<int>(strlen(input_str2)),
                                  tagged_file,
                                  false));

  const char* const input_str3 = "abcd asdf";
  omaha::ApplyTag tag3;
  ASSERT_HRESULT_FAILED(tag3.Init(signed_exe_file,
                                  input_str3,
                                  static_cast<int>(strlen(input_str3)),
                                  tagged_file,
                                  false));
}

}  // namespace omaha

