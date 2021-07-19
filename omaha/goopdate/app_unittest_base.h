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

// Provides a base framework for unit tests that need an App object.

#ifndef OMAHA_GOOPDATE_APP_UNITTEST_BASE_H_
#define OMAHA_GOOPDATE_APP_UNITTEST_BASE_H_

#include <atlbase.h>
#include <atlcom.h>

#include "omaha/base/app_util.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/goopdate/app_bundle_state_initialized.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/update_response_utils.h"
#include "omaha/goopdate/worker_mock.h"
#include "omaha/testing/unit_test.h"

using ::testing::Return;

namespace omaha {

class AppTestBase : public testing::Test {
 protected:
  AppTestBase(bool is_machine, bool use_strict_mock)
      : is_machine_(is_machine),
        use_strict_mock_(use_strict_mock),
        goopdate_(is_machine) {
  }

  virtual void SetUp() {
    EXPECT_SUCCEEDED(AppManager::CreateInstance(is_machine_));

    // Needed for error strings.
    EXPECT_SUCCEEDED(ResourceManager::Create(
                         is_machine_,
                         app_util::GetCurrentModuleDirectory(),
                         _T("en")));

    if (use_strict_mock_) {
      mock_worker_.reset(new testing::StrictMock<MockWorker>);
    } else {
      mock_worker_.reset(new testing::NiceMock<MockWorker>);
    }

    EXPECT_CALL(*mock_worker_, Lock()).WillRepeatedly(Return(2));
    EXPECT_CALL(*mock_worker_, Unlock()).WillRepeatedly(Return(1));

    model_.reset(new Model(mock_worker_.get()));

    app_bundle_ = model_->CreateAppBundle(is_machine_);
    ASSERT_TRUE(app_bundle_.get());

    EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("Test Bundle"))));
    EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
    EXPECT_SUCCEEDED(app_bundle_->put_installSource(CComBSTR(_T("unittest"))));
    // TODO(omaha3): Address with the TODO in AppBundleInitializedTest::SetUp()
    // then remove app_bundle_state_initialized.h above.
    if (is_machine_) {
      SetAppBundleStateForUnitTest(app_bundle_.get(),
                                   new fsm::AppBundleStateInitialized);
    } else {
      EXPECT_SUCCEEDED(app_bundle_->initialize());
    }
  }

  virtual void TearDown() {
    ResourceManager::Delete();
    AppManager::DeleteInstance();
  }

  static HRESULT LoadBundleFromXml(AppBundle* app_bundle,
                                   const CStringA& buffer_string) {
    __mutexScope(app_bundle->model()->lock());

    std::vector<uint8> buffer(buffer_string.GetLength());
    memcpy(&buffer.front(), buffer_string, buffer.size());

    std::unique_ptr<xml::UpdateResponse> update_response(
        xml::UpdateResponse::Create());
    HRESULT hr = update_response->Deserialize(buffer);
    if (FAILED(hr)) {
      return hr;
    }

    for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
      hr = update_response_utils::BuildApp(update_response.get(),
                                           S_OK,
                                           app_bundle->GetApp(i));
      if (FAILED(hr)) {
        return hr;
      }
    }

    return S_OK;
  }

  const bool is_machine_;
  const bool use_strict_mock_;

  CString hive_override_key_name_;

  std::unique_ptr<MockWorker> mock_worker_;
  std::unique_ptr<Model> model_;

  Goopdate goopdate_;
  std::shared_ptr<AppBundle> app_bundle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppTestBase);
};

// Overrides the registry.
class AppTestBaseWithRegistryOverride
    : public AppTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  AppTestBaseWithRegistryOverride(bool is_machine, bool use_strict_mock)
      : AppTestBase(is_machine, use_strict_mock),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {}

  // Override the registry after initializing the AppBundle so that the latter
  // has the correct network configuration in the event there are pings to send.
  // TODO(omaha3): Ideally we would not send pings from tests: http://b/2911608.
  virtual void SetUp() {
    AppTestBase::SetUp();

    // Registry redirection impacts the creation of the COM XML parser.
    // This code instantiates the parser before registry redirection occurs.
    {
      CComPtr<IXMLDOMDocument> document;
      EXPECT_SUCCEEDED(CoCreateSafeDOMDocument(&document));
    }

    RegKey::DeleteKey(hive_override_key_name_);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV, kRegValueIsEnrolledToDomain);

    RestoreRegistryHives();
    RegKey::DeleteKey(hive_override_key_name_);

    ConfigManager::DeleteInstance();

    AppTestBase::TearDown();
  }

  bool IsDomain() {
    return GetParam();
  }

  void SetEnrolledPolicy(const CString& policy, DWORD value) {
    EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueIsEnrolledToDomain,
                                      IsDomain() ? 1UL : 0UL));
    EXPECT_SUCCEEDED(SetPolicy(policy, value));
  }

  CString hive_override_key_name_;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_UNITTEST_BASE_H_
