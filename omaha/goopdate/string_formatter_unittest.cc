// Copyright 2010 Google Inc.
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

#include "omaha/goopdate/string_formatter.h"

#include "omaha/base/app_util.h"
#include "omaha/base/string.h"
#include "omaha/common/lang.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/testing/unit_test.h"

using ::testing::_;

namespace omaha {

class StringFormatterTest : public testing::Test {
 protected:
  StringFormatterTest() {}

  static void SetUpTestCase() {
    CString resource_dir = app_util::GetModuleDirectory(NULL);
    EXPECT_HRESULT_SUCCEEDED(
        ResourceManager::CreateForDefaultLanguage(false, resource_dir));
  }
  static void TearDownTestCase() {
    ResourceManager::Delete();
  }

  virtual void SetUp() {}

  virtual void TearDown() {}
};

TEST_F(StringFormatterTest, LoadStringTest) {
  CString loaded_string;
  StringFormatter formatter_en(_T("en"));
  EXPECT_HRESULT_SUCCEEDED(formatter_en.LoadString(IDS_CLOSE, &loaded_string));
  EXPECT_STREQ(_T("Close"), loaded_string);

  StringFormatter formatter_de(_T("de"));
  EXPECT_HRESULT_SUCCEEDED(
      formatter_de.LoadString(IDS_DEFAULT_APP_DISPLAY_NAME, &loaded_string));
  // The loaded string should keep the raw format ('%1!s!') untouched.
  EXPECT_STREQ(_T("%1!s!-Anwendung"), loaded_string);

  // Test that loading non-existing language resource returns error.
  {
    StringFormatter formatter_unknown(_T("non-existing"));
    ExpectAsserts expect_asserts;
    EXPECT_HRESULT_FAILED(
        formatter_unknown.LoadString(IDS_CLOSE, &loaded_string));
  }
}

TEST_F(StringFormatterTest, FormatMessageTest) {
  CString format_result;

  // Test FormatMessage loads string from correct language resource file.
  StringFormatter formatter_en(_T("en"));
  EXPECT_HRESULT_SUCCEEDED(
      formatter_en.FormatMessage(&format_result, IDS_CLOSE));
  EXPECT_STREQ(_T("Close"), format_result);

  StringFormatter formatter_de(_T("de"));
  EXPECT_HRESULT_SUCCEEDED(
      formatter_de.FormatMessage(&format_result, IDS_CLOSE));
  EXPECT_STREQ(_T("Schlie\u00dfen"), format_result);   // NOLINT

  StringFormatter formatter_fr(_T("fr"));
  EXPECT_HRESULT_SUCCEEDED(
      formatter_fr.FormatMessage(&format_result, IDS_CLOSE));
  EXPECT_STREQ(_T("Fermer"), format_result);

  // Test FormatMessage with additional argument(s).
  EXPECT_HRESULT_SUCCEEDED(formatter_en.FormatMessage(
      &format_result, IDS_DEFAULT_APP_DISPLAY_NAME, _T("English")));
  EXPECT_STREQ(_T("English Application"), format_result);

  EXPECT_HRESULT_SUCCEEDED(formatter_de.FormatMessage(
      &format_result, IDS_DEFAULT_APP_DISPLAY_NAME, _T("German")));
  EXPECT_STREQ(_T("German-Anwendung"), format_result);

  EXPECT_HRESULT_SUCCEEDED(formatter_fr.FormatMessage(
      &format_result, IDS_DEFAULT_APP_DISPLAY_NAME, _T("French")));
  EXPECT_STREQ(_T("Application French"), format_result);   // NOLINT
}

}  // namespace omaha

