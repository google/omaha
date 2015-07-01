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

#include "omaha/plugins/update/npapi/dispatch_host.h"
#include <atlbase.h>
#include <atlcom.h>
#include <string.h>
#include <vector>
#include "omaha/plugins/update/npapi/testing/dispatch_host_test_interface.h"
#include "omaha/plugins/update/npapi/testing/stubs.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class DispatchHostTest : public testing::Test {
 protected:
  virtual void SetUp() {
    CComPtr<IDispatch> dispatch;
    ASSERT_SUCCEEDED(
        CComCoClass<DispatchHostTestInterface>::CreateInstance(&dispatch));
    dispatch_host_ = NPN_CreateObject(NULL, &DispatchHost::kNPClass_);
    static_cast<DispatchHost*>(dispatch_host_)->dispatch_ = dispatch.Detach();
    VOID_TO_NPVARIANT(result_);
  }

  virtual void TearDown() {
    NPN_ReleaseObject(dispatch_host_);
    for (std::vector<NPVariant>::iterator it = args_.begin();
         it != args_.end(); ++it) {
      NPN_ReleaseVariantValue(&*it);
    }
    NPN_ReleaseVariantValue(&result_);
  }

  void UseTestInterface2() {
    NPN_ReleaseObject(dispatch_host_);
    CComPtr<IDispatch> dispatch;
    ASSERT_SUCCEEDED(
        CComCoClass<DispatchHostTestInterface2>::CreateInstance(&dispatch));
    dispatch_host_ = NPN_CreateObject(NULL, &DispatchHost::kNPClass_);
    static_cast<DispatchHost*>(dispatch_host_)->dispatch_ = dispatch.Detach();
  }

  void PushArg(bool value) {
    args_.push_back(NPVariant());
    BOOLEAN_TO_NPVARIANT(value, args_.back());
  }

  void PushArg(int32 value) {
    args_.push_back(NPVariant());
    INT32_TO_NPVARIANT(value, args_.back());
  }

  void PushArg(double value) {
    args_.push_back(NPVariant());
    DOUBLE_TO_NPVARIANT(value, args_.back());
  }

  void PushArg(const char* value) {
    args_.push_back(NPVariant());

    #pragma warning(push)
    // conversion from 'size_t' to 'uint32_t', possible loss of data.
    #pragma warning(disable : 4267)

    // TODO(omaha): _strdup is an implementation detail of the stubs.
    STRINGZ_TO_NPVARIANT(_strdup(value), args_.back());

    #pragma warning(pop)
  }

  NPObject* dispatch_host_;
  NPIdentifierFactory id_factory_;

  std::vector<NPVariant> args_;
  NPVariant result_;
};

TEST_F(DispatchHostTest, HasMethod) {
  EXPECT_TRUE(NPN_HasMethod(NULL, dispatch_host_,
                            id_factory_.Create("Random")));
  EXPECT_TRUE(NPN_HasMethod(NULL, dispatch_host_,
                            id_factory_.Create("AddAsMethod")));

  // Property getters with input arguments should be treated as methods.
  EXPECT_TRUE(NPN_HasMethod(NULL, dispatch_host_,
                            id_factory_.Create("AddAsProperty")));

  // Properties and non-existent members are not methods.
  EXPECT_FALSE(NPN_HasMethod(NULL, dispatch_host_,
                             id_factory_.Create("Property")));
  EXPECT_FALSE(NPN_HasMethod(NULL, dispatch_host_,
                             id_factory_.Create("ReadOnlyProperty")));
  EXPECT_FALSE(NPN_HasMethod(NULL, dispatch_host_,
                             id_factory_.Create("WriteOnlyProperty")));
  EXPECT_FALSE(NPN_HasMethod(NULL, dispatch_host_,
                             id_factory_.Create("DoesNotExist")));
}

