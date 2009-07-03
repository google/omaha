// Copyright 2007-2009 Google Inc.
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

#include "base/basictypes.h"
#include "omaha/common/logging.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(LoggingTest, Logging) {
#ifdef _DEBUG
  OPT_LOG(L1, (_T("[OPT_LOG from debug build.]")));
#else
  OPT_LOG(L1, (_T("[OPT_LOG from optimized build.]")));
#endif
}

class FileLogWriterTest : public testing::Test {
 public:

  int FindFirstInMultiString(const TCHAR* multi_str,
                             size_t count,
                             const TCHAR* str) {
    return FileLogWriter::FindFirstInMultiString(multi_str, count, str);
  }
};

class HistoryTest : public testing::Test {
 protected:
  HistoryTest() {
    logging_ = GetLogging();
  }

  void AppendToHistory(const wchar_t* msg) {
    logging_->AppendToHistory(msg);
  }

  CString GetHistory() {
    return logging_->GetHistory();
  }

 private:
  Logging* logging_;
};

#define EOS _T("")
TEST_F(FileLogWriterTest, FindInMultiString) {
  // One string of one char.
  const TCHAR s1[] = _T("a\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s1, arraysize(s1), _T("a")), 0);

  // One string.
  const TCHAR s2[] = _T("abc\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s2, arraysize(s2), _T("abc")), 0);

  // Two strings of one char.
  const TCHAR s3[] = _T("a\0b\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s3, arraysize(s3), _T("b")), 2);

  // Two strings.
  const TCHAR s4[] = _T("ab\0cde\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s4, arraysize(s4), _T("cde")), 3);

  // Three strings one char.
  const TCHAR s5[] = _T("a\0b\0c\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s5, arraysize(s5), _T("c")), 4);

  // Many strings.
  const TCHAR s6[] = _T("a\0bcd\0efgh\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s6, arraysize(s6), _T("efgh")), 6);

  // Many strings including empty string.
  const TCHAR s7[] = _T("a\0\0bc\0\0de\0fg") EOS;
  EXPECT_EQ(FindFirstInMultiString(s7, arraysize(s7), _T("fg")), 10);

  // Many strings, empty string at the end, negative test.
  const TCHAR s8[] = _T("a\0bcd\0efgh\0\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s8, arraysize(s8), _T("foo")), -1);

  // Another negative test.
  const TCHAR s9[] = _T("a\0bcd\0\0\0efgh\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s9, arraysize(s9), _T("foo")), -1);

  // Empty string is always found.
  const TCHAR s10[] = _T("\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s10, arraysize(s10), _T("\0")), 0);

  const TCHAR s11[] = _T("\0") EOS;
  EXPECT_EQ(FindFirstInMultiString(s11, arraysize(s11), _T("a")), -1);
}

TEST_F(HistoryTest, GetHistory) {
  EXPECT_TRUE(GetHistory().IsEmpty());

  const TCHAR msg1[] = _T("Hello");
  AppendToHistory(msg1);
  EXPECT_STREQ(msg1, GetHistory());
  EXPECT_TRUE(GetHistory().IsEmpty());
}

TEST_F(HistoryTest, AppendToHistoryTest) {
  // Test one character.
  const TCHAR msg1[] = _T("A");
  AppendToHistory(msg1);
  EXPECT_STREQ(msg1, GetHistory());

  // Test small string.
  const TCHAR msg2[] = _T("ABCD");
  AppendToHistory(msg2);
  EXPECT_STREQ(msg2, GetHistory());

  // Test one string that fills the buffer.
  TCHAR msg3[kMaxHistoryBufferSize + 1] = {0};
  for (int i = 0; i <= kMaxHistoryBufferSize; ++i) {
    msg3[i] = _T('A');
  }
  msg3[kMaxHistoryBufferSize] = _T('\0');
  AppendToHistory(msg3);
  EXPECT_STREQ(msg3, GetHistory());

  // Test set of strings that exactly fill buffer.
  const int test_buffer_size = 64;
  TCHAR msg4[test_buffer_size + 1] = {0};
  for (int i = 0; i <= test_buffer_size; ++i) {
    msg4[i] = _T('A');
  }
  msg4[test_buffer_size] = _T('\0');

  int num_times_to_append = kMaxHistoryBufferSize / test_buffer_size;
  EXPECT_EQ(kMaxHistoryBufferSize, num_times_to_append * test_buffer_size);
  for (int i = 0; i < num_times_to_append; ++i) {
    AppendToHistory(msg4);
  }
  EXPECT_STREQ(msg3, GetHistory());
}

TEST_F(HistoryTest, AppendToHistoryTest_WrapAround) {
  // Test string that wraps around the buffer.
  // First fill kMaxHistoryBufferSize - 1 with one string, then use
  // another string of length 2("XX"), and another string to length of length 3.
  // "XFFFGGGGG....GGGX" should be the result. The returned string should
  // be in correct FIFO order i.e. "GGGGG......XXFFF".
  const TCHAR msg6[] = _T("XX");
  const TCHAR msg7[] = _T("FFF");
  const int test_buffer_size = kMaxHistoryBufferSize - 1;
  TCHAR msg5[test_buffer_size + 1] = {0};
  TCHAR expected_buffer[kMaxHistoryBufferSize + 1] = {0};
  for (int i = 0; i <= test_buffer_size; ++i) {
    msg5[i] = _T('G');
  }
  msg5[test_buffer_size] = _T('\0');

  // Call test method.
  AppendToHistory(msg5);
  AppendToHistory(msg6);
  AppendToHistory(msg7);

  // Create the expected string.
  for (int i = 0; i <= kMaxHistoryBufferSize; ++i) {
    expected_buffer[i] = _T('G');
  }
  int msg6len = wcslen(msg6);
  int msg7len = wcslen(msg7);
  memcpy(expected_buffer + kMaxHistoryBufferSize - msg6len - msg7len,
         msg6,
         msg6len * sizeof(TCHAR));
  memcpy(expected_buffer + kMaxHistoryBufferSize - msg7len,
         msg7,
         msg7len * sizeof(TCHAR));
  expected_buffer[kMaxHistoryBufferSize] = _T('\0');
  EXPECT_STREQ(expected_buffer, GetHistory());
}

TEST_F(HistoryTest, AppendToHistoryTest_AnotherWrapAroundTest) {
  // Test string that wraps around the buffer.
  // First fill the kMaxHistoryBufferSize - 1 with one string, then fill it with
  // another string of same length.
  const int test_buffer_size = kMaxHistoryBufferSize - 1;
  TCHAR msg2[test_buffer_size + 1] = {0};
  TCHAR msg1[test_buffer_size + 1] = {0};
  TCHAR expected_buffer[kMaxHistoryBufferSize + 1] = {0};
  for (int i = 0; i <= test_buffer_size; ++i) {
    msg1[i] = _T('G');
    msg2[i] = _T('J');
  }
  msg2[test_buffer_size] = _T('\0');
  msg1[test_buffer_size] = _T('\0');

  // Call test method.
  AppendToHistory(msg1);
  AppendToHistory(msg2);

  // Create the expected string.
  for (int i = 0; i <= kMaxHistoryBufferSize; ++i) {
    expected_buffer[i] = _T('J');
  }
  expected_buffer[0] = _T('G');
  expected_buffer[kMaxHistoryBufferSize] = _T('\0');
  EXPECT_STREQ(expected_buffer, GetHistory());
}

TEST_F(HistoryTest, AppendToHistoryTest_LotsOfLogs) {
  // Run over a number of Append calls, with strings length
  // (kMaxHistoryBufferSize / 2) + 1, causing wrap on every run.
  TCHAR expected_buffer[kMaxHistoryBufferSize + 1] = {0};
  const int test_buffer_size = (kMaxHistoryBufferSize / 2) + 1;
  TCHAR msg1[test_buffer_size + 1] = {0};
  for (int test_char = 'A'; test_char <= 'Z'; ++test_char) {
    for (int i = 0; i <= test_buffer_size; ++i) {
      msg1[i] = static_cast<TCHAR>(test_char);
    }
    msg1[test_buffer_size] = _T('\0');

    // Call test method.
    AppendToHistory(msg1);
  }

  // Create the expected string.
  int i = 0;
  for (; i < test_buffer_size - 2; ++i) {
    expected_buffer[i] = _T('Y');
  }
  for (; i <= kMaxHistoryBufferSize; ++i) {
    expected_buffer[i] = _T('Z');
  }
  expected_buffer[kMaxHistoryBufferSize] = _T('\0');
  EXPECT_STREQ(expected_buffer, GetHistory());
}

TEST_F(HistoryTest, AppendToHistoryTest_LargeBuffer) {
  // Test with a message that is larger than the buffer.
  const int test_buffer_size = kMaxHistoryBufferSize + 10;
  TCHAR msg4[test_buffer_size + 1] = {0};
  TCHAR expected_buffer[kMaxHistoryBufferSize + 1] = {0};
  for (int i = 0; i < test_buffer_size; ++i) {
    msg4[i] = _T('A');
  }
  msg4[test_buffer_size] = _T('\0');

  for (int i = 0; i <= kMaxHistoryBufferSize; ++i) {
    expected_buffer[i] = _T('A');
  }
  expected_buffer[kMaxHistoryBufferSize] = _T('\0');

  AppendToHistory(msg4);
  EXPECT_STREQ(expected_buffer, GetHistory());
}

TEST_F(HistoryTest, AppendToHistoryTest_EmptyBuffer) {
  CString test_string;
  AppendToHistory(test_string);
  EXPECT_TRUE(GetHistory().IsEmpty());
}

}  // namespace omaha

