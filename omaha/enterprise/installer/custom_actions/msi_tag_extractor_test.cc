// Copyright 2013 Google Inc.
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

#include <atlpath.h>
#include <time.h>
#include <fstream>
#include <vector>
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

#include "omaha/enterprise/installer/custom_actions/msi_tag_extractor.h"

namespace omaha {

namespace {

const TCHAR kSourceMsi[] = _T("GoogleUpdateHelper.msi");
const TCHAR kTaggedMsi[] = _T("tagged_GoogleUpdateHelper.msi");

}  // namesapce

class MsiTagExtractorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    unittest_file_path_ = app_util::GetModuleDirectory(NULL);
    EXPECT_TRUE(::PathAppend(CStrBuf(unittest_file_path_, MAX_PATH),
                             _T("..\\staging\\unittest_support")));
    EXPECT_TRUE(File::Exists(unittest_file_path_));
  }

  virtual void TearDown() {
    File::Remove(GetTaggedMsiPath());
  }

  CString GetSourceMsiPath() const {
    CString source_msi_path(unittest_file_path_);
    EXPECT_TRUE(::PathAppend(CStrBuf(source_msi_path, MAX_PATH), kSourceMsi));
    EXPECT_TRUE(File::Exists(source_msi_path));
    return source_msi_path;
  }

  CString GetTaggedMsiPath() const {
    CString tagged_msi_path(unittest_file_path_);
    EXPECT_TRUE(::PathAppend(CStrBuf(tagged_msi_path, MAX_PATH), kTaggedMsi));
    return tagged_msi_path;
  }

  HRESULT CreateMsiFileWithTag(const uint8* tag_buffer,
                               size_t size) const {
    const CString& source_msi(GetSourceMsiPath());
    const CString& output_msi(GetTaggedMsiPath());
    std::ifstream in;
    std::ofstream out;

    in.open(source_msi, std::ifstream::in | std::ifstream::binary);
    out.open(output_msi, std::ofstream::out | std::ofstream::binary);
    if (!in.is_open() || !out.is_open()) {
      return E_FAIL;
    }

    // Copy MSI package first.
    out << in.rdbuf();

    if (tag_buffer) {
      out.write(reinterpret_cast<const char*>(tag_buffer),
                static_cast<std::streamsize>(size));
    }
    return S_OK;
  }

 private:
  CString unittest_file_path_;
};

TEST_F(MsiTagExtractorTest, ValidTag) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 10,  // Tag string length.
    'b', 'r', 'a', 'n', 'd', '=', 'Q', 'A', 'Q', 'A',  // BRAND=QAQA.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("brand", &brand_code));
  EXPECT_EQ(brand_code.compare("QAQA"), 0);

  // Tag is case sensitive.
  EXPECT_FALSE(tag_extractor.GetValue("Brand", &brand_code));

  EXPECT_FALSE(tag_extractor.GetValue("NoneExistKey", &brand_code));
}

TEST_F(MsiTagExtractorTest, ValidTag_AmpersandAtEnd) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 11,  // Tag string length.
    'b', 'r', 'a', 'n', 'd', '=', 'Q', 'A', 'Q', 'A', '&', 0,  // brand=QAQA.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("brand", &brand_code));
  EXPECT_EQ(brand_code.compare("QAQA"), 0);
}

