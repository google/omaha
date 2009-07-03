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

#include <shlobj.h>
#include "base/scoped_ptr.h"
#include "omaha/common/apply_tag.h"
#include "omaha/common/app_util.h"
#include "omaha/common/extractor.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR kFilePath[] = _T(".");
const TCHAR kFileName[] = _T("GoogleUpdate.exe");
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

  // No tag string in the original exe file.
  int tag_buffer_size = 0;
  ASSERT_FALSE(extractor.ExtractTag(NULL, &tag_buffer_size));
  ASSERT_EQ(tag_buffer_size, 0);
  extractor.CloseFile();

  // Create a temp dir.
  TCHAR temp_path[MAX_PATH] = {0};
  *temp_path = 0;
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_path), 0);

  // Embed the tag string.
  CString tagged_file;
  tagged_file.Format(_T("%s%s"), temp_path, kFileName);
  omaha::ApplyTag tag;
  ASSERT_HRESULT_SUCCEEDED(tag.Init(signed_exe_file,
                                    kTagString,
                                    strlen(kTagString),
                                    tagged_file,
                                    false));
// TODO(omaha): Remove the ifdef when signing occurs after instrumentation.
#ifdef COVERAGE_ENABLED
  std::wcout << _T("\tTest does not run in coverage builds.") << std::endl;
#else
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
#endif
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

  // No tag string in the original exe file.
  int tag_buffer_size = 0;
  ASSERT_FALSE(extractor.ExtractTag(NULL, &tag_buffer_size));
// TODO(omaha): Remove the ifdef when signing occurs after instrumentation.
#ifdef COVERAGE_ENABLED
  std::wcout << _T("\tTest does not run in coverage builds.") << std::endl;
#else
  ASSERT_GT(extractor.cert_length(), 0);
  ASSERT_EQ(tag_buffer_size, 0);
  extractor.CloseFile();

  // Create a temp dir.
  TCHAR temp_path[MAX_PATH] = {0};
  *temp_path = 0;
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_path), 0);

  // Embed the tag string.
  CString tagged_file;
  tagged_file.Format(_T("%s%d%s"), temp_path, 1, kFileName);
  omaha::ApplyTag tag;
  ASSERT_HRESULT_SUCCEEDED(tag.Init(signed_exe_file,
                                    kTagString,
                                    strlen(kTagString),
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
                                     strlen(kAppendTagString),
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
                                     strlen(kAppendTagString),
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

  scoped_array<char> tag_buffer(new char[expected_tag_string_len]);
  ASSERT_TRUE(extractor.ExtractTag(tag_buffer.get(), &tag_buffer_size));
  ASSERT_EQ(tag_buffer_size, expected_tag_string_len);
  ASSERT_EQ(memcmp(tag_buffer.get(),
                   expected_tag_string,
                   expected_tag_string_len),
            0);
#endif
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

  // No tag string in the original exe file.
  int tag_buffer_size = 0;
  ASSERT_FALSE(extractor.ExtractTag(NULL, &tag_buffer_size));
// TODO(omaha): Remove the ifdef when signing occurs after instrumentation.
#ifdef COVERAGE_ENABLED
  std::wcout << _T("\tTest does not run in coverage builds.") << std::endl;
#else
  ASSERT_GT(extractor.cert_length(), 0);
  ASSERT_EQ(tag_buffer_size, 0);
  extractor.CloseFile();

  // Create a temp dir.
  TCHAR temp_path[MAX_PATH] = {0};
  *temp_path = 0;
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_path), 0);

  // Embed the tag string.
  CString tagged_file;
  tagged_file.Format(_T("%s%d%s"), temp_path, 1, kFileName);
  omaha::ApplyTag tag1;
  ASSERT_HRESULT_SUCCEEDED(tag1.Init(signed_exe_file,
                                     kTagString,
                                     strlen(kTagString),
                                     tagged_file,
                                     false));
  ASSERT_SUCCEEDED(tag1.EmbedTagString());
  ON_SCOPE_EXIT(::DeleteFile, tagged_file);

  CString tagged_appended_file;
  tagged_appended_file.Format(_T("%s%d%s"), temp_path, 2, kFileName);
  omaha::ApplyTag tag2;
  ASSERT_HRESULT_SUCCEEDED(tag2.Init(tagged_file,
                                     kAppendTagString,
                                     strlen(kAppendTagString),
                                     tagged_appended_file,
                                     false));
  ASSERT_EQ(tag2.EmbedTagString(), APPLYTAG_E_ALREADY_TAGGED);
  ON_SCOPE_EXIT(::DeleteFile, tagged_appended_file);
#endif
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
                                     strlen(input_str),
                                     tagged_file,
                                     false));

  const char* const input_str2 = "abcd$%#";
  omaha::ApplyTag tag2;
  ASSERT_HRESULT_FAILED(tag2.Init(signed_exe_file,
                                  input_str2,
                                  strlen(input_str2),
                                  tagged_file,
                                  false));

  const char* const input_str3 = "abcd asdf";
  omaha::ApplyTag tag3;
  ASSERT_HRESULT_FAILED(tag3.Init(signed_exe_file,
                                  input_str3,
                                  strlen(input_str3),
                                  tagged_file,
                                  false));
}

}  // namespace omaha

