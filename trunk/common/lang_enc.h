// Copyright 2004-2009 Google Inc.
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
// This file is for i18n. It contains two enums, namely Language and
// Encoding, where Language is the linguistic convention, and Encoding
// contains information on both language encoding and character set.
//
// The language and encoding are both based on Teragram's conventions,
// except for some common ISO-8859 encodings that are not detected by
// Teragram but might be in the future.
//
// This file also includes functions that do mappings among
// Language/Encoding enums, language/encoding string names (typically
// the output from Language Encoding identifier), and language codes
// (iso 639), and two-letter country codes (iso 3166)
//
// NOTE: Both Language and Encoding enums should always start from
// zero value. This assumption has been made and used.

#ifndef  OMAHA_COMMON_LANG_ENC_H_
#define  OMAHA_COMMON_LANG_ENC_H_

#include "omaha/common/commontypes.h"

// some of the popular encoding aliases
#define LATIN1     ISO_8859_1
#define LATIN2     ISO_8859_2
#define LATIN3     ISO_8859_3
#define LATIN4     ISO_8859_4
#define CYRILLIC   ISO_8859_5
#define ARABIC_ENCODING  ISO_8859_6     // avoiding the same name as language
#define GREEK_ENCODING   ISO_8859_7     // avoiding the same name as language
#define HEBREW_ENCODING  ISO_8859_8     // avoiding the same name as language
#define LATIN5     ISO_8859_9
#define LATIN6     ISO_8859_10
#define KOREAN_HANGUL  KOREAN_EUC_KR

// NOTE: Only add new languages to the end of this list (but before
// NUM_LANGUAGES).
enum Language {
  ENGLISH = 0,  /* 0 */
  DANISH,       /* 1 */
  DUTCH,        /* 2 */
  FINNISH,      /* 3 */
  FRENCH,       /* 4 */
  GERMAN,       /* 5 */
  HEBREW,       /* 6 */
  ITALIAN,      /* 7 */
  JAPANESE,     /* 8 */
  KOREAN,       /* 9 */
  NORWEGIAN,    /* 10 */
  POLISH,       /* 11 */
  PORTUGUESE,   /* 12 */
  RUSSIAN,      /* 13 */
  SPANISH,      /* 14 */
  SWEDISH,      /* 15 */
  CHINESE,      /* 16 */
  CZECH,        /* 17 */
  GREEK,        /* 18 */
  ICELANDIC,    /* 19 */
  LATVIAN,      /* 20 */
  LITHUANIAN,   /* 21 */
  ROMANIAN,     /* 22 */
  HUNGARIAN,    /* 23 */
  ESTONIAN,     /* 24 */
  TG_UNKNOWN_LANGUAGE,  /* 25 */
  UNKNOWN_LANGUAGE,     /* 26 */
  BULGARIAN,    /* 27 */
  CROATIAN,     /* 28 */
  SERBIAN,      /* 29 */
  IRISH,        /* 30 */
  GALICIAN,     /* 31 */
  TAGALOG,      /* 32 */
  TURKISH,      /* 33 */
  UKRAINIAN,    /* 34 */
  HINDI,        /* 35 */
  MACEDONIAN,   /* 36 */
  BENGALI,      /* 37 */
  INDONESIAN,   /* 38 */
  LATIN,        /* 39 */
  MALAY,        /* 40 */
  MALAYALAM,    /* 41 */
  WELSH,        /* 42 */
  NEPALI,       /* 43 */
  TELUGU,       /* 44 */
  ALBANIAN,     /* 45 */
  TAMIL,        /* 46 */
  BELARUSIAN,   /* 47 */
  JAVANESE,     /* 48 */
  OCCITAN,      /* 49 */
  URDU,         /* 50 */
  BIHARI,       /* 51 */
  GUJARATI,     /* 52 */
  THAI,         /* 53 */
  ARABIC,       /* 54 */
  CATALAN,      /* 55 */
  ESPERANTO,    /* 56 */
  BASQUE,       /* 57 */
  INTERLINGUA,  /* 58 */
  KANNADA,      /* 59 */
  PUNJABI,      /* 60 */
  SCOTS_GAELIC, /* 61 */
  SWAHILI,      /* 62 */
  SLOVENIAN,    /* 63 */
  MARATHI,      /* 64 */
  MALTESE,      /* 65 */
  VIETNAMESE,   /* 66 */
  FRISIAN,      /* 67 */
  SLOVAK,       /* 68 */
  CHINESE_T,    /* 69 */      // This is added to solve the problem of
                              // distinguishing Traditional and Simplified
                              // Chinese when the encoding is UTF8.
  FAROESE,      /* 70 */
  SUNDANESE,    /* 71 */
  UZBEK,        /* 72 */
  AMHARIC,      /* 73 */
  AZERBAIJANI,  /* 74 */
  GEORGIAN,     /* 75 */
  TIGRINYA,     /* 76 */
  PERSIAN,      /* 77 */
  BOSNIAN,      /* 78 */
  SINHALESE,    /* 79 */
  NORWEGIAN_N,  /* 80 */
  PORTUGUESE_P, /* 81 */
  PORTUGUESE_B, /* 82 */
  XHOSA,        /* 83 */
  ZULU,         /* 84 */
  GUARANI,      /* 85 */
  SESOTHO,      /* 86 */
  TURKMEN,      /* 87 */
  KYRGYZ,       /* 88 */
  BRETON,       /* 89 */
  TWI,          /* 90 */
  YIDDISH,      /* 91 */
  ORIYA,        /* 92 */
  SERBO_CROATIAN,       /* 93 */
  SOMALI,       /* 94 */
  UIGHUR,       /* 95 */
  KURDISH,      /* 96 */
  MONGOLIAN,    /* 97 */
  ARMENIAN,     /* 98 */
  LAOTHIAN,     /* 99 */
  SINDHI,       /* 100! */
  RHAETO_ROMANCE,  /* 101 */
  CHINESE_JAPANESE_KOREAN,  /* 103 */  // Not really a language
  PSEUDOTRANSLATION,  /* 104 */  // Not really a language
  NUM_LANGUAGES,              // Always keep this at the end. It is not a
                              // valid Language enum, it is only used to
                              // indicate the total number of Languages.
};