TEST_F(MsiTagExtractorTest, MultiValidTags) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 206,  // Tag string length.

    // appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&
    'a', 'p', 'p', 'g', 'u', 'i', 'd', '=',
    '{', '8', 'A', '6', '9', 'D', '3', '4', '5', '-',
    'D', '5', '6', '4', '-', '4', '6', '3', 'C', '-',
    'A', 'F', 'F', '1', '-',
    'A', '6', '9', 'D', '9', 'E', '5', '3', '0', 'F', '9', '6', '}', '&',

    // iid={2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}&
    'i', 'i', 'd', '=',
    '{', '2', 'D', '8', 'C', '1', '8', 'E', '9', '-',
    '8', 'D', '3', 'A', '-', '4', 'E', 'F', 'C', '-',
    '6', 'D', '6', '1', '-',
    'A', 'E', '2', '3', 'E', '3', '5', '3', '0', 'E', 'A', '2', '}', '&',

    // lang=en&
    'l', 'a', 'n', 'g', '=', 'e', 'n', '&',

    // browser=4&
    'b', 'r', 'o', 'w', 's', 'e', 'r', '=', '4', '&',

    // usagestats=0&
    'u', 's', 'a', 'g', 'e', 's', 't', 'a', 't', 's', '=', '0', '&',

    // appname=Google%20Chrome&
    'a', 'p', 'p', 'n', 'a', 'm', 'e', '=', 'G', 'o', 'o', 'g', 'l', 'e',
    '%', '2', '0', 'C', 'h', 'r', 'o', 'm', 'e', '&',

    // needsadmin=prefers&
    'n', 'e', 'e', 'd', 's', 'a', 'd', 'm', 'i', 'n', '=',
    'p', 'r', 'e', 'f', 'e', 'r', 's', '&',

    // brand=CHMB&
    'b', 'r', 'a', 'n', 'd', '=', 'C', 'H', 'M', 'B', '&',

    // installdataindex=defaultbrowser
    'i', 'n', 's', 't', 'a', 'l', 'l', 'd', 'a', 't', 'a', 'i', 'n', 'd', 'e',
    'x', '=', 'd', 'e', 'f', 'a', 'u', 'l', 't', 'b', 'r', 'o', 'w', 's', 'e',
    'r',

    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string value;
  EXPECT_TRUE(tag_extractor.GetValue("appguid", &value));
  EXPECT_EQ(value.compare("{8A69D345-D564-463C-AFF1-A69D9E530F96}"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("iid", &value));
  EXPECT_EQ(value.compare("{2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("lang", &value));
  EXPECT_EQ(value.compare("en"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("browser", &value));
  EXPECT_EQ(value.compare("4"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("usagestats", &value));
  EXPECT_EQ(value.compare("0"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("appname", &value));
  EXPECT_EQ(value.compare("Google%20Chrome"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("needsadmin", &value));
  EXPECT_EQ(value.compare("prefers"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("brand", &value));
  EXPECT_EQ(value.compare("CHMB"), 0);

  EXPECT_TRUE(tag_extractor.GetValue("installdataindex", &value));
  EXPECT_EQ(value.compare("defaultbrowser"), 0);
}

TEST_F(MsiTagExtractorTest, EmptyKey) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 18,  // Tag string length.
    '=', 'V', 'a', 'l', 'u', 'e', '&',  // =Value
    'B', 'R', 'A', 'N', 'D', '=', 'Q', 'A', 'Q', 'A',  0,  // BRAND=QAQA.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("BRAND", &brand_code));
  EXPECT_EQ(brand_code.compare("QAQA"), 0);
}

TEST_F(MsiTagExtractorTest, EmptyValue) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 7,  // Tag string length.
    'B', 'R', 'A', 'N', 'D', '=', 0,  // BRAND=.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("BRAND", &brand_code));
  EXPECT_TRUE(brand_code.empty());
}

TEST_F(MsiTagExtractorTest, NoTagString) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 0,  // Tag string length.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BRAND", &brand_code));
}

TEST_F(MsiTagExtractorTest, NoTag) {
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(NULL, 0));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_FALSE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));
}

TEST_F(MsiTagExtractorTest, InvalidMagicNumber) {
  const uint8 tag[] = {
    '_', 'a', 'c', 't',  // Invalid magic number.
    0, 10,  // Tag string length.
    'B', 'R', 'A', 'N', 'D', '=', 'Q', 'A', 'Q', 'A',  // BRAND=QAQA.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_FALSE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BRAND", &brand_code));
}


TEST_F(MsiTagExtractorTest, InvalidTagLength) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 88,  // Invalid tag length.
    'B', 'R', 'A', 'N', 'D', '=', 'Q', 'A', 'Q', 'A',  0,  // BRAND=QAQA.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_FALSE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BRAND", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagCharacterInKey) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 10,  // Tag string length.
    'B', 'R', '*', 'N', 'D', '=', 'Q', 'A', 'Q', 'A',  // BR*ND=QAQA.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BR*ND", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagCharacterInValue) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 10,  // Tag string length.
    'B', 'R', 'A', 'N', 'D', '=', 'Q', 'A', '*', 'A',  // BRAND=QA*A.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BRAND", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagFormat) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 5,  // Tag string length.
    'B', 'R', 'A', 'N', 'D',  // No '='.
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BRAND", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagFormat2) {
  const uint8 tag[] = {
    'G', 'a', 'c', 't',  // magic number.
    0, 24,  // Tag string length.
    '=', '=', '=', '=', '=', '=', '=', '&',
    '=', '=', '=', '=', '=', '=', '=', '&',
    '&', '&', '=', '&', '&', '&', '&', '0',
    0, 0, 0, 0,  // 4 bytes of 0s.
  };
  EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));

  std::wstring tagged_msi(GetTaggedMsiPath());
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("BRAND", &brand_code));
}

// Verifies that tag extractor won't crash with garbage tag.
TEST_F(MsiTagExtractorTest, RandomTag) {
  const int kNumPassesToRun = 10;
  srand(static_cast<unsigned int>(time(NULL)));

  uint8 tag[1024];
  for (int i = 0; i < kNumPassesToRun; ++i) {
    // Fill tag with random data.
    for (int j = 0; j < arraysize(tag); ++j) {
      tag[j] = static_cast<char>(rand() % 0x100);  // NOLINT
    }

    EXPECT_SUCCEEDED(CreateMsiFileWithTag(tag, arraysize(tag)));
    std::wstring tagged_msi(GetTaggedMsiPath());
    custom_action::MsiTagExtractor tag_extractor;
    tag_extractor.ReadTagFromFile(tagged_msi.c_str());
  }
}

}  // namespace omaha
