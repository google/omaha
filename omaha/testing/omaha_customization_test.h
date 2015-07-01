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
#include "omaha/base/app_util.h"

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

    omaha_dll_path.Format(_T("%s\\..\\staging\\%s"),
                          omaha::app_util::GetModuleDirectory(NULL),
                          dll_name_);
    EXPECT_SUCCEEDED(::LoadTypeLib(omaha_dll_path, &type_lib_));
  }

  HRESULT GetTypeLibDocumentation() {
    return type_lib_->GetDocumentation(-1,
                                       &item_name_,
                                       &item_doc_string_,
                                       &help_context_,
                                       &help_file_);
  }

  HRESULT GetDocumentation(const CString& type_name) {
    ITypeInfo* type_info[] = {NULL, NULL};
    MEMBERID member_id[] = {MEMBERID_NIL, MEMBERID_NIL};
    USHORT found = 2;

    CComBSTR type_name_bstr(type_name);
    HRESULT hr = type_lib_->FindName(type_name_bstr,
                                     0,
                                     type_info,
                                     member_id,
                                     &found);

    if (FAILED(hr)) {
      return hr;
    }

    EXPECT_EQ(1, found);
    EXPECT_EQ(MEMBERID_NIL, member_id[0]);

    if (found != 1 || member_id[0] != MEMBERID_NIL) {
      hr = TYPE_E_ELEMENTNOTFOUND;
    } else {
      hr = type_info[0]->GetDocumentation(member_id[0],
                                          &item_name_,
                                          &item_doc_string_,
                                          &help_context_,
                                          &help_file_);
    }

    for (int i = 0; i < found; ++i) {
      type_info[i]->Release();
    }

    return hr;
  }

  CString dll_name_;
  ITypeLib* type_lib_;

  CComBSTR item_name_;
  CComBSTR item_doc_string_;
  unsigned long help_context_;  // NOLINT
  CComBSTR help_file_;
};

#endif  // OMAHA_TESTING_OMAHA_CUSTOMIZATION_TEST_H_
