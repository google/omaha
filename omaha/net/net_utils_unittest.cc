// Copyright 2008-2009 Google Inc.
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


#include "omaha/net/net_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// It only tests for the happy scenario.
TEST(NetUtilsTest, IsMachineConnectedToNetwork) {
  EXPECT_TRUE(IsMachineConnectedToNetwork());
}

TEST(NetUtilsTest, BufferToPrintableString) {
  EXPECT_STREQ(_T(""), BufferToPrintableString(NULL, 0));

  const char* buffer = "a\n\r\x7f\xE2\x80\x99\xff";
  EXPECT_STREQ(_T("a\n\r....."),
               BufferToPrintableString(buffer, strlen(buffer)));
}

TEST(NetUtilsTest, VectorToPrintableString) {
  EXPECT_STREQ(_T(""), VectorToPrintableString(std::vector<uint8>()));

  const char* buffer = "a\n\r\x7f\xE2\x80\x99\xff";
  std::vector<uint8> vec(buffer, buffer + strlen(buffer));
  EXPECT_STREQ(_T("a\n\r....."), VectorToPrintableString(vec));
}

TEST(NetUtilsTest, IsHttpUrl) {
  EXPECT_TRUE(IsHttpUrl(_T("http://www.google.com")));
  EXPECT_TRUE(IsHttpUrl(_T("HTTP://www.google.com")));
  EXPECT_TRUE(IsHttpUrl(_T("http://")));

  EXPECT_FALSE(IsHttpUrl(NULL));
  EXPECT_FALSE(IsHttpUrl(_T("")));
  EXPECT_FALSE(IsHttpUrl(_T("https://www.google.com")));
  EXPECT_FALSE(IsHttpUrl(_T("http:")));
  EXPECT_FALSE(IsHttpUrl(_T("//www.google.com")));
  EXPECT_FALSE(IsHttpUrl(_T("www.google.com")));
  EXPECT_FALSE(IsHttpUrl(_T("http.google.com")));
}

TEST(NetUtilsTest, IsHttpsUrl) {
  EXPECT_TRUE(IsHttpsUrl(_T("https://www.google.com")));
  EXPECT_TRUE(IsHttpsUrl(_T("HTTPS://www.google.com")));
  EXPECT_TRUE(IsHttpsUrl(_T("https://")));

  EXPECT_FALSE(IsHttpsUrl(NULL));
  EXPECT_FALSE(IsHttpsUrl(_T("")));
  EXPECT_FALSE(IsHttpsUrl(_T("http://www.google.com")));
  EXPECT_FALSE(IsHttpsUrl(_T("https:")));
  EXPECT_FALSE(IsHttpsUrl(_T("//www.google.com")));
  EXPECT_FALSE(IsHttpsUrl(_T("www.google.com")));
  EXPECT_FALSE(IsHttpsUrl(_T("https.google.com")));
}

TEST(NetUtilsTest, MakeHttpUrl) {
  EXPECT_STREQ(_T("http://www.google.com"),
               MakeHttpUrl(_T("http://www.google.com")));
  EXPECT_STREQ(_T("http://www.google.com"),
               MakeHttpUrl(_T("https://www.google.com")));
  EXPECT_STREQ(_T("http://www.google.com"),
               MakeHttpUrl(_T("HTTPS://www.google.com")));
  EXPECT_STREQ(_T("http://www.google.com"),
               MakeHttpUrl(_T("www.google.com")));
  EXPECT_STREQ(_T("http://www.google.com"),
               MakeHttpUrl(_T("mailto:www.google.com")));
}

TEST(NetUtilsTest, MakeHttpsUrl) {
  EXPECT_STREQ(_T("https://www.google.com"),
               MakeHttpsUrl(_T("https://www.google.com")));
  EXPECT_STREQ(_T("https://www.google.com"),
               MakeHttpsUrl(_T("http://www.google.com")));
  EXPECT_STREQ(_T("https://www.google.com"),
               MakeHttpsUrl(_T("HTTP://www.google.com")));
  EXPECT_STREQ(_T("https://www.google.com"),
               MakeHttpsUrl(_T("www.google.com")));
  EXPECT_STREQ(_T("https://www.google.com"),
               MakeHttpsUrl(_T("mailto:www.google.com")));
}

}  // namespace omaha

