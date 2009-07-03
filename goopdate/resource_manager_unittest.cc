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
#include "omaha/common/app_util.h"
#include "omaha/common/debug.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/string.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/resources/goopdateres/goopdate.grh"
#include "omaha/testing/unit_test.h"

namespace omaha {

class ResourceManagerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    path_ = ConcatenatePath(app_util::GetModuleDirectory(NULL),
                            _T("unittest_support\\Omaha_1.2.x_resources"));
    manager_.reset(new ResourceManager(false, path_));
  }

  virtual void TearDown() {
  }

  void SetMachine(bool is_machine) {
    manager_->is_machine_ = is_machine;
  }

  void SetResourceDir(const CString& resource_dir) {
    manager_->resource_dir_ = resource_dir;
  }

  void GetDistinctLanguageMapFromTranslationTable(
      std::map<CString, bool>* languages) {
    manager_->GetDistinctLanguageMapFromTranslationTable(languages);
  }

  CString GetLang(LANGID langid) {
    return manager_->GetLanguageForLangID(langid);
  }

  static CString GetResourceDllName(const CString& language) {
    return ResourceManager::GetResourceDllName(language);
  }

  scoped_ptr<ResourceManager> manager_;
  CString path_;
};

TEST_F(ResourceManagerTest, GetResourceDllName) {
  const CString kLang(_T("en"));
  CString ret = GetResourceDllName(kLang);

  CString expected_filename;
  expected_filename.Format(kGoopdateResourceDllName, kLang);
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
  SetResourceDir(_T("non_existing\\abcddir"));

  CString lang(_T("en"));
  EXPECT_HRESULT_FAILED(manager_->LoadResourceDll(lang));
  EXPECT_FALSE(manager_->resource_dll());
  EXPECT_STREQ(manager_->language(), _T(""));
}

TEST_F(ResourceManagerTest, LoadResourceDllCmdLine) {
  SetMachine(false);

  CString lang(_T("ca"));
  EXPECT_HRESULT_SUCCEEDED(manager_->LoadResourceDll(lang));
  EXPECT_TRUE(manager_->resource_dll());
  EXPECT_STREQ(manager_->language(), lang);

  CString expected_filename;
  expected_filename.Format(kGoopdateResourceDllName, lang);
  CString expected_path = ConcatenatePath(path_, expected_filename);
  EXPECT_STREQ(expected_path, manager_->resource_dll_filepath());
}

TEST_F(ResourceManagerTest, LoadResourceDllCmdLineMachine) {
  SetMachine(true);

  CString lang(_T("ca"));
  EXPECT_HRESULT_SUCCEEDED(manager_->LoadResourceDll(lang));
  EXPECT_TRUE(manager_->resource_dll());
  EXPECT_STREQ(manager_->language(), lang);

  CString expected_filename;
  expected_filename.Format(kGoopdateResourceDllName, lang);
  CString expected_path = ConcatenatePath(path_, expected_filename);
  EXPECT_STREQ(expected_path, manager_->resource_dll_filepath());
}

TEST_F(ResourceManagerTest, GetLanguageForLangID_NoLangID) {
  EXPECT_STREQ(_T("en"), ResourceManager::GetLanguageForLangID(0));
}

