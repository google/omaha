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

#include "omaha/base/error.h"
#include "omaha/client/bundle_installer.h"
#include "omaha/client/resource.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

using internal::BuildAppNameList;

class BuildAppNameListTest : public testing::Test {
 public:
 protected:
  std::vector<CString> app_names_;
};

TEST_F(BuildAppNameListTest, OneApp) {
  app_names_.push_back(_T("Test App1"));
  EXPECT_STREQ(_T("Test App1"), BuildAppNameList(app_names_));
}

TEST_F(BuildAppNameListTest, TwoApps) {
  app_names_.push_back(_T("Test App1"));
  app_names_.push_back(_T("Next App2"));
  EXPECT_STREQ(_T("Test App1, Next App2"), BuildAppNameList(app_names_));
}

TEST_F(BuildAppNameListTest, ManyApps) {
  app_names_.push_back(_T("Test App1"));
  app_names_.push_back(_T("Next App2"));
  app_names_.push_back(_T("My App3"));
  app_names_.push_back(_T("Your App4"));
  app_names_.push_back(_T("Other App5"));
  EXPECT_STREQ(_T("Test App1, Next App2, My App3, Your App4, Other App5"),
               BuildAppNameList(app_names_));
}

// TODO(omaha): Load "ar" resources and enable after we get translations.
TEST_F(BuildAppNameListTest, DISABLED_ManyApps_Bidi) {
  app_names_.push_back(_T("Test App1"));
  app_names_.push_back(_T("Next App2"));
  app_names_.push_back(_T("My App3"));
  app_names_.push_back(_T("Your App4"));
  app_names_.push_back(_T("Other App5"));
  EXPECT_STREQ(_T("Other App5, Your App4, My App3, Next App2, Test App1"),
               BuildAppNameList(app_names_));
}

class GetBundleCompletionMessageTest : public testing::Test {
 public:
 protected:
  void AddSucceededAppInfo(int id) {
    AppCompletionInfo app_info;
    app_info.display_name.Format(_T("AppSucceeded%d"), id);
    app_info.app_id.Format(_T("app_id_s_%d"), id);
    app_info.error_code = 0;
    app_info.extra_code1 = 0;
    app_info.completion_message = kSuccessAppCompletionMessage;
    app_info.installer_result_code = 0;
    app_info.is_canceled = false;
    apps_info_.push_back(app_info);
  }

  void AddFailedAppInfo(int id, bool make_error_info_unique) {
    AppCompletionInfo app_info;
    app_info.display_name.Format(_T("AppFailed%d"), id);
    app_info.app_id.Format(_T("app_id_f_%d"), id);
    app_info.error_code = 0x80070001;
    app_info.extra_code1 = 123;
    app_info.completion_message = kFailedAppCompletionMessage;
    app_info.installer_result_code = 111;
    app_info.is_canceled = false;
    if (make_error_info_unique) {
      app_info.error_code += id;
      app_info.completion_message.AppendFormat(_T(" AppName:%s."),
                                               app_info.display_name);
      app_info.extra_code1 += id;
      app_info.installer_result_code += id;
    }

    apps_info_.push_back(app_info);
  }

  void AddCanceledAppInfo(int id) {
    AppCompletionInfo app_info;
    app_info.display_name.Format(_T("AppCanceled%d"), id);
    app_info.app_id.Format(_T("app_id_f_%d"), id);
    app_info.error_code = GOOPDATE_E_CANCELLED;
    app_info.extra_code1 = 0;
    app_info.completion_message = kCanceledAppCompletionMessage;
    app_info.installer_result_code = 0;
    app_info.is_canceled = true;

    apps_info_.push_back(app_info);
  }

  static const TCHAR* kBundleDisplayName;
  static const TCHAR* kSuccessAppCompletionMessage;
  static const TCHAR* kFailedAppCompletionMessage;
  static const TCHAR* kCanceledAppCompletionMessage;
  std::vector<AppCompletionInfo> apps_info_;
};

const TCHAR* GetBundleCompletionMessageTest::kBundleDisplayName =
    _T("TestBundle");
const TCHAR* GetBundleCompletionMessageTest::kSuccessAppCompletionMessage =
    _T("App is installed successfully and is ready to use.");
const TCHAR* GetBundleCompletionMessageTest::kFailedAppCompletionMessage =
    _T("Failed to install the app.");
const TCHAR* GetBundleCompletionMessageTest::kCanceledAppCompletionMessage =
    _T("Installation is canceled by user.");

