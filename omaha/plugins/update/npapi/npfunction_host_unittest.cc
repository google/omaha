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

#include "omaha/plugins/update/npapi/npfunction_host.h"
#include <atlbase.h>
#include <atlcom.h>
#include <string.h>
#include <vector>
#include "omaha/plugins/update/npapi/testing/dispatch_host_test_interface.h"
#include "omaha/plugins/update/npapi/testing/stubs.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class NpFunctionHostTest;

class MockFunctionNPObject : public NPObject {
 public:
  static MockFunctionNPObject* CreateInstance(NpFunctionHostTest* creator) {
    NPObject* obj = NPN_CreateObject(NULL, &MockFunctionNPObject::kNPClass_);
    MockFunctionNPObject* realobj = static_cast<MockFunctionNPObject*>(obj);
    realobj->creator_ = creator;
    return realobj;
  }

  static NPObject* Allocate(NPP npp, NPClass* class_functions) {
    UNREFERENCED_PARAMETER(class_functions);
    return new MockFunctionNPObject(npp);
  }

  static void Deallocate(NPObject* object) {
    delete static_cast<MockFunctionNPObject*>(object);
  }

  static bool InvokeDefault(NPObject* object,
                            const NPVariant* args,
                            uint32_t arg_count,
                            NPVariant* result) {
    MockFunctionNPObject* realobj = static_cast<MockFunctionNPObject*>(object);
    return realobj->InvokeDefaultLocal(args, arg_count, result);
  }

  bool InvokeDefaultLocal(const NPVariant* args,
                          uint32_t arg_count,
                          NPVariant* result);

 protected:
  explicit MockFunctionNPObject(NPP npp) : npp_(npp), creator_(NULL) {}

  static NPUTF8* NPN_ReallocateStringZ(const char* string) {
    uint32 buflen = static_cast<uint32>(strlen(string) + 1);
    NPUTF8* npnstr = reinterpret_cast<NPUTF8*>(NPN_MemAlloc(buflen));
    memmove(npnstr, string, buflen);
    return npnstr;
  }

 private:
  NPP npp_;
  NpFunctionHostTest* creator_;

  // The NPObject vtable.
  static NPClass kNPClass_;
};

class NpFunctionHostTest : public testing::Test {
 protected:
  friend class MockFunctionNPObject;

  virtual void SetUp() {
    function_ = MockFunctionNPObject::CreateInstance(this);
    EXPECT_SUCCEEDED(NpFunctionHost::Create(NULL, function_, &host_));
  }

  virtual void TearDown() {
  }

  NPObject* function_;
  CComPtr<IDispatch> host_;

  std::vector<NPVariant> mock_args_;
  NPVariant mock_result_;
};

