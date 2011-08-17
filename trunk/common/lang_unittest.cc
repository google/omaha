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
//
#include <map>
#include <vector>
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/string.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/lang.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const int kNumberOfLanguages = 56;

}  // namespace

class LanguageManagerTest : public testing::Test {
 protected:
  CString GetLang(LANGID langid) {
    return lang::GetLanguageForLangID(langid);
  }
};

TEST_F(LanguageManagerTest, GetLanguageForLangID_NoLangID) {
  EXPECT_STREQ(_T("en"), lang::GetLanguageForLangID(0xFFFF));
}

TEST_F(LanguageManagerTest, IsLanguageSupported) {
  EXPECT_TRUE(lang::IsLanguageSupported(_T("en")));

  EXPECT_FALSE(lang::IsLanguageSupported(_T("")));
  EXPECT_FALSE(lang::IsLanguageSupported(_T("non-existing lang")));
  EXPECT_FALSE(lang::IsLanguageSupported(_T("en-US")));
}

TEST_F(LanguageManagerTest, GetLanguageForLangID_SupportedIds) {
  EXPECT_STREQ(_T("am"), GetLang(MAKELANGID(LANG_AMHARIC, SUBLANG_DEFAULT)));
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
  EXPECT_STREQ(_T("sw"), GetLang(MAKELANGID(LANG_SWAHILI, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("th"), GetLang(MAKELANGID(LANG_THAI, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("tr"), GetLang(MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("uk"), GetLang(MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("vi"), GetLang(MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT)));
  EXPECT_STREQ(_T("zh-HK"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_HONGKONG)));
  EXPECT_STREQ(_T("zh-TW"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_MACAU)));
  EXPECT_STREQ(_T("zh-CN"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_SIMPLIFIED)));
  EXPECT_STREQ(_T("zh-CN"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_SINGAPORE)));
  EXPECT_STREQ(_T("zh-TW"), GetLang(MAKELANGID(LANG_CHINESE,
                                               SUBLANG_CHINESE_TRADITIONAL)));
}

// Unsupported languages and sublanguages fall back to "en".
TEST_F(LanguageManagerTest, GetLanguageForLangID_UnsupportedSubLang) {
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

TEST_F(LanguageManagerTest, TestCountLanguagesInTranslationTable) {
  std::vector<CString> languages;
  lang::GetSupportedLanguages(&languages);
  EXPECT_EQ(kNumberOfLanguages, languages.size());
}

TEST_F(LanguageManagerTest, TestAppropriateLanguagesInTranslationTable) {
  EXPECT_TRUE(lang::IsLanguageSupported(_T("am")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ar")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("bg")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("bn")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ca")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("cs")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("da")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("de")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("el")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("en-GB")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("en")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("es-419")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("es")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("et")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("fa")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("fi")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("fil")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("fr")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("gu")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("hi")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("hr")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("hu")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("id")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("is")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("it")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("iw")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ja")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("kn")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ko")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("lt")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("lv")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ml")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("mr")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ms")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("nl")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("no")));
  EXPECT_FALSE(lang::IsLanguageSupported(_T("or")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("pl")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("pt-BR")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("pt-PT")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ro")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ru")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("sk")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("sl")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("sr")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("sv")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("sw")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ta")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("te")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("th")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("tr")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("uk")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("ur")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("vi")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("zh-CN")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("zh-HK")));
  EXPECT_TRUE(lang::IsLanguageSupported(_T("zh-TW")));
}

}  // namespace omaha