// Language codes for those languages we support, used to map to IDs from
// the Language enumeration.  We could have used the Rfc1766ToLcid from the
// Win32 system's mlang.dll to map these to LCIDs, but a) we don't want to
// have to load mlang.dll and b) we are using our own language IDs.
const TCHAR* const kLangCodeChinesePrc = _T("zh_cn");
const TCHAR* const kLangCodeChineseTaiwan = _T("zh_tw");
const TCHAR* const kLangCodeCjk = _T("cjk");
const TCHAR* const kLangCodeDutch = _T("nl");
const TCHAR* const kLangCodeEnglish = _T("en");
const TCHAR* const kLangCodeFrench = _T("fr");
const TCHAR* const kLangCodeGerman = _T("de");
const TCHAR* const kLangCodeItalian = _T("it");
const TCHAR* const kLangCodeJapanese = _T("ja");
const TCHAR* const kLangCodeKorean = _T("ko");
const TCHAR* const kLangCodePseudo = _T("x");
const TCHAR* const kLangCodeSpanish = _T("es");


// Maps language codes to languages.  Terminated by a { NULL, UNKNOWN_LANGUAGE }
// item.
struct CodeToLanguage {
  const TCHAR* code;
  Language language;
};

SELECTANY CodeToLanguage codes_to_languages[] = {
  { kLangCodeChinesePrc, CHINESE },
  { kLangCodeChineseTaiwan, CHINESE_T },
  { kLangCodeCjk, CHINESE_JAPANESE_KOREAN },
  { kLangCodeDutch, DUTCH },
  { kLangCodeEnglish, ENGLISH },
  { kLangCodeFrench, FRENCH },
  { kLangCodeGerman, GERMAN },
  { kLangCodeItalian, ITALIAN },
  { kLangCodeJapanese, JAPANESE },
  { kLangCodeKorean, KOREAN },
  { kLangCodePseudo, PSEUDOTRANSLATION },
  { kLangCodeSpanish, SPANISH },
  { NULL, UNKNOWN_LANGUAGE }
};



// Macro to wrap the notion of "unknown language".
#define IS_LANGUAGE_UNKNOWN(l)  \
  ((l) == TG_UNKNOWN_LANGUAGE || (l) == UNKNOWN_LANGUAGE)

