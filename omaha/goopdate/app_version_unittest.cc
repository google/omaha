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

#include <atlbase.h>
#include <atlcom.h>

#include "omaha/base/synchronized.h"
#include "omaha/base/utils.h"
#include "omaha/common/update3_utils.h"
#include "omaha/goopdate/model.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// TODO(omaha): there is a problem with this unit test. The model is built
// bottom up. This makes it impossible to set the references to parents. Will
// have to fix the code, eventually using Builder DP to create a bunch of
// models containing bundles, apps, and such.

#if 0

const TCHAR* const kTestId = _T("{8260D23D-D23B-427F-AF1A-2CE36E6F073B}");

class ITestUnknownImpl : public IUnknown {
 public:
  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return 1;
  }
  virtual ULONG STDMETHODCALLTYPE Release() {
    return 1;
  }
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) {
    return E_NOTIMPL;
  }
};

class AppVersionTest : public testing::Test {
 protected:
  LLock lock_;
};

TEST_F(AppVersionTest, TestReadOnly) {
  ITestUnknownImpl test_unknown;
  std::unique_ptr<AppVersion> app_version;
  EXPECT_SUCCEEDED(AppVersion::Create(&lock_,
                                      &test_unknown,
                                      true,
                                      address(app_version)));

  CComPtr<IAppDataReadOnly> version_data_ro;
  EXPECT_SUCCEEDED(update3_utils::GetAppData(app_version.get(),
                                             &version_data_ro));

  CComPtr<IAppData> version_data_rw;
  EXPECT_EQ(E_NOINTERFACE,
            update3_utils::GetAppData(app_version.get(), &version_data_rw));

  // Get the IDispatch interface, and verify that it is a read-only interface.
  CComPtr<IDispatch> version_data_ro_dispatch;
  EXPECT_SUCCEEDED(
      version_data_ro.QueryInterface(&version_data_ro_dispatch));

  CComVariant var;
  EXPECT_SUCCEEDED(version_data_ro_dispatch.GetPropertyByName(_T("appGuid"),
                                                              &var));
  EXPECT_EQ(VT_BSTR, V_VT(&var));
  EXPECT_STREQ(GuidToString(GUID_NULL), V_BSTR(&var));

  var = kTestId;

  // ITypeInfo::Invoke with a DISPATCH_PROPERTYPUT results in a
  // DISP_E_BADPARAMCOUNT, and not in a DISP_E_MEMBERNOTFOUND, as I would have
  // expected.
  EXPECT_EQ(DISP_E_BADPARAMCOUNT,
            version_data_ro_dispatch.PutPropertyByName(_T("appGuid"), &var));
}

TEST_F(AppVersionTest, TestReadWrite) {
  ITestUnknownImpl test_unknown;
  std::unique_ptr<AppVersion> app_version;
  EXPECT_SUCCEEDED(AppVersion::Create(&lock_,
                                      &test_unknown,
                                      false,
                                      address(app_version)));

  CComPtr<IAppDataReadOnly> version_data_ro;
  EXPECT_SUCCEEDED(update3_utils::GetAppData(app_version.get(),
                                             &version_data_ro));

  CComPtr<IAppData> version_data_rw;
  EXPECT_SUCCEEDED(update3_utils::GetAppData(app_version.get(),
                                             &version_data_rw));

  CComPtr<IUnknown> version_data_ro_unknown;
  EXPECT_SUCCEEDED(
      version_data_ro.QueryInterface(&version_data_ro_unknown));

  CComPtr<IUnknown> version_data_rw_unknown;
  EXPECT_SUCCEEDED(
      version_data_rw.QueryInterface(&version_data_rw_unknown));

  // COM identity rule.
  EXPECT_TRUE(version_data_ro_unknown == version_data_rw_unknown);

  // Get the IDispatch interface, and verify that it is a read-write interface.
  CComPtr<IDispatch> version_data_rw_dispatch;
  EXPECT_SUCCEEDED(
      version_data_rw.QueryInterface(&version_data_rw_dispatch));

  CComVariant var;
  EXPECT_SUCCEEDED(version_data_rw_dispatch.GetPropertyByName(_T("appGuid"),
                                                              &var));
  EXPECT_EQ(VT_BSTR, V_VT(&var));
  EXPECT_STREQ(GuidToString(GUID_NULL), V_BSTR(&var));

  var = kTestId;
  EXPECT_SUCCEEDED(version_data_rw_dispatch.PutPropertyByName(_T("appGuid"),
                                                              &var));

  var.ClearToZero();
  EXPECT_SUCCEEDED(version_data_rw_dispatch.GetPropertyByName(_T("appGuid"),
                                                              &var));
  EXPECT_EQ(VT_BSTR, V_VT(&var));
  EXPECT_STREQ(kTestId, V_BSTR(&var));

  // Verify that the read-only interface returns the same value for the appGuid.
  CComBSTR app_id;
  EXPECT_SUCCEEDED(version_data_ro->get_appId(&app_id));
  EXPECT_STREQ(kTestId, app_id);
}

#endif

}  // namespace omaha
