// Copyright 2009 Google Inc.
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

#include "omaha/plugins/update/npapi/variant_utils.h"
#include <atlbase.h>
#include <string.h>
#include <stdlib.h>
#include "omaha/plugins/update/npapi/testing/dispatch_host_test_interface.h"
#include "omaha/plugins/update/npapi/testing/stubs.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class VariantUtilsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    VOID_TO_NPVARIANT(np_variant_);
  }

  virtual void TearDown() {
    NPN_ReleaseVariantValue(&np_variant_);
  }

  void TestNPV2V() {
    NPVariantToVariant(NULL, np_variant_, &variant_);
    ExpectTypesEquivalent();
  }

  void TestV2NPV() {
    NPN_ReleaseVariantValue(&np_variant_);
    VariantToNPVariant(NULL, variant_, &np_variant_);
    ExpectTypesEquivalent();
  }

  void ExpectTypesEquivalent() {
    switch (V_VT(&variant_)) {
      case VT_EMPTY:
        EXPECT_TRUE(NPVARIANT_IS_VOID(np_variant_));
        return;
        break;
      case VT_NULL:
        EXPECT_TRUE(NPVARIANT_IS_NULL(np_variant_));
        return;
        break;
      case VT_BOOL:
        EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(np_variant_));
        return;
        break;
      case VT_I4:
      case VT_UI4:
        EXPECT_TRUE(NPVARIANT_IS_INT32(np_variant_));
        return;
        break;
      case VT_R8:
        EXPECT_TRUE(NPVARIANT_IS_DOUBLE(np_variant_));
        return;
        break;
      case VT_BSTR:
        EXPECT_TRUE(NPVARIANT_IS_STRING(np_variant_));
        return;
        break;
      case VT_DISPATCH:
        EXPECT_TRUE(NPVARIANT_IS_OBJECT(np_variant_));
        return;
        break;
      default:
        break;
    }
    ASSERT(false, (L"Expected equivalent types but got the following instead:\n"
                   L"  np_variant_.type -> %d\n  V_VT(&variant_) -> %d\n",
                   np_variant_.type, V_VT(&variant_)));
  }

  NPVariant np_variant_;
  CComVariant variant_;
};

TEST_F(VariantUtilsTest, NPVariantToVariant_Void) {
  VOID_TO_NPVARIANT(np_variant_);
  TestNPV2V();
}

TEST_F(VariantUtilsTest, NPVariantToVariant_Null) {
  NULL_TO_NPVARIANT(np_variant_);
  TestNPV2V();
}

TEST_F(VariantUtilsTest, NPVariantToVariant_Bool) {
  BOOLEAN_TO_NPVARIANT(true, np_variant_);
  TestNPV2V();
  EXPECT_EQ(VARIANT_TRUE, V_BOOL(&variant_));

  BOOLEAN_TO_NPVARIANT(false, np_variant_);
  TestNPV2V();
  EXPECT_EQ(VARIANT_FALSE, V_BOOL(&variant_));
}

TEST_F(VariantUtilsTest, NPVariantToVariant_Int32) {
  INT32_TO_NPVARIANT(kint32min, np_variant_);
  TestNPV2V();
  EXPECT_EQ(kint32min, V_I4(&variant_));

  INT32_TO_NPVARIANT(0, np_variant_);
  TestNPV2V();
  EXPECT_EQ(0, V_I4(&variant_));

  INT32_TO_NPVARIANT(kint32max, np_variant_);
  TestNPV2V();
  EXPECT_EQ(kint32max, V_I4(&variant_));
}

TEST_F(VariantUtilsTest, NPVariantToVariant_Double) {
  DOUBLE_TO_NPVARIANT(-1, np_variant_);
  TestNPV2V();
  EXPECT_DOUBLE_EQ(-1, V_R8(&variant_));

  DOUBLE_TO_NPVARIANT(0, np_variant_);
  TestNPV2V();
  EXPECT_DOUBLE_EQ(0, V_R8(&variant_));

  DOUBLE_TO_NPVARIANT(static_cast<double>(kuint64max),
                      np_variant_);
  TestNPV2V();
  EXPECT_DOUBLE_EQ(static_cast<double>(kuint64max), V_R8(&variant_));
}

