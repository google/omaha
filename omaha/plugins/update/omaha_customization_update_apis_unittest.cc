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
//
// Tests the constants that vary depending on the customization of Omaha.
// The test checks for the Google Update variations, but can be modified for
// your purposes.

#include <windows.h>
#include <tchar.h>
#include <atlbase.h>
#include <oleauto.h>
#include "omaha/base/utils.h"
#include "plugins/update/activex/update_control_idl.h"
#include "omaha/testing/omaha_customization_test.h"

// Most of the tests are intentionally not using the omaha namespace. Most of
// the values being tested are not in this namespace, and being in the global
// namespace is required by TEST_GU_INT_F to catch conflicts with Google types
// when building non-Google versions.

class OmahaCustomizationUpdateComInterfaceTest
    : public OmahaCustomizationTypeLibComInterfaceTest {
 protected:
  OmahaCustomizationUpdateComInterfaceTest()
      : OmahaCustomizationTypeLibComInterfaceTest(UPDATE_PLUGIN_FILENAME) {
  }
};

TEST_F(OmahaCustomizationUpdateComInterfaceTest, TypeLib) {
  EXPECT_GU_ID_EQ(_T("{b627c883-e979-4873-80b3-ddd0b658b56a}"),
                  LIBID_GoogleUpdateControlLib);

  EXPECT_SUCCEEDED(GetTypeLibDocumentation());
  EXPECT_STREQ(_T("GoogleUpdateControlLib"), item_name_);
  EXPECT_GU_STREQ(_T("Google Update Browser Plugins 3.0 Type Library"),
                  item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationUpdateComInterfaceTest,
              IGoogleUpdateOneClick) {
  // TODO(omaha): Test uuid constants after extracting from IDLs.
  EXPECT_GU_ID_EQ(_T("{6F65D62B-2F32-4483-9028-176C30B2389D}"),
                  __uuidof(IGoogleUpdateOneClick));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IGoogleUpdateOneClick")));
  EXPECT_STREQ(_T("Google Update OneClick Control"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationUpdateComInterfaceTest,
              IGoogleUpdate3WebControl) {
  // TODO(omaha): Test uuid constants after extracting from IDLs.
  EXPECT_GU_ID_EQ(_T("{57E37502-65A5-484a-A035-C1608B2626EA}"),
                  __uuidof(IGoogleUpdate3WebControl));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IGoogleUpdate3WebControl")));
  EXPECT_STREQ(_T("GoogleUpdate3Web Control"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationUpdateComInterfaceTest,
              GoogleUpdateOneClickControlCoClass) {
  EXPECT_GU_ID_EQ(_T("{c442ac41-9200-4770-8cc0-7cdb4f245c55}"),
                  __uuidof(GoogleUpdateOneClickControlCoClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdateOneClickControlCoClass")));
  EXPECT_STREQ(_T("Google Update OneClick Control Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationUpdateComInterfaceTest,
              GoogleUpdate3WebControlCoClass) {
  EXPECT_GU_ID_EQ(_T("{c3101a8b-0ee1-4612-bfe9-41ffc1a3c19d}"),
                  __uuidof(GoogleUpdate3WebControlCoClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdate3WebControlCoClass")));
  EXPECT_STREQ(_T("GoogleUpdate3Web Control Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// Verifies there are no new interfaces in the TypeLib.
TEST_F(OmahaCustomizationUpdateComInterfaceTest, VerifyNoNewInterfaces) {
  EXPECT_EQ(4, type_lib_->GetTypeInfoCount())
      << _T("A new interface may have been added. If so, roll ")
      << _T("the plugin version and add test(s) for new interface(s).");
}
