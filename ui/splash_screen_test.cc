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

// This unit test is driving the UI through its states so that we can
// visually inspect all the controls are there in their right state and
// position. To go from state to state, simply close the window on the screen.
//
// The unit test is useful for debugging UI states so different tests are
// enabled/disabled at compile time, depending what needs to be tested.

#include <windows.h>
#include "omaha/base/app_util.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/unit_test.h"
#include "omaha/ui/splash_screen.h"

namespace omaha {

class SplashScreenTest : public testing::Test {
 protected:
  SplashScreenTest() {}

  static void SetUpTestCase() {
    CString resource_dir = app_util::GetModuleDirectory(NULL);
    EXPECT_HRESULT_SUCCEEDED(
        ResourceManager::Create(false, resource_dir, _T("en")));
  }
  static void TearDownTestCase() {
    ResourceManager::Delete();
  }

  static void PostCloseMessage(const SplashScreen& splash_screen) {
    ASSERT_TRUE(splash_screen.IsWindow());
    ::PostMessage(splash_screen.m_hWnd, WM_CLOSE, 0, 0);
  }
};

TEST_F(SplashScreenTest, SplashScreen) {
  SplashScreen splash_screen(NULL);
  splash_screen.Show();
  ::Sleep(200);
  splash_screen.Dismiss();
}

TEST_F(SplashScreenTest, SplashScreen_QuickRelease) {
  SplashScreen splash_screen(_T("Sample bundle"));
  splash_screen.Show();
  splash_screen.Dismiss();
}

TEST_F(SplashScreenTest, SplashScreen_NoShow) {
  SplashScreen splash_screen(_T("You Should Not See This"));
}

TEST_F(SplashScreenTest, SplashScreen_PostCloseMessage) {
  SplashScreen splash_screen(NULL);
  splash_screen.Show();
  ::Sleep(100);
  SplashScreenTest::PostCloseMessage(splash_screen);
  ::Sleep(100);
  splash_screen.Dismiss();
}

}   // namespace omaha