TEST_F(VariantUtilsTest, NPVariantToVariant_String) {
  #pragma warning(push)
  // conversion from 'size_t' to 'uint32_t', possible loss of data.
  #pragma warning(disable : 4267)

  // TODO(omaha): _strdup depends on an implementation detail of the stubs.
  STRINGZ_TO_NPVARIANT(_strdup(""), np_variant_);
  TestNPV2V();
  EXPECT_STREQ(L"", V_BSTR(&variant_));

  // Force the length to be zero.
  STRINGZ_TO_NPVARIANT(_strdup("junk"), np_variant_);
  np_variant_.value.stringValue.UTF8Length = 0;
  TestNPV2V();
  EXPECT_STREQ(L"", V_BSTR(&variant_));

  STRINGZ_TO_NPVARIANT(_strdup("ROBERT'); DROP TABLE Students; --"),
                       np_variant_);
  TestNPV2V();
  EXPECT_STREQ(L"ROBERT'); DROP TABLE Students; --", V_BSTR(&variant_));

  // Check that NPVariantToVariant properly converts UTF-8 to UTF-16.
  STRINGZ_TO_NPVARIANT(_strdup("one: \xe4\xb8\x80"), np_variant_);
  TestNPV2V();
  EXPECT_STREQ(L"one: \x4e00", V_BSTR(&variant_));

  #pragma warning(pop)
}
/*
TEST_F(VariantUtilsTest, NPVariantToVariant_Unsupported) {
  // NPVariantType_Object -> VT_DISPATCH conversion is not supported.
  ExpectAsserts expect_asserts;
  OBJECT_TO_NPVARIANT(NULL, np_variant_);
  variant_ = 24;
  NPVariantToVariant(NULL, np_variant_, &variant_);
  EXPECT_EQ(VT_EMPTY, V_VT(&variant_));
  // Manual cleanup, since OBJECT_TO_NPVARIANT macro was used with a NULL
  // NPObject, which is normally illegal.
  VOID_TO_NPVARIANT(np_variant_);
}
*/
TEST_F(VariantUtilsTest, VariantToNPVariant_VT_EMPTY) {
  variant_.ChangeType(VT_EMPTY);
  TestV2NPV();
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_NULL) {
  variant_.ChangeType(VT_NULL);
  TestV2NPV();
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_BOOL) {
  variant_ = true;
  TestV2NPV();
  EXPECT_TRUE(np_variant_.value.boolValue);

  variant_ = false;
  TestV2NPV();
  EXPECT_FALSE(np_variant_.value.boolValue);
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_I4) {
  variant_ = kint32max;
  TestV2NPV();
  EXPECT_EQ(kint32max, np_variant_.value.intValue);

  variant_ = 0;
  TestV2NPV();
  EXPECT_EQ(0, np_variant_.value.intValue);

  variant_ = kint32min;
  TestV2NPV();
  EXPECT_EQ(kint32min, np_variant_.value.intValue);
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_UI4) {
  variant_ = 0U;
  TestV2NPV();
  EXPECT_EQ(0, np_variant_.value.intValue);

  variant_ = static_cast<uint32>(kint32max);
  TestV2NPV();
  EXPECT_EQ(kint32max, np_variant_.value.intValue);

  // MSIE can natively support VT_UI4. Unfortunately, Firefox cannot.
  // Check that kuint32max wraps around to -1.
  variant_ = kuint32max;
  TestV2NPV();
  EXPECT_EQ(-1, np_variant_.value.intValue);
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_R8) {
  variant_ = 0.0;
  TestV2NPV();
  EXPECT_DOUBLE_EQ(0.0, np_variant_.value.doubleValue);

  variant_ = -1.0;
  TestV2NPV();
  EXPECT_DOUBLE_EQ(-1.0, np_variant_.value.doubleValue);

  variant_ = static_cast<double>(kuint64max);
  TestV2NPV();
  EXPECT_DOUBLE_EQ(static_cast<double>(kuint64max),
                   np_variant_.value.doubleValue);
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_BSTR) {
  variant_ = "";
  TestV2NPV();
  EXPECT_STREQ("", np_variant_.value.stringValue.UTF8Characters);

  variant_ = L"sudo make me a sandwich";
  TestV2NPV();
  EXPECT_STREQ("sudo make me a sandwich",
               np_variant_.value.stringValue.UTF8Characters);

  // A NULL BSTR should be treated as an empty string.
  V_VT(&variant_) = VT_BSTR;
  V_BSTR(&variant_) = NULL;
  TestV2NPV();
  EXPECT_STREQ("", np_variant_.value.stringValue.UTF8Characters);

  // Check that VariantToNPVariant properly converts UTF-16 to UTF-8.
  variant_ = L"one: \x4e00";
  TestV2NPV();
  EXPECT_STREQ("one: \xe4\xb8\x80",
               np_variant_.value.stringValue.UTF8Characters);
}

