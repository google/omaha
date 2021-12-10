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

#include "omaha/enterprise/installer/custom_actions/msi_tag_extractor.h"

#include <atlpath.h>
#include <time.h>

#include <fstream>
#include <vector>

#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class MsiTagExtractorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    unittest_file_path_ = app_util::GetModuleDirectory(NULL);
    EXPECT_TRUE(::PathAppend(CStrBuf(unittest_file_path_, MAX_PATH),
                             _T("unittest_support\\tagged_msi")));
    EXPECT_TRUE(File::Exists(unittest_file_path_));
  }

  CString GetMsiFilePath(const CString& file_name) const {
    CString tagged_msi_path(unittest_file_path_);
    EXPECT_TRUE(::PathAppend(CStrBuf(tagged_msi_path, MAX_PATH), file_name));
    EXPECT_TRUE(File::Exists(tagged_msi_path));
    return tagged_msi_path;
  }

 private:
  CString unittest_file_path_;
};

TEST_F(MsiTagExtractorTest, ValidTag) {
  // GUH-brand-only.msi's tag:BRAND=QAQA
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-brand-only.msi")));
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
  // GUH-ampersand-ending.msi's tag: BRAND=QAQA&
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-ampersand-ending.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("brand", &brand_code));
  EXPECT_EQ(brand_code.compare("QAQA"), 0);
}

TEST_F(MsiTagExtractorTest, MultiValidTags) {
  // File GUH-multiple.msi has tag:
  //   appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&
  //   iid={2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}&
  //   lang=en&browser=4&usagestats=0&appname=Google%20Chrome&
  //   needsadmin=prefers&brand=CHMB&
  //   installdataindex=defaultbrowser
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-multiple.msi")));
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
  // GUH-empty-key.msi's tag: =value&BRAND=QAQA
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-empty-key.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("brand", &brand_code));
  EXPECT_EQ(brand_code.compare("QAQA"), 0);
}

TEST_F(MsiTagExtractorTest, EmptyValue) {
  // GUH-empty-value.msi's tag: BRAND=
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-empty-value.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_TRUE(tag_extractor.GetValue("brand", &brand_code));
  EXPECT_TRUE(brand_code.empty());
}

TEST_F(MsiTagExtractorTest, NoTagString) {
  // GUH-empty-tag.msi's tag:(empty string)
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-empty-tag.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("brand", &brand_code));
}

TEST_F(MsiTagExtractorTest, NoTag) {
  // The original MSI is in parent folder.
  std::wstring tagged_msi(GetMsiFilePath(_T("..\\") MAIN_EXE_BASE_NAME _T("Helper.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_FALSE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));
}

TEST_F(MsiTagExtractorTest, InvalidMagicNumber) {
  // GUH-invalid-marker.msi's has invalid magic number "Gact2.0Foo".
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-invalid-marker.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_FALSE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));
  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("brand", &brand_code));
}


TEST_F(MsiTagExtractorTest, InvalidTagCharacterInKey) {
  // GUH-invalid-key.msi's has invalid charaters in the tag key.
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-invalid-key.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("br*nd", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagCharacterInValue) {
  // GUH-invalid-value.msi's has invalid charaters in the tag value.
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-invalid-value.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("brand", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagFormat) {
  // GUH-bad-format.msi's has invalid tag format.
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-bad-format.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("brand", &brand_code));
}

TEST_F(MsiTagExtractorTest, InvalidTagFormat2) {
  // GUH-bad-format2.msi's has invalid tag format.
  std::wstring tagged_msi(GetMsiFilePath(_T("GUH-bad-format.msi")));
  custom_action::MsiTagExtractor tag_extractor;
  EXPECT_TRUE(tag_extractor.ReadTagFromFile(tagged_msi.c_str()));

  std::string brand_code;
  EXPECT_FALSE(tag_extractor.GetValue("brand", &brand_code));
}

}  // namespace omaha
