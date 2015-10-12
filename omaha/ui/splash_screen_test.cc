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
#include "omaha/base/smart_handle.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/resource.h"
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

    omaha_dll.Attach(::LoadLibraryEx(
        kOmahaDllName, NULL, LOAD_LIBRARY_AS_DATAFILE));
    EXPECT_TRUE(omaha_dll.IsValid());
  }
  static void TearDownTestCase() {
    ResourceManager::Delete();
  }

  static void PostCloseMessage(const SplashScreen& splash_screen) {
    ASSERT_TRUE(splash_screen.IsWindow());
    ::PostMessage(splash_screen.m_hWnd, WM_CLOSE, 0, 0);
  }

  static void FormatMessage(DWORD message_id, SplashScreen* splash_screen) {
    ASSERT_TRUE(splash_screen);
    CString text;
    text.LoadString(message_id);
    if (String_Contains(text, _T("%1"))) {
      text.FormatMessage(message_id,
                         _T("Parameter One"),
                         _T("Parameter Two"),
                         _T("Parameter Three"));
    }
    splash_screen->text_ = text;
  }

  static AutoLibrary omaha_dll;
};

AutoLibrary SplashScreenTest::omaha_dll;

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

/*
// The following tests take too long to run. Commenting it out for the moment.
TEST_F(SplashScreenTest, RenderResourceStrings_en) {
  // The last message ID needs to be kept in sync with goopdateres.grh.
  for (DWORD message_id = IDS_FRIENDLY_COMPANY_NAME;
       message_id <= IDS_PROXY_PROMPT_MESSAGE;
       ++message_id) {
    SplashScreen splash_screen(NULL);
    FormatMessage(message_id, &splash_screen);
    splash_screen.Show();
    ::Sleep(200);
    splash_screen.Dismiss();
  }
}

TEST_F(SplashScreenTest, RenderResourceStrings) {
  const TCHAR * const langs[] = {
    _T("am"),
    _T("ar"),
    _T("bg"),
    _T("bn"),
    _T("ca"),
    _T("cs"),
    _T("da"),
    _T("de"),
    _T("el"),
    _T("en"),
    _T("en-GB"),
    _T("es"),
    _T("es-419"),
    _T("et"),
    _T("fa"),
    _T("fi"),
    _T("fil"),
    _T("fr"),
    _T("gu"),
    _T("hi"),
    _T("hr"),
    _T("hu"),
    _T("id"),
    _T("is"),
    _T("it"),
    _T("iw"),
    _T("ja"),
    _T("kn"),
    _T("ko"),
    _T("lt"),
    _T("lv"),
    _T("ml"),
    _T("mr"),
    _T("ms"),
    _T("nl"),
    _T("no"),
    _T("pl"),
    _T("pt-BR"),
    _T("pt-PT"),
    _T("ro"),
    _T("ru"),
    _T("sk"),
    _T("sl"),
    _T("sr"),
    _T("sv"),
    _T("sw"),
    _T("ta"),
    _T("te"),
    _T("th"),
    _T("tr"),
    _T("uk"),
    _T("ur"),
    _T("vi"),
    _T("zh-CN"),
    _T("zh-TW"),
  };

  const CString resource_dir =
      app_util::GetModuleDirectory(NULL) + _T("\\..\\staging");
  for (size_t lang_index = 0; lang_index < arraysize(langs); ++lang_index) {
    ResourceManager::Delete();

    ASSERT_SUCCEEDED(
        ResourceManager::Create(false, resource_dir, langs[lang_index]));

    // The last message ID needs to be kept in sync with goopdateres.grh.
    for (DWORD message_id = IDS_FRIENDLY_COMPANY_NAME;
         message_id <= IDS_PROXY_PROMPT_MESSAGE;
         ++message_id) {
      SplashScreen splash_screen(NULL);
      FormatMessage(message_id, &splash_screen);
      splash_screen.Show();
      ::Sleep(200);
      splash_screen.Dismiss();
    }
  }
}
*/

}   // namespace omaha