// NOTE: Only add new encodings to the end of this list (but before
// NUM_ENCODINGS).
// NOTE: If you add an encoding here, you must also modify basistech_encoding()
// and google2/com/google/i18n/Encoding.java
enum Encoding {
  ISO_8859_1 = 0,       // 0: Teragram ASCII
  ISO_8859_2,           // 1: Teragram Latin2
  ISO_8859_3,           // 2: in BasisTech but not in Teragram
  ISO_8859_4,           // 3: Teragram Latin4
  ISO_8859_5,           // 4: Teragram ISO-8859-5
  ISO_8859_6,           // 5: Teragram Arabic
  ISO_8859_7,           // 6: Teragram Greek
  ISO_8859_8,           // 7: Teragram Hebrew
  ISO_8859_9,           // 8: in BasisTech but not in Teragram
  ISO_8859_10,          // 9: in BasisTech but not in Teragram
  JAPANESE_EUC_JP,      // 10: Teragram EUC_JP
  JAPANESE_SHIFT_JIS,   // 11: Teragram SJS
  JAPANESE_JIS,         // 12: Teragram JIS
  CHINESE_BIG5,         // 13: Teragram BIG5
  CHINESE_GB,           // 14: Teragram GB
  CHINESE_EUC_CN,       // 15: Teragram EUC-CN
  KOREAN_EUC_KR,        // 16: Teragram KSC
  UNICODE_ENCODING,     // 17: Teragram Unicode, changed to UNICODE_ENCODING
                        //     from UNICODE, which is predefined by WINDOW
  CHINESE_EUC_DEC,      // 18: Teragram EUC
  CHINESE_CNS,          // 19: Teragram CNS
  CHINESE_BIG5_CP950,   // 20: Teragram BIG5_CP950
  JAPANESE_CP932,       // 21: Teragram CP932
  UTF8,                 // 22
  UNKNOWN_ENCODING,     // 23
  ASCII_7BIT,           // 24: ISO_8859_1 with all characters <= 127.
                        //     Should be present only in the crawler
                        //     and in the repository,
                        //     *never* as a result of Document::encoding().
  RUSSIAN_KOI8_R,       // 25: Teragram KOI8R
  RUSSIAN_CP1251,       // 26: Teragram CP1251

  //----------------------------------------------------------
  // These are _not_ output from teragram. Instead, they are as
  // detected in the headers of usenet articles.
  MSFT_CP1252,          // 27: CP1252 aka MSFT euro ascii
  RUSSIAN_KOI8_RU,      // 28: CP21866 aka KOI8_RU, used for Ukrainian
  MSFT_CP1250,          // 29: CP1250 aka MSFT eastern european
  ISO_8859_15,          // 30: aka ISO_8859_0 aka ISO_8859_1 euroized
  //----------------------------------------------------------

  //----------------------------------------------------------
  // These are in BasisTech but not in Teragram. They are
  // needed for new interface languages. Now detected by
  // research langid
  MSFT_CP1254,          // 31: used for Turkish
  MSFT_CP1257,          // 32: used in Baltic countries
  //----------------------------------------------------------

  //----------------------------------------------------------
  //----------------------------------------------------------
  // New encodings detected by Teragram
  ISO_8859_11,          // 33: aka TIS-620, used for Thai
  MSFT_CP874,           // 34: used for Thai
  MSFT_CP1256,          // 35: used for Arabic

  //----------------------------------------------------------
  // Detected as ISO_8859_8 by Teragram, but can be found in META tags
  MSFT_CP1255,          // 36: Logical Hebrew Microsoft
  ISO_8859_8_I,         // 37: Iso Hebrew Logical
  HEBREW_VISUAL,        // 38: Iso Hebrew Visual
  //----------------------------------------------------------

  //----------------------------------------------------------
  // Detected by research langid
  CZECH_CP852,          // 39
  CZECH_CSN_369103,     // 40: aka ISO_IR_139 aka KOI8_CS
  MSFT_CP1253,          // 41: used for Greek
  RUSSIAN_CP866,        // 42
  //----------------------------------------------------------
  HZ_ENCODING,
  ISO2022_CN,
  ISO2022_KR,

  NUM_ENCODINGS              // Always keep this at the end. It is not a
                             // valid Encoding enum, it is only used to
                             // indicate the total number of Encodings.
};

const int kNumLanguages = NUM_LANGUAGES;
const int kNumEncodings = NUM_ENCODINGS;

#endif  // OMAHA_COMMON_LANG_ENC_H_
