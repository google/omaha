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
// Common include file for Omaha customization tests.

#ifndef OMAHA_TESTING_OMAHA_CUSTOMIZATION_TEST_H_
#define OMAHA_TESTING_OMAHA_CUSTOMIZATION_TEST_H_

#include "omaha/testing/unit_test.h"

#ifdef GOOGLE_UPDATE_BUILD
// For Google Update builds, expect the values to be equal and for the
// interface names to exist.

// Test fixture for a Google Update-specific interface name.
// Tests using this fixture must be in the global namespace.
#define TEST_GU_INT_F(test_fixture, test_name) TEST_F(test_fixture, test_name)

// Expect the values to be equal only in the case of Google Update builds.
#define EXPECT_GU_EQ(expected, actual) EXPECT_EQ(expected, actual)
#define EXPECT_GU_STREQ(expected, actual) EXPECT_STREQ(expected, actual)
#define EXPECT_GU_TRUE(condition) EXPECT_TRUE(condition)
#define EXPECT_GU_FALSE(condition) EXPECT_FALSE(condition)

// Expect an interface name that is Google Update-specific to have the uuid.
#define EXPECT_GU_ID_EQ(uuid, interface_id) \
    EXPECT_STREQ(CString(uuid).MakeUpper(), \
                 omaha::GuidToString(interface_id));

#else
// For open source builds, expect the values to not be equal.  (Interfaces
// should still exist.)

#define TEST_GU_INT_F(test_fixture, test_name) TEST_F(test_fixture, test_name)

#define EXPECT_GU_EQ(expected, actual) EXPECT_NE(expected, actual)
#define EXPECT_GU_STREQ(expected, actual) EXPECT_STRNE(expected, actual)
#define EXPECT_GU_TRUE(condition) EXPECT_FALSE(condition)
#define EXPECT_GU_FALSE(condition) EXPECT_TRUE(condition)

#define EXPECT_GU_ID_EQ(uuid, interface_id) \
    EXPECT_STRNE(CString(uuid).MakeUpper(), \
                 omaha::GuidToString(interface_id));

#endif


class OmahaCustomizationTypeLibComInterfaceTest : public testing::Test {
 protected:
  explicit OmahaCustomizationTypeLibComInterfaceTest(const CString& dll_name)
      : dll_name_(dll_name),
        type_lib_(NULL),
        help_context_(UINT_MAX) {
  }

  virtual void SetUp() {
    CString omaha_dll_path;
    omaha_dll_path.Format(_T("..\\staging\\%s"), dll_name_);
    EXPECT_SUCCEEDED(::LoadTypeLib(omaha_dll_path, &type_lib_));
  }

  HRESULT GetDocumentation(int type_description_index) {
    return type_lib_->GetDocumentation(type_description_index,
                                      &item_name_,
                                      &item_doc_string_,
                                      &help_context_,
                                      &help_file_);
  }

  CString dll_name_;
  ITypeLib* type_lib_;

  CComBSTR item_name_;
  CComBSTR item_doc_string_;
  unsigned long help_context_;  // NOLINT
  CComBSTR help_file_;
};

#endif  // OMAHA_TESTING_OMAHA_CUSTOMIZATION_TEST_H_