TEST_F(VariantUtilsTest, VariantToNPVariant_VT_DISPATCH) {
  CComPtr<IDispatch> dispatch;
  ASSERT_SUCCEEDED(
      CComCoClass<DispatchHostTestInterface>::CreateInstance(&dispatch));
  variant_ = dispatch;
  TestV2NPV();

  // Check that the wrapped object's methods can be called.
  NPIdentifierFactory identifier;
  NPVariant result;
  VOID_TO_NPVARIANT(result);
  EXPECT_TRUE(NPN_Invoke(NULL, np_variant_.value.objectValue,
                         identifier.Create("Random"), NULL, 0, &result));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result));
  EXPECT_EQ(42, result.value.intValue);
}

TEST_F(VariantUtilsTest, VariantToNPVariant_Unsupported) {
  // Legal variant types inferred from oaidl.idl and wtypes.h. Note that some
  // types that aren't marked as appearing in VARIANTs still have a VARIANT
  // field for that type, so they are included anyway...

  // Note that VT_UNKNOWN must not be the last element in the array; otherwise
  // CComVariant will attempt to call Release() on a NULL pointer.
  const VARTYPE kUnsupportedSimpleTypes[] = {
    VT_I2, VT_R4, VT_CY, VT_DATE, VT_ERROR, VT_VARIANT, VT_UNKNOWN, VT_DECIMAL,
    VT_RECORD, VT_I1, VT_UI1, VT_UI2, VT_I8, VT_UI8, VT_INT, VT_UINT, VT_BYREF
  };
  for (int i = 0; i < arraysize(kUnsupportedSimpleTypes); ++i) {
    ExpectAsserts expect_asserts;
    V_VT(&variant_) = kUnsupportedSimpleTypes[i];
    INT32_TO_NPVARIANT(42, np_variant_);
    TestV2NPV();
    EXPECT_TRUE(NPVARIANT_IS_VOID(np_variant_));
  }

  // Compound modifiers.
  const VARTYPE kCompoundModifiers[] = {
    VT_ARRAY, VT_BYREF, VT_ARRAY | VT_BYREF
  };
  // Compound types.
  const VARTYPE kCompoundableTypes[] = {
    VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BSTR, VT_DISPATCH, VT_ERROR,
    VT_BOOL, VT_VARIANT, VT_UNKNOWN, VT_DECIMAL, VT_RECORD, VT_I1, VT_UI1,
    VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT
  };

  for (int i = 0; i < arraysize(kCompoundModifiers); ++i) {
    for (int j = 0; j < arraysize(kCompoundableTypes); ++j) {
      ExpectAsserts expect_asserts;
      V_VT(&variant_) = kCompoundModifiers[i] | kCompoundableTypes[j];
      INT32_TO_NPVARIANT(42, np_variant_);
      TestV2NPV();
      EXPECT_TRUE(NPVARIANT_IS_VOID(np_variant_));
    }
  }
}

}  // namespace omaha