TEST_F(DispatchHostTest, InvokeNoArgs) {
  EXPECT_TRUE(NPN_Invoke(NULL, dispatch_host_, id_factory_.Create("Random"),
                         NULL, 0, &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(42, result_.value.intValue);
}

TEST_F(DispatchHostTest, InvokeWithArgs) {
  PushArg(7);
  PushArg(27);
  EXPECT_TRUE(NPN_Invoke(NULL, dispatch_host_,
                         id_factory_.Create("AddAsMethod"), &args_.front(),
                         static_cast<uint32_t>(args_.size()), &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(34, result_.value.intValue);
}

TEST_F(DispatchHostTest, InvokePropertyWithArgs) {
  // Property getters that have input args should be handle by Invoke
  PushArg(8);
  PushArg(15);
  EXPECT_TRUE(NPN_Invoke(NULL, dispatch_host_,
                         id_factory_.Create("AddAsProperty"), &args_.front(),
                         static_cast<uint32_t>(args_.size()), &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(23, result_.value.intValue);
}

TEST_F(DispatchHostTest, InvokeNonexistentMethod) {
  // Non-existent method, should fail.
  INT32_TO_NPVARIANT(0x19821982, result_);
  EXPECT_FALSE(NPN_Invoke(NULL, dispatch_host_,
                          id_factory_.Create("NonExistent"), NULL, 0,
                          &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(DispatchHostTest, InvokeWithIncompatibleArgs) {
  PushArg("Hello World!");
  PushArg(0x19851985);
  INT32_TO_NPVARIANT(0x19881988, result_);
  EXPECT_FALSE(NPN_Invoke(NULL, dispatch_host_,
                          id_factory_.Create("AddAsMethod"), &args_.front(),
                          static_cast<uint32_t>(args_.size()), &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(DispatchHostTest, InvokeWithIncorrectNumberOfArgs) {
  PushArg("Don't panic.");
  INT32_TO_NPVARIANT(0x77777777, result_);
  EXPECT_FALSE(NPN_Invoke(NULL, dispatch_host_, id_factory_.Create("Random"),
                          &args_.front(), static_cast<uint32_t>(args_.size()),
                          &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(DispatchHostTest, InvokeDefault) {
  EXPECT_TRUE(NPN_InvokeDefault(NULL, dispatch_host_, NULL, 0, &result_));
  EXPECT_TRUE(NPVARIANT_IS_OBJECT(result_));
}

TEST_F(DispatchHostTest, InvokeDefaultPropertyWithArgs) {
  UseTestInterface2();
  PushArg(1048576);
  EXPECT_TRUE(NPN_InvokeDefault(NULL, dispatch_host_, &args_.front(),
                                static_cast<uint32_t>(args_.size()), &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(1048576 * 2, result_.value.intValue);
}

// TODO(omaha): implement negative test

TEST_F(DispatchHostTest, HasProperty) {
  EXPECT_TRUE(NPN_HasProperty(NULL, dispatch_host_,
                              id_factory_.Create("Property")));
  EXPECT_TRUE(NPN_HasProperty(NULL, dispatch_host_,
                              id_factory_.Create("ReadOnlyProperty")));
  EXPECT_TRUE(NPN_HasProperty(NULL, dispatch_host_,
                              id_factory_.Create("WriteOnlyProperty")));

  // Property getters with input arguments should not be treated as properties.
  EXPECT_FALSE(NPN_HasProperty(NULL, dispatch_host_,
                               id_factory_.Create("AddAsProperty")));

  // Methods and non-existent members are not properties.
  EXPECT_FALSE(NPN_HasProperty(NULL, dispatch_host_,
                               id_factory_.Create("Random")));
  EXPECT_FALSE(NPN_HasProperty(NULL, dispatch_host_,
                               id_factory_.Create("DoesNotExist")));
}

TEST_F(DispatchHostTest, GetProperty) {
  EXPECT_TRUE(NPN_GetProperty(NULL, dispatch_host_,
                              id_factory_.Create("Property"), &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(0xdeadbeef, result_.value.intValue);
}

TEST_F(DispatchHostTest, GetPropertyReadOnly) {
  EXPECT_TRUE(NPN_GetProperty(NULL, dispatch_host_,
                              id_factory_.Create("ReadOnlyProperty"),
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(19700101, result_.value.intValue);
}

TEST_F(DispatchHostTest, SetProperty) {
  PushArg(20002000);
  EXPECT_TRUE(NPN_SetProperty(NULL, dispatch_host_,
                              id_factory_.Create("Property"), &args_.front()));
}

TEST_F(DispatchHostTest, SetPropertyWriteOnly) {
  PushArg(20612061);
  EXPECT_TRUE(NPN_SetProperty(NULL, dispatch_host_,
                              id_factory_.Create("WriteOnlyProperty"),
                              &args_.front()));
}

TEST_F(DispatchHostTest, Unsupported) {
  EXPECT_FALSE(NPN_RemoveProperty(NULL, dispatch_host_, NULL));
  EXPECT_FALSE(NPN_Enumerate(NULL, dispatch_host_, NULL, NULL));
  EXPECT_FALSE(NPN_Construct(NULL, dispatch_host_, NULL, 0, NULL));
}

}  // namespace omaha