NPClass MockFunctionNPObject::kNPClass_ = {
  NP_CLASS_STRUCT_VERSION,
  Allocate,
  Deallocate,
  NULL,
  NULL,
  NULL,
  InvokeDefault,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

bool MockFunctionNPObject::InvokeDefaultLocal(const NPVariant* args,
                                              uint32_t arg_count,
                                              NPVariant* result) {
  const char* kMultiStringReturn = "multi";

  // The mock NPObject exhibits the following external behavior:
  // * If no arguments, return nothing
  // * If one argument, return a boolean (true)
  // * If two arguments, return a string ("multi")
  // * Otherwise, treat it as an invoke failure.
  // It also copies the arguments as supplied, and the intended NPVariant
  // return value, to the test closure that created it.

  creator_->mock_args_.resize(arg_count);
  for (uint32_t i = 0; i < arg_count; ++i) {
    creator_->mock_args_[i] = args[i];
  }

  switch (arg_count) {
    case 0:
      VOID_TO_NPVARIANT(*result);
      break;
    case 1:
      BOOLEAN_TO_NPVARIANT(true, *result);
      break;
    case 2:
      {
      #pragma warning(push)
      // conversion from 'size_t' to 'uint32_t', possible loss of data.
      #pragma warning(disable : 4267)

        NPUTF8* utf8string = NPN_ReallocateStringZ(kMultiStringReturn);
        STRINGZ_TO_NPVARIANT(utf8string, *result);

      #pragma warning(pop)
      }
      break;
    default:
      return false;
  }

  creator_->mock_result_ = *result;
  return true;
}

TEST_F(NpFunctionHostTest, GetTypeInfoCount) {
  UINT typeinfos_available = 1;
  EXPECT_SUCCEEDED(host_->GetTypeInfoCount(&typeinfos_available));
  EXPECT_EQ(0, typeinfos_available);
}

TEST_F(NpFunctionHostTest, GetTypeInfo_NotImplemented) {
  ITypeInfo* typeinfo = NULL;

  EXPECT_EQ(E_NOTIMPL, host_->GetTypeInfo(0, LOCALE_SYSTEM_DEFAULT, &typeinfo));
}

TEST_F(NpFunctionHostTest, GetIDsOfNames_NotImplemented) {
  LPOLESTR member_name = L"NonexistentMember";
  DISPID member_dispid = 0;
  EXPECT_EQ(E_NOTIMPL, host_->GetIDsOfNames(IID_NULL, &member_name, 1,
                                            LOCALE_SYSTEM_DEFAULT,
                                            &member_dispid));
}

TEST_F(NpFunctionHostTest, Invoke_NonMethod_NotSupported) {
  EXPECT_EQ(DISP_E_MEMBERNOTFOUND, host_->Invoke(0, IID_NULL,
                                                 LOCALE_SYSTEM_DEFAULT,
                                                 DISPATCH_PROPERTYGET,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 NULL));
}

TEST_F(NpFunctionHostTest, Invoke_NamedArgs_NotSupported) {
  DISPID param_name = 12;
  DISPPARAMS params = {};
  params.cNamedArgs = 1;
  params.rgdispidNamedArgs = &param_name;
  EXPECT_EQ(DISP_E_NONAMEDARGS, host_->Invoke(0, IID_NULL,
                                              LOCALE_SYSTEM_DEFAULT,
                                              DISPATCH_METHOD,
                                              &params,
                                              NULL,
                                              NULL,
                                              NULL));
}

TEST_F(NpFunctionHostTest, Invoke_NoArgs_NullDispParams) {
  VARIANT retval = {};
  EXPECT_SUCCEEDED(host_->Invoke(0, IID_NULL,
                                 LOCALE_SYSTEM_DEFAULT,
                                 DISPATCH_METHOD,
                                 NULL,
                                 &retval,
                                 NULL,
                                 NULL));

  EXPECT_EQ(0, static_cast<int>(mock_args_.size()));

  EXPECT_TRUE(NPVARIANT_IS_VOID(mock_result_));
  EXPECT_EQ(VT_EMPTY, retval.vt);
  VariantClear(&retval);
}

TEST_F(NpFunctionHostTest, Invoke_NoArgs_ValidDispParams) {
  VARIANT retval = {};
  DISPPARAMS params = {};
  EXPECT_SUCCEEDED(host_->Invoke(0, IID_NULL,
                                 LOCALE_SYSTEM_DEFAULT,
                                 DISPATCH_METHOD,
                                 &params,
                                 &retval,
                                 NULL,
                                 NULL));

  EXPECT_EQ(0, static_cast<int>(mock_args_.size()));

  EXPECT_TRUE(NPVARIANT_IS_VOID(mock_result_));
  EXPECT_EQ(VT_EMPTY, retval.vt);
  VariantClear(&retval);
}

TEST_F(NpFunctionHostTest, Invoke_NoArgs_OneParam) {
  const int kTestIntVal = 0xDEADBEEF;

  VARIANT retval = {};
  VARIANT firstparam = {};
  firstparam.vt = VT_I4;
  firstparam.intVal = kTestIntVal;

  DISPPARAMS dispparams = {};
  dispparams.cArgs = 1;
  dispparams.rgvarg = &firstparam;

  EXPECT_SUCCEEDED(host_->Invoke(0, IID_NULL,
                                 LOCALE_SYSTEM_DEFAULT,
                                 DISPATCH_METHOD,
                                 &dispparams,
                                 &retval,
                                 NULL,
                                 NULL));

  EXPECT_EQ(1, mock_args_.size());
  EXPECT_TRUE(NPVARIANT_IS_INT32(mock_args_[0]));
  EXPECT_EQ(kTestIntVal, NPVARIANT_TO_INT32(mock_args_[0]));

  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(mock_result_));
  EXPECT_EQ(true, NPVARIANT_TO_BOOLEAN(mock_result_));
  EXPECT_EQ(VT_BOOL, retval.vt);
  EXPECT_EQ(VARIANT_TRUE, retval.boolVal);
  VariantClear(&retval);
}

TEST_F(NpFunctionHostTest, Invoke_NoArgs_TwoParams) {
  const double kTestFloatVal = 3.1415927;

  VARIANT retval = {};
  VARIANT params[2] = {};
  params[0].vt = VT_BOOL;           // Invoke expects args in reverse order
  params[0].intVal = VARIANT_TRUE;
  params[1].vt = VT_R8;
  params[1].dblVal = kTestFloatVal;

  DISPPARAMS dispparams = {};
  dispparams.cArgs = 2;
  dispparams.rgvarg = params;
  EXPECT_SUCCEEDED(host_->Invoke(0, IID_NULL,
                                 LOCALE_SYSTEM_DEFAULT,
                                 DISPATCH_METHOD,
                                 &dispparams,
                                 &retval,
                                 NULL,
                                 NULL));

  EXPECT_EQ(2, mock_args_.size());
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(mock_args_[0]));
  EXPECT_EQ(kTestFloatVal, NPVARIANT_TO_DOUBLE(mock_args_[0]));
  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(mock_args_[1]));
  EXPECT_EQ(true, NPVARIANT_TO_BOOLEAN(mock_args_[1]));

  EXPECT_TRUE(NPVARIANT_IS_STRING(mock_result_));
  // Don't check mock_result's contents; it will have been released by Invoke()
  EXPECT_EQ(VT_BSTR, retval.vt);
  EXPECT_STREQ(CString("multi"), CString(retval.bstrVal));
  VariantClear(&retval);
}

TEST_F(NpFunctionHostTest, Invoke_NoArgs_ThreeParams) {
  VARIANT retval = {};
  VARIANT params[3] = {};
  for (int i = 0; i < 3; ++i) {
    params[i].vt = VT_BOOL;
    params[i].intVal = VARIANT_TRUE;
  }

  DISPPARAMS dispparams = {};
  dispparams.cArgs = 3;
  dispparams.rgvarg = params;
  EXPECT_EQ(E_FAIL, host_->Invoke(0, IID_NULL,
                                  LOCALE_SYSTEM_DEFAULT,
                                  DISPATCH_METHOD,
                                  &dispparams,
                                  &retval,
                                  NULL,
                                  NULL));
  EXPECT_EQ(3, mock_args_.size());
}


}  // namespace omaha