TEST_F(GetBundleCompletionMessageTest, SingleAppSucceeded) {
  AddSucceededAppInfo(1);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled
  // Bundle install succeeded, the completion message should be based on
  // IDS_BUNDLE_INSTALLED_SUCCESSFULLY.
  CString expected_message(_T("Installation complete."));
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, MultipleAppsSucceeded) {
  AddSucceededAppInfo(1);
  AddSucceededAppInfo(2);
  AddSucceededAppInfo(3);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled
  // Bundle install succeeded, the completion message should be based on
  // IDS_BUNDLE_INSTALLED_SUCCESSFULLY.
  CString expected_message(_T("Installation complete."));

  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, OneFailedAppWithSuccessApps) {
  AddSucceededAppInfo(1);
  AddFailedAppInfo(2, false);
  AddSucceededAppInfo(3);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled

  CString expected_message =
      _T("An application failed to install.\n")
      _T("<b>Succeeded:</b> AppSucceeded1, AppSucceeded3    ")
      _T("<b>Failed:</b> AppFailed2<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, MulitpleFailedAppsWithSuccessApps) {
  AddSucceededAppInfo(1);
  AddFailedAppInfo(2, false);
  AddSucceededAppInfo(3);
  AddFailedAppInfo(4, false);
  AddFailedAppInfo(5, false);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled

  CString expected_message =
      _T("Some applications failed to install.\n")
      _T("<b>Succeeded:</b> AppSucceeded1, AppSucceeded3    ")
      _T("<b>Failed:</b> AppFailed2, AppFailed4, AppFailed5<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, OneFailedAppOnly) {
  AddFailedAppInfo(1, false);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled

  EXPECT_STREQ(kFailedAppCompletionMessage, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AllAppsFailWithSameError) {
  AddFailedAppInfo(1, false);
  AddFailedAppInfo(2, false);
  AddFailedAppInfo(3, false);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled

  EXPECT_STREQ(kFailedAppCompletionMessage, bundle_message);
}


TEST_F(GetBundleCompletionMessageTest, AllAppsFailWithUniqueError) {
  AddFailedAppInfo(1, true);
  AddFailedAppInfo(2, true);
  AddFailedAppInfo(3, true);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       false);  // is_canceled

  CString expected_message = kFailedAppCompletionMessage;
  expected_message.AppendFormat(_T(" AppName:%s."), apps_info_[0].display_name);
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, BundleCanceled) {
  AddCanceledAppInfo(1);
  AddCanceledAppInfo(4);
  AddCanceledAppInfo(7);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  CString expected_message;
  EXPECT_TRUE(expected_message.LoadString(IDS_CANCELED));
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AppSucceededAfterCancel) {
  AddSucceededAppInfo(1);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  EXPECT_STREQ(_T("Installation completed before it could be canceled."),
               bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AppCanceledWithSuccesses) {
  AddSucceededAppInfo(1);
  AddSucceededAppInfo(2);
  AddSucceededAppInfo(3);
  AddCanceledAppInfo(4);
  AddCanceledAppInfo(5);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  CString expected_message =
      _T("Installation completed before it could be canceled.\n")
      _T("<b>Succeeded:</b> AppSucceeded1, AppSucceeded2, AppSucceeded3    ")
      _T("<b>Canceled:</b> AppCanceled4, AppCanceled5<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AppsCanceledWithSuccessesAndOneFailure) {
  AddSucceededAppInfo(1);
  AddSucceededAppInfo(2);
  AddFailedAppInfo(3, false);
  AddCanceledAppInfo(4);
  AddCanceledAppInfo(5);
  AddCanceledAppInfo(6);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled
  CString expected_message =
      _T("An application failed to install.\n")
      _T("<b>Succeeded:</b> AppSucceeded1, AppSucceeded2    ")
      _T("<b>Failed:</b> AppFailed3    ")
      _T("<b>Canceled:</b> AppCanceled4, AppCanceled5, AppCanceled6<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest,
       AppsCanceledWithSuccessesAndMultipleFailures) {
  AddSucceededAppInfo(1);
  AddSucceededAppInfo(2);
  AddFailedAppInfo(3, false);
  AddFailedAppInfo(4, false);
  AddCanceledAppInfo(5);
  AddCanceledAppInfo(6);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  CString expected_message =
      _T("Some applications failed to install.\n")
      _T("<b>Succeeded:</b> AppSucceeded1, AppSucceeded2    ")
      _T("<b>Failed:</b> AppFailed3, AppFailed4    ")
      _T("<b>Canceled:</b> AppCanceled5, AppCanceled6<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AppsCanceledWithOneFailure) {
  AddFailedAppInfo(1, false);
  AddCanceledAppInfo(2);
  AddCanceledAppInfo(3);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  CString expected_message =
      _T("An application failed to install.\n")
      _T("<b>Failed:</b> AppFailed1    ")
      _T("<b>Canceled:</b> AppCanceled2, AppCanceled3<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AppFailedAfterCancel) {
  AddFailedAppInfo(1, false);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  EXPECT_STREQ(kFailedAppCompletionMessage, bundle_message);
}

TEST_F(GetBundleCompletionMessageTest, AppsCanceledWithMultipleFailures) {
  AddFailedAppInfo(1, false);
  AddFailedAppInfo(2, false);
  AddFailedAppInfo(3, false);
  AddCanceledAppInfo(4);
  AddCanceledAppInfo(5);
  AddCanceledAppInfo(6);

  CString bundle_message = internal::GetBundleCompletionMessage(
                                       kBundleDisplayName,
                                       apps_info_,
                                       false,   // is_only_no_update
                                       true);   // is_canceled

  CString expected_message =
      _T("Some applications failed to install.\n")
      _T("<b>Failed:</b> AppFailed1, AppFailed2, AppFailed3    ")
      _T("<b>Canceled:</b> AppCanceled4, AppCanceled5, AppCanceled6<B> </B>");
  EXPECT_STREQ(expected_message, bundle_message);
}

}  // namespace omaha
