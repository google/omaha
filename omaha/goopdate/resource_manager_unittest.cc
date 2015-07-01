// Copyright 2007-2009 Google Inc.
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
// ResourceManager unit tests.

#include <map>
#include <vector>
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/string.h"
#include "omaha/common/lang.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/resources/goopdateres/goopdate.grh"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const int kNumberOfLanguageDlls = 55;

}  // namespace

class ResourceManagerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    path_ = app_util::GetModuleDirectory(NULL);
    EXPECT_HRESULT_SUCCEEDED(
        ResourceManager::CreateForDefaultLanguage(false, path_));
  }

  virtual void TearDown() {
    ResourceManager::Delete();
  }

  void SetMachine(bool is_machine) {
    ResourceManager::instance_->is_machine_ = is_machine;
  }

  void SetResourceDir(const CString& resource_dir) {
    ResourceManager::instance_->resource_dir_ = resource_dir;
  }

  CString GetResourceDir() const {
    return ResourceManager::instance_->resource_dir_;
  }

  CString GetLang(LANGID langid) {
    return lang::GetLanguageForLangID(langid);
  }

  void VerifyLoadingResourceDll(const CString& lang, bool is_success) {
    ResourceManager::ResourceDllInfo dll_info;

    HRESULT hr = ResourceManager::Instance().GetResourceDllInfo(lang,
                                                                &dll_info);
    if (is_success) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      EXPECT_TRUE(dll_info.dll_handle != NULL);
      EXPECT_STREQ(lang, dll_info.language);

      CString expected_file_name;
      expected_file_name.Format(kOmahaResourceDllNameFormat, lang);
      CString expected_path = ConcatenatePath(path_, expected_file_name);
      EXPECT_STREQ(expected_path, dll_info.file_path);
    } else {
      EXPECT_HRESULT_FAILED(hr);
      EXPECT_EQ(NULL, dll_info.dll_handle);
      EXPECT_STREQ(_T(""), dll_info.language);
    }
  }

  static CString GetResourceDllName(const CString& language) {
    return ResourceManager::GetResourceDllName(language);
  }

  CString path_;
};

// Disables the default resources used for unit testing and restores them after
// the test.
// For some reason, the _AtlBaseModule.SetResourceInstance() call in
// ResourceManager does not replace the existing resources, so they must be
// unloaded first.
class ResourceManagerResourcesProtectedTest : public ResourceManagerTest {
 protected:
  // Assumes that the default resources are the first loaded at index 0.
  virtual void SetUp() {
    ResourceManagerTest::SetUp();

     default_resources_ = _AtlBaseModule.GetHInstanceAt(0);
    _AtlBaseModule.RemoveResourceInstance(default_resources_);
  }

  virtual void TearDown() {
    _AtlBaseModule.AddResourceInstance(default_resources_);

    ResourceManagerTest::TearDown();
  }

 private:
  HINSTANCE default_resources_;
};

TEST_F(ResourceManagerTest, GetResourceDllName) {
  const CString kLang(_T("en"));
  CString ret = GetResourceDllName(kLang);

  CString expected_filename;
  expected_filename.Format(kOmahaResourceDllNameFormat, kLang);
  EXPECT_STREQ(expected_filename, ret);
}

TEST_F(ResourceManagerTest, GetResourceDllName_SpecialCases) {
  // zh-HK -> zh-TW
  EXPECT_STREQ(_T("goopdateres_zh-TW.dll"), GetResourceDllName(_T("zh-TW")));
  EXPECT_STREQ(_T("goopdateres_zh-TW.dll"), GetResourceDllName(_T("zh-HK")));

  // he -> iw
  EXPECT_STREQ(_T("goopdateres_iw.dll"), GetResourceDllName(_T("iw")));
  EXPECT_STREQ(_T("goopdateres_iw.dll"), GetResourceDllName(_T("he")));
}

TEST_F(ResourceManagerTest, LoadResourceFail) {
  SetMachine(false);

  CString original_resoruce_dir = GetResourceDir();
  SetResourceDir(_T("non_existing\\abcddir"));

  // Loading resource from a non-existing directory should fail. The language
  // being loaded here should not be loaded previously. Otherwise the resource
  // manager will return the cached value instead of doing actual load.
  VerifyLoadingResourceDll(_T("ca"), false);

  SetResourceDir(original_resoruce_dir);
}

TEST_F(ResourceManagerTest, LoadResourceDllCmdLine) {
  SetMachine(false);

  CString lang = _T("ca");
  VerifyLoadingResourceDll(lang, true);
}

TEST_F(ResourceManagerTest, LoadResourceDllCmdLineMachine) {
  SetMachine(true);

  CString lang = _T("ca");
  VerifyLoadingResourceDll(lang, true);
}

TEST_F(ResourceManagerTest, TestCountLanguageDlls) {
  std::vector<CString> filenames;
  ResourceManager::GetSupportedLanguageDllNames(&filenames);
  EXPECT_EQ(kNumberOfLanguageDlls, filenames.size());
}

TEST_F(ResourceManagerTest, TestAppropriateLanguageDlls) {
  std::vector<CString> filenames;
  ResourceManager::GetSupportedLanguageDllNames(&filenames);

  std::vector<CString>::iterator iter = filenames.begin();

  EXPECT_STREQ(_T("goopdateres_am.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ar.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_bg.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_bn.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ca.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_cs.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_da.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_de.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_el.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_en.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_en-GB.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_es.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_es-419.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_et.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_fa.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_fi.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_fil.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_fr.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_gu.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_hi.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_hr.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_hu.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_id.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_is.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_it.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_iw.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ja.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_kn.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ko.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_lt.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_lv.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ml.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_mr.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ms.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_nl.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_no.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_pl.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_pt-BR.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_pt-PT.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ro.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ru.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sk.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sl.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sr.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sv.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sw.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ta.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_te.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_th.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_tr.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_uk.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ur.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_vi.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_zh-CN.dll"), *iter++);
  // goopdateres_zh-HK.dll not present
  EXPECT_STREQ(_T("goopdateres_zh-TW.dll"), *iter++);
}

TEST_F(ResourceManagerResourcesProtectedTest, RussianResourcesValid) {
  ResourceManager::Delete();

  CString lang(_T("ru"));
  EXPECT_HRESULT_SUCCEEDED(ResourceManager::Create(false, path_, lang));

  CString display_name(FormatResourceMessage(
      IDS_DEFAULT_APP_DISPLAY_NAME, _T("Google Gears")));

  EXPECT_STREQ("Приложение Google Gears", WideToUtf8(display_name));

  CString install_fail(FormatResourceMessage(IDS_INSTALLER_FAILED_WITH_MESSAGE,
                                             _T("12345"),
                                             _T("Action failed.")));

  EXPECT_STREQ("Ошибка установщика 12345: Action failed.",
               WideToUtf8(install_fail));
}

}  // namespace omaha