TEST_F(ResourceManagerTest, GetLanguageForLangID_SupportedIds) {
  EXPECT_STREQ(_T("ar"), GetLang(MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("bg"), GetLang(MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("ca"), GetLang(MAKELANGID(LANG_CATALAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("cs"), GetLang(MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("da"), GetLang(MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("de"), GetLang(MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("el"), GetLang(MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("en"), GetLang(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("en-GB"), GetLang(MAKELANGID(LANG_ENGLISH,
                                               SUBLANG_ENGLISH_UK)));
  EXPECT_STREQ(_T("es"), GetLang(MAKELANGID(LANG_SPANISH,
                                            SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("es"), GetLang(MAKELANGID(LANG_SPANISH,
                                            SUBLANG_SPANISH)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_MEXICAN)));
  EXPECT_STREQ(_T("es"), GetLang(MAKELANGID(LANG_SPANISH,
                                            SUBLANG_SPANISH_MODERN)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_GUATEMALA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_COSTA_RICA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_PANAMA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(
      LANG_SPANISH,
      SUBLANG_SPANISH_DOMINICAN_REPUBLIC)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_VENEZUELA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_COLOMBIA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_PERU)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_ARGENTINA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_ECUADOR)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_CHILE)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_URUGUAY)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_PARAGUAY)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_BOLIVIA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_EL_SALVADOR)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_HONDURAS)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_NICARAGUA)));
  EXPECT_STREQ(_T("es-419"), GetLang(MAKELANGID(LANG_SPANISH,
                                                SUBLANG_SPANISH_PUERTO_RICO)));
  EXPECT_STREQ(_T("et"), GetLang(MAKELANGID(LANG_ESTONIAN,
                                            SUBLANG_ESTONIAN_ESTONIA)));
  EXPECT_STREQ(_T("fi"), GetLang(MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("fil"), GetLang(MAKELANGID(LANG_FILIPINO, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("fr"), GetLang(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("hi"), GetLang(MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("hr"), GetLang(MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("hr"), GetLang(MAKELANGID(LANG_SERBIAN,
                                            SUBLANG_SERBIAN_CROATIA)));
  EXPECT_STREQ(_T("hu"), GetLang(MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("id"), GetLang(MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("it"), GetLang(MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("iw"), GetLang(MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("ja"), GetLang(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("ko"), GetLang(MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("lt"), GetLang(MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("lv"), GetLang(MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("nl"), GetLang(MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("no"), GetLang(MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("pl"), GetLang(MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("pt-BR"), GetLang(MAKELANGID(LANG_PORTUGUESE,
                                               SUBLANG_PORTUGUESE_BRAZILIAN)));
  EXPECT_STREQ(_T("pt-PT"), GetLang(MAKELANGID(LANG_PORTUGUESE,
                                               SUBLANG_PORTUGUESE)));
  EXPECT_STREQ(_T("ro"), GetLang(MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("ru"), GetLang(MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("sk"), GetLang(MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("sl"), GetLang(MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("sr"), GetLang(
      MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_CYRILLIC)));
  EXPECT_STREQ(_T("sr"), GetLang(
      MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_LATIN)));
  EXPECT_STREQ(_T("sr"), GetLang(MAKELANGID(LANG_SERBIAN,
                                            SUBLANG_SERBIAN_CYRILLIC)));
  EXPECT_STREQ(_T("sr"), GetLang(MAKELANGID(LANG_SERBIAN,
                                            SUBLANG_SERBIAN_LATIN)));
  EXPECT_STREQ(_T("sv"), GetLang(MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("th"), GetLang(MAKELANGID(LANG_THAI, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("tr"), GetLang(MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("uk"), GetLang(MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("vi"), GetLang(MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("zh-HK"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_HONGKONG)));
  EXPECT_STREQ(_T("zh-CN"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_MACAU)));
  EXPECT_STREQ(_T("zh-CN"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_SIMPLIFIED)));
  EXPECT_STREQ(_T("zh-CN"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_SINGAPORE)));
  EXPECT_STREQ(_T("zh-TW"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_TRADITIONAL)));
}

// Unsupported languages and sublanguages fall back to "en".
TEST_F(ResourceManagerTest, GetLanguageForLangID_UnsupportedSubLang) {
  // LANG_NEUTRAL is unsupported.
  EXPECT_STREQ(_T("en"), GetLang(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)));
  // LANG_AFRIKAANS is unsupported.
  EXPECT_STREQ(_T("en"), GetLang(MAKELANGID(LANG_AFRIKAANS, SUBLANG_NEUTRAL)));
  // SUBLANG_NEUTRAL is unsupported.
  EXPECT_STREQ(_T("en"), GetLang(MAKELANGID(LANG_SPANISH, SUBLANG_NEUTRAL)));
  // SUBLANG_SYS_DEFAULT is unsupported. It happens to be 2, which is not
  // supported for Hungarian but is for English, Spanish, and others/
  EXPECT_STREQ(_T("en"),
               GetLang(MAKELANGID(LANG_HUNGARIAN, SUBLANG_SYS_DEFAULT)));
  EXPECT_STREQ(_T("es-419"),
               GetLang(MAKELANGID(LANG_SPANISH, SUBLANG_SYS_DEFAULT)));
  // 0x3f is an invalid sublang. There is a "es" file.
  EXPECT_STREQ(_T("en"), GetLang(MAKELANGID(LANG_SPANISH, 0x3f)));
  // 0x3f is an invalid sublang. There is not a "zh" file.
  EXPECT_STREQ(_T("en"), GetLang(MAKELANGID(LANG_CHINESE, 0x3f)));
}

TEST_F(ResourceManagerTest, TestCountLanguagesInTranslationTable) {
  std::map<CString, bool> languages;
  GetDistinctLanguageMapFromTranslationTable(&languages);
  // Number of language DLLs + zh-HK special case.
  EXPECT_EQ(54 + 1, languages.size());
}

TEST_F(ResourceManagerTest, TestAppropriateLanguagesInTranslationTable) {
  std::map<CString, bool> languages;
  GetDistinctLanguageMapFromTranslationTable(&languages);

  EXPECT_TRUE(languages.find(_T("ar")) != languages.end());
  EXPECT_TRUE(languages.find(_T("bg")) != languages.end());
  EXPECT_TRUE(languages.find(_T("bn")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ca")) != languages.end());
  EXPECT_TRUE(languages.find(_T("cs")) != languages.end());
  EXPECT_TRUE(languages.find(_T("da")) != languages.end());
  EXPECT_TRUE(languages.find(_T("de")) != languages.end());
  EXPECT_TRUE(languages.find(_T("el")) != languages.end());
  EXPECT_TRUE(languages.find(_T("en-GB")) != languages.end());
  EXPECT_TRUE(languages.find(_T("en")) != languages.end());
  EXPECT_TRUE(languages.find(_T("es-419")) != languages.end());
  EXPECT_TRUE(languages.find(_T("es")) != languages.end());
  EXPECT_TRUE(languages.find(_T("et")) != languages.end());
  EXPECT_TRUE(languages.find(_T("fa")) != languages.end());
  EXPECT_TRUE(languages.find(_T("fi")) != languages.end());
  EXPECT_TRUE(languages.find(_T("fil")) != languages.end());
  EXPECT_TRUE(languages.find(_T("fr")) != languages.end());
  EXPECT_TRUE(languages.find(_T("gu")) != languages.end());
  EXPECT_TRUE(languages.find(_T("hi")) != languages.end());
  EXPECT_TRUE(languages.find(_T("hr")) != languages.end());
  EXPECT_TRUE(languages.find(_T("hu")) != languages.end());
  EXPECT_TRUE(languages.find(_T("id")) != languages.end());
  EXPECT_TRUE(languages.find(_T("is")) != languages.end());
  EXPECT_TRUE(languages.find(_T("it")) != languages.end());
  EXPECT_TRUE(languages.find(_T("iw")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ja")) != languages.end());
  EXPECT_TRUE(languages.find(_T("kn")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ko")) != languages.end());
  EXPECT_TRUE(languages.find(_T("lt")) != languages.end());
  EXPECT_TRUE(languages.find(_T("lv")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ml")) != languages.end());
  EXPECT_TRUE(languages.find(_T("mr")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ms")) != languages.end());
  EXPECT_TRUE(languages.find(_T("nl")) != languages.end());
  EXPECT_TRUE(languages.find(_T("no")) != languages.end());
  EXPECT_TRUE(languages.find(_T("or")) != languages.end());
  EXPECT_TRUE(languages.find(_T("pl")) != languages.end());
  EXPECT_TRUE(languages.find(_T("pt-BR")) != languages.end());
  EXPECT_TRUE(languages.find(_T("pt-PT")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ro")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ru")) != languages.end());
  EXPECT_TRUE(languages.find(_T("sk")) != languages.end());
  EXPECT_TRUE(languages.find(_T("sl")) != languages.end());
  EXPECT_TRUE(languages.find(_T("sr")) != languages.end());
  EXPECT_TRUE(languages.find(_T("sv")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ta")) != languages.end());
  EXPECT_TRUE(languages.find(_T("te")) != languages.end());
  EXPECT_TRUE(languages.find(_T("th")) != languages.end());
  EXPECT_TRUE(languages.find(_T("tr")) != languages.end());
  EXPECT_TRUE(languages.find(_T("uk")) != languages.end());
  EXPECT_TRUE(languages.find(_T("ur")) != languages.end());
  EXPECT_TRUE(languages.find(_T("vi")) != languages.end());
  EXPECT_TRUE(languages.find(_T("zh-CN")) != languages.end());
  EXPECT_TRUE(languages.find(_T("zh-HK")) != languages.end());
  EXPECT_TRUE(languages.find(_T("zh-TW")) != languages.end());
}

TEST_F(ResourceManagerTest, TestCountLanguageDlls) {
  std::vector<CString> filenames;
  ResourceManager::GetSupportedLanguageDllNames(&filenames);
  EXPECT_EQ(54, filenames.size());
}

TEST_F(ResourceManagerTest, TestAppropriateLanguageDlls) {
  std::vector<CString> filenames;
  ResourceManager::GetSupportedLanguageDllNames(&filenames);

  std::vector<CString>::iterator iter = filenames.begin();

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
  EXPECT_STREQ(_T("goopdateres_or.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_pl.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_pt-BR.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_pt-PT.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ro.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_ru.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sk.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sl.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sr.dll"), *iter++);
  EXPECT_STREQ(_T("goopdateres_sv.dll"), *iter++);
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

TEST_F(ResourceManagerTest, RussianResourcesValid) {
  SetResourceDir(app_util::GetModuleDirectory(NULL));

  CString lang(_T("ru"));
  EXPECT_HRESULT_SUCCEEDED(manager_->LoadResourceDll(lang));
  EXPECT_TRUE(manager_->resource_dll());
  EXPECT_STREQ(lang, manager_->language());

  CString install_success(FormatResourceMessage(
      IDS_APPLICATION_INSTALLED_SUCCESSFULLY, _T("Gears")));
  EXPECT_STREQ("Благодарим вас за установку Gears.",
               WideToUtf8(install_success));

  CString install_fail(FormatResourceMessage(IDS_INSTALLER_FAILED_WITH_MESSAGE,
                            _T("12345"), _T("Action failed.")));
  EXPECT_STREQ("Ошибка установщика 12345: Action failed.",
               WideToUtf8(install_fail));
}

}  // namespace omaha.

