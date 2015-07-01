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

#include <windows.h>
#include "omaha/base/app_util.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/unit_test.h"
#include "omaha/ui/yes_no_dialog.h"

namespace omaha {

class YesNoDialogTest : public testing::Test {
 protected:
  YesNoDialogTest() {}

  static void SetUpTestCase() {
    CString resource_dir = app_util::GetModuleDirectory(NULL);
    EXPECT_HRESULT_SUCCEEDED(
        ResourceManager::Create(false, resource_dir, _T("en")));
  }

  static void TearDownTestCase() {
    ResourceManager::Delete();
  }

  static void SendCloseMessage(const YesNoDialog& yes_no_dialog) {
    EXPECT_TRUE(yes_no_dialog.IsWindow());
    ::SendMessage(yes_no_dialog.m_hWnd, WM_CLOSE, 0, 0);
  }
};

TEST_F(YesNoDialogTest, YesNoDialog) {
  CString title(_T("YesNoDialog"));
  CString text(_T("This is a test. Continue?"));

  CMessageLoop message_loop;
  YesNoDialog yes_no_dialog(&message_loop, NULL);
  EXPECT_SUCCEEDED(yes_no_dialog.Initialize(title, text));
  EXPECT_SUCCEEDED(yes_no_dialog.Show());
  YesNoDialogTest::SendCloseMessage(yes_no_dialog);
}

}   // namespace omaha

