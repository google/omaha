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

#include "omaha/common/lang.h"
#include <windows.h>
#include <set>
#include <map>
#include <vector>
#include "omaha/base/constants.h"
#include "omaha/base/commontypes.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/file_ver.h"
#include "omaha/base/logging.h"
#include "omaha/base/debug.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

namespace lang {

namespace {

const LANGID kFallbackLangID = 0x0409;
const TCHAR* const kFallbackLanguage = _T("en");

// This is the structure of the table which contains the language identifier
// and the associated language string.
struct LangIDAndPath {
  LANGID langid;
  TCHAR lang[12];
};
const LangIDAndPath kLanguageTranslationTable[] = {
  // First we collect all main languages here which need special treatment.
  // Special treatment might be necessary because of:
  //  1.) Google has a special region code which is "Google specific". They
  //      trump everything and should be at the beginning. (e.g. "iw")
  //  2.) Usually a lot of languages are like "en-US", "en-GB",... and start
  //      with the main language. However some languages like Norwegian do not
  //      follow this scheme and we put them therefore at the top.
  // The matching algorithm will look first for the full match, but at the same
  // time it will look for a partial match. We want to add therefore the partial
  // matches as high as possible to get them as a fallback.
  { 0x0409, _T("en")         },  // _T("English (United States)")
  { 0x040d, _T("iw")         },  // _T("Hebrew")
  { 0x0464, _T("fil")        },  // _T("Tagalog")
  { 0x0414, _T("no")         },  // _T("Norwegian (Bokmal, Norway)")

  // and then we have all dialects here:
  // { 0x041c, _T("sq-AL")      },  // _T("Albanian (Albania)")")
  // { 0x0484, _T("gsw-FR")     },  // _T("Alsatian (France)")
  { 0x045e, _T("am")  },  // _T("am-ET")      },  // _T("Amharic (Ethiopia)")
  { 0x1401, _T("ar")  },  // _T("ar-DZ")      },  // _T("Arabic (Algeria)")
  { 0x3c01, _T("ar")  },  // _T("ar-BH")      },  // _T("Arabic (Bahrain)")
  { 0x0c01, _T("ar")  },  // _T("ar-EG")      },  // _T("Arabic (Egypt)")
  { 0x0801, _T("ar")  },  // _T("ar-IQ")      },  // _T("Arabic (Iraq)")
  { 0x2c01, _T("ar")  },  // _T("ar-JO")      },  // _T("Arabic (Jordan)")
  { 0x3401, _T("ar")  },  // _T("ar-KW")      },  // _T("Arabic (Kuwait)")
  { 0x3001, _T("ar")  },  // _T("ar-LB")      },  // _T("Arabic (Lebanon)")
  { 0x1001, _T("ar")  },  // _T("ar-LY")      },  // _T("Arabic (Libya)")
  { 0x1801, _T("ar")  },  // _T("ar-MA")      },  // _T("Arabic (Morocco)")
  { 0x2001, _T("ar")  },  // _T("ar-OM")      },  // _T("Arabic (Oman)")
  { 0x4001, _T("ar")  },  // _T("ar-QA")      },  // _T("Arabic (Qatar)")
  { 0x0401, _T("ar")  },  // _T("ar-SA")      },  // _T("Arabic (Saudi Arabia)")
  { 0x2801, _T("ar")  },  // _T("ar-SY")      },  // _T("Arabic (Syria)")
  { 0x1c01, _T("ar")  },  // _T("ar-TN")      },  // _T("Arabic (Tunisia)")
  { 0x3801, _T("ar")  },  // _T("ar-AE")      },  // _T("Arabic (U.A.E.)")
  { 0x2401, _T("ar")  },  // _T("ar-YE")      },  // _T("Arabic (Yemen)")
  { 0x042b, _T("ar")  },  // _T("hy-AM")      },  // _T("Armenian (Armenia)")
  // { 0x044d, _T("as-IN")      },  // _T("Assamese (India)")
  // { 0x082c, _T("az-Cyrl-AZ") },  // _T("Azeri (Azerbaijan, Cyrillic)")
  // { 0x042c, _T("az-Latn-AZ") },  // _T("Azeri (Azerbaijan, Latin)")
  // { 0x046d, _T("ba-RU")      },  // _T("Bashkir (Russia)")
  // { 0x042d, _T("eu-ES")      },  // _T("Basque (Basque)")
  // { 0x0423, _T("be-BY")      },  // _T("Belarusian (Belarus)")
  { 0x0445, _T("bn")  },  // _T("bn-IN")      },  // _T("Bengali (India)")
  // { 0x201a, _T("bs-Cyrl-BA") },  // _T("Bosnian (Bosnia and Herzegovina,
  //                                       Cyrillic)")
  // { 0x141a, _T("bs-Latn-BA") },  // _T("Bosnian (Bosnia and Herzegovina,
  //                                       Latin)")
  // { 0x047e, _T("br-FR")      },  // _T("Breton (France)")
  { 0x0402, _T("bg")  },  // _T("bg-BG")   },  // _T("Bulgarian (Bulgaria)")
  { 0x0403, _T("ca")  },  // _T("ca-ES")   },  // _T("Catalan (Catalan)")
  { 0x0c04, _T("zh-HK") },  // _T("zh-HK")     // _T("Chinese
                            //   (Hong Kong SAR, PRC)")
  { 0x1404, _T("zh-TW") },  // _T("zh-MO")   },  // _T("Chinese (Macao SAR)")
  { 0x0804, _T("zh-CN") },  // _T("zh-CN")   },  // _T("Chinese (PRC)")
  { 0x1004, _T("zh-CN") },  // _T("zh-SG")   },  // _T("Chinese (Singapore)")
  { 0x0404, _T("zh-TW") },  // _T("Chinese (Taiwan)")
  { 0x101a, _T("hr")    },  // _T("hr-BA")   },  // _T("Croatian
                            //   (Bosnia and Herzegovina, Latin)")
  { 0x041a, _T("hr")    },  // _T("hr-HR")   },  // _T("Croatian (Croatia)")
  { 0x0405, _T("cs")    },  // _T("cs-CZ")   },  // _T("Czech (Czech Republic)")
  { 0x0406, _T("da")    },  // _T("da-DK")   },  // _T("Danish (Denmark)")
  // { 0x048c, _T("gbz-AF")     },  // _T("Dari (Afghanistan)")
  // { 0x0465, _T("dv-MV")      },  // _T("Divehi (Maldives)")
  { 0x0813, _T("nl")  },  // _T("nl-BE")   },  // _T("Dutch (Belgium)")
  { 0x0413, _T("nl")  },  // _T("nl-NL")   },  // _T("Dutch (Netherlands)")
  { 0x0c09, _T("en")  },  // _T("en-AU")   },  // _T("English (Australia)")
  { 0x2809, _T("en")  },  // _T("en-BZ")   },  // _T("English (Belize)")
  { 0x1009, _T("en")  },  // _T("en-CA")   },  // _T("English (Canada)")
  { 0x2409, _T("en")  },  // _T("en-029")  },  // _T("English (Caribbean)")
  { 0x4009, _T("en")  },  // _T("en-IN")   },  // _T("English (India)")
  { 0x1809, _T("en")  },  // _T("en-IE")   },  // _T("English (Ireland)")
  { 0x2009, _T("en")  },  // _T("en-JM")   },  // _T("English (Jamaica)")
  { 0x4409, _T("en")  },  // _T("en-MY")   },  // _T("English (Malaysia)")
  { 0x1409, _T("en")  },  // _T("en-NZ")   },  // _T("English (New Zealand)")
  { 0x3409, _T("en")  },  // _T("en-PH")   },  // _T("English (Philippines)")
  { 0x4809, _T("en")  },  // _T("en-SG")   },  // _T("English (Singapore)")
  { 0x1c09, _T("en")  },  // _T("en-ZA")   },  // _T("English (South Africa)")
  { 0x2c09, _T("en")  },  // _T("en-TT")   },  // _T("English
                          //   (Trinidad and Tobago)")
  { 0x0809, _T("en-GB") },  // _T("English (United Kingdom)")
  { 0x3009, _T("en")    },  // _T("en-ZW")      },  // _T("English (Zimbabwe)")
  { 0x0425, _T("et")  },  // _T("et-EE")   },  // _T("Estonian (Estonia)")
  // { 0x0438, _T("fo-FO")  },  // _T("Faroese (Faroe Islands)")
  { 0x0464, _T("fil") },  // _T("fil-PH")    },  // _T("Filipino (Philippines)")
  { 0x040b, _T("fi")  },  // _T("fi-FI")      },  // _T("Finnish (Finland)")
  { 0x080c, _T("fr")  },  // _T("fr-BE")      },  // _T("French (Belgium)")
  { 0x0c0c, _T("fr")  },  // _T("fr-CA")      },  // _T("French (Canada)")
  { 0x040c, _T("fr")  },  // _T("fr-FR")      },  // _T("French (France)")
  { 0x140c, _T("fr")  },  // _T("fr-LU")      },  // _T("French (Luxembourg)")
  { 0x180c, _T("fr")  },  // _T("fr-MC")      },  // _T("French (Monaco)")
  { 0x100c, _T("fr")  },  // _T("fr-CH")      },  // _T("French (Switzerland)")
  // { 0x0462, _T("fy-NL")      },  // _T("Frisian (Netherlands)")
  // { 0x0456, _T("gl-ES")      },  // _T("Galician (Spain)")
  // { 0x0437, _T("ka-GE")      },  // _T("Georgian (Georgia)")
  { 0x0c07, _T("de")  },  // _T("de-AT")   },  // _T("German (Austria)")
  { 0x0407, _T("de")  },  // _T("de-DE")   },  // _T("German (Germany)")
  { 0x1407, _T("de")  },  // _T("de-LI")   },  // _T("German (Liechtenstein)")
  { 0x1007, _T("de")  },  // _T("de-LU")   },  // _T("German (Luxembourg)")
  { 0x0807, _T("de")  },  // _T("de-CH")   },  // _T("German (Switzerland)")
  { 0x0408, _T("el")  },  // _T("el-GR")   },  // _T("Greek (Greece)")
  // { 0x046f, _T("kl-GL")      },  // _T("Greenlandic (Greenland)")
  { 0x0447, _T("gu")  },  // _T("gu-IN")      },  // _T("Gujarati (India)")
  // { 0x0468, _T("ha-Latn-NG") },  // _T("Hausa (Nigeria, Latin)")
  // { 0x040d, _T("he-IL")      },  // _T("Hebrew (Israel)")
  { 0x0439, _T("hi")  },  // _T("hi-IN")   },  // _T("Hindi (India)")
  { 0x040e, _T("hu")  },  // _T("hu-HU")   },  // _T("Hungarian (Hungary)")
  { 0x040f, _T("is")  },  // _T("is-IS")      },  // _T("Icelandic (Iceland)")
  // { 0x0470, _T("ig-NG")      },  // _T("Igbo (Nigeria)")
  { 0x0421, _T("id")  },  // _T("id-ID")    },  // _T("Indonesian (Indonesia)")
  // { 0x085d, _T("iu-Latn-CA") },  // _T("Inuktitut (Canada, Latin)")
  // { 0x045d, _T("iu-Cans-CA") },  // _T("Inuktitut (Canada, Syllabics)")
  // { 0x083c, _T("ga-IE")      },  // _T("Irish (Ireland)")
  { 0x0410, _T("it")  },  // _T("it-IT")      },  // _T("Italian (Italy)")
  { 0x0810, _T("it")  },  // _T("it-CH")      },  // _T("Italian (Switzerland)")
  { 0x0411, _T("ja")  },  // _T("ja-JP")      },  // _T("Japanese (Japan)")
  { 0x044b, _T("kn")  },  // _T("kn-IN")      },  // _T("Kannada (India)")
  // { 0x043f, _T("kk-KZ")      },  // _T("Kazakh (Kazakhstan)")
  // { 0x0453, _T("kh-KH")      },  // _T("Khmer (Cambodia)")
  // { 0x0486, _T("qut-GT")     },  // _T("K'iche (Guatemala)")
  // { 0x0487, _T("rw-RW")      },  // _T("Kinyarwanda (Rwanda)")
  // { 0x0457, _T("kok-IN")     },  // _T("Konkani (India)")
  { 0x0812, _T("ko")         },  // _T("ko-Jo")      },  // _T("Korean (Johab)")
  { 0x0412, _T("ko")         },  // _T("ko-KR")      },  // _T("Korean (Korea)")
  // { 0x0440, _T("ky-KG")      },  // _T("Kyrgyz (Kyrgyzstan)")
  // { 0x0454, _T("lo-LA")      },  // _T("Lao (Lao PDR)")
  { 0x0426, _T("lv")  },  // _T("lv-LV")   },  // _T("Latvian (Latvia)")
  { 0x0427, _T("lt")  },  // _T("lt-LT")   },  // _T("Lithuanian (Lithuania)")
  // { 0x082e, _T("dsb-DE")     },  // _T("Lower Sorbian (Germany)")
  // { 0x046e, _T("lb-LU")      },  // _T("Luxembourgish (Luxembourg)")
  // { 0x042f, _T("mk-MK")      },  // _T("Macedonian (Macedonia, FYROM)")
  { 0x083e, _T("ms")  },  // _T("ms-BN")  },  // _T("Malay (Brunei Darussalam)")
  { 0x043e, _T("ms")  },  // _T("ms-MY")      },  // _T("Malay (Malaysia)")
  { 0x044c, _T("ml")  },  // _T("ml-IN")      },  // _T("Malayalam (India)")
  // { 0x043a, _T("mt-MT")      },  // _T("Maltese (Malta)")
  // { 0x0481, _T("mi-NZ")      },  // _T("Maori (New Zealand)")
  // { 0x047a, _T("arn-CL")     },  // _T("Mapudungun (Chile)")
  { 0x044e, _T("mr")  },  // _T("mr-IN")      },  // _T("Marathi (India)")
  // { 0x047c, _T("moh-CA")     },  // _T("Mohawk (Canada)")
  // { 0x0450, _T("mn-Cyrl-MN") },  // _T("Mongolian (Mongolia)")
  // { 0x0850, _T("mn-Mong-CN") },  // _T("Mongolian (PRC)")
  // { 0x0461, _T("ne-NP")      },  // _T("Nepali (Nepal)")
  // { 0x0414, _T("nb-NO")      },  // _T("Norwegian (Bokmal, Norway)")
  // { 0x0814, _T("nn-NO")      },  // _T("Norwegian (Nynorsk, Norway)")
  // { 0x0482, _T("oc-FR")      },  // _T("Occitan (France)")
  // { 0x0448, _T("or-IN") },  // _T("Oriya (India)")
  // { 0x0463, _T("ps-AF") },  // _T("Pashto (Afghanistan)")
  { 0x0429, _T("fa")  },  // _T("fa-IR")      },  // _T("Persian (Iran)")
  { 0x0415, _T("pl")  },  // _T("pl-PL")      },  // _T("Polish (Poland)")
  { 0x0416, _T("pt-BR")      },  // _T("Portuguese (Brazil)")
  { 0x0816, _T("pt-PT")      },  // _T("Portuguese (Portugal)")
  // { 0x0446, _T("pa-IN")      },  // _T("Punjabi (India)")
  // { 0x046b, _T("quz-BO")     },  // _T("Quechua (Bolivia)")
  // { 0x086b, _T("quz-EC")     },  // _T("Quechua (Ecuador)")
  // { 0x0c6b, _T("quz-PE")     },  // _T("Quechua (Peru)")
  { 0x0418, _T("ro")  },  // _T("ro-RO")      },  // _T("Romanian (Romania)")
  // { 0x0417, _T("rm-CH")      },  // _T("Romansh (Switzerland)")
  { 0x0419, _T("ru")  },  // _T("ru-RU")      },  // _T("Russian (Russia)")
  // { 0x243b, _T("smn-FI")     },  // _T("Sami (Inari, Finland)")
  // { 0x103b, _T("smj-NO")     },  // _T("Sami (Lule, Norway)")
  // { 0x143b, _T("smj-SE")     },  // _T("Sami (Lule, Sweden)")
  // { 0x0c3b, _T("se-FI")      },  // _T("Sami (Northern, Finland)")
  // { 0x043b, _T("se-NO")      },  // _T("Sami (Northern, Norway)")
  // { 0x083b, _T("se-SE")      },  // _T("Sami (Northern, Sweden)")
  // { 0x203b, _T("sms-FI")     },  // _T("Sami (Skolt, Finland)")
  // { 0x183b, _T("sma-NO")     },  // _T("Sami (Southern, Norway)")
  // { 0x1c3b, _T("sma-SE")     },  // _T("Sami (Southern, Sweden)")
  // { 0x044f, _T("sa-IN")      },  // _T("Sanskrit (India)")
  { 0x1c1a, _T("sr")  },  // _T("sr-Cyrl-BA") },  // _T("Serbian
                          //   (Bosnia and Herzegovina, Cyrillic)")
  { 0x181a, _T("sr")  },  // _T("sr-Latn-BA") },  // _T("Serbian
                          //   (Bosnia and Herzegovina, Latin)")
  { 0x0c1a, _T("sr")  },  // _T("sr-Cyrl-CS") },  // _T("Serbian
                          //   (Serbia and Montenegro, Cyrillic)")
  { 0x081a, _T("sr")  },  // _T("sr-Latn-CS") },  // _T("Serbian
                          //   (Serbia and Montenegro, Latin)")
  // { 0x046c, _T("ns-ZA")      },  // _T("Sesotho sa Leboa/Northern Sotho
  //                                       (South Africa)")
  // { 0x0432, _T("tn-ZA")      },  // _T("Setswana/Tswana (South Africa)")
  // { 0x045b, _T("si-LK")      },  // _T("Sinhala (Sri Lanka)")
  { 0x041b, _T("sk")  },  // _T("sk-SK")   },  // _T("Slovak (Slovakia)")
  { 0x0424, _T("sl")  },  // _T("sl-SI")   },  // _T("Slovenian (Slovenia)")
  { 0x2c0a, _T("es-419") },  // _T("es-AR")   },  // _T("Spanish (Argentina)")
  { 0x400a, _T("es-419") },  // _T("es-BO")   },  // _T("Spanish (Bolivia)")
  { 0x340a, _T("es-419") },  // _T("es-CL")   },  // _T("Spanish (Chile)")
  { 0x240a, _T("es-419") },  // _T("es-CO")   },  // _T("Spanish (Colombia)")
  { 0x140a, _T("es-419") },  // _T("es-CR")   },  // _T("Spanish (Costa Rica)")
  { 0x1c0a, _T("es-419") },  // _T("es-DO")   },  // _T("Spanish
                                                  //     (Dominican Republic)")
  { 0x300a, _T("es-419") },  // _T("es-EC")   },  // _T("Spanish (Ecuador)")
  { 0x440a, _T("es-419") },  // _T("es-SV")   },  // _T("Spanish (El Salvador)")
  { 0x100a, _T("es-419") },  // _T("es-GT")   },  // _T("Spanish (Guatemala)")
  { 0x480a, _T("es-419") },  // _T("es-HN")   },  // _T("Spanish (Honduras)")
  { 0x080a, _T("es-419") },  // _T("es-MX")   },  // _T("Spanish (Mexico)")
  { 0x4c0a, _T("es-419") },  // _T("es-NI")   },  // _T("Spanish (Nicaragua)")
  { 0x180a, _T("es-419") },  // _T("es-PA")   },  // _T("Spanish (Panama)")
  { 0x3c0a, _T("es-419") },  // _T("es-PY")   },  // _T("Spanish (Paraguay)")
  { 0x280a, _T("es-419") },  // _T("es-PE")   },  // _T("Spanish (Peru)")
  { 0x500a, _T("es-419") },  // _T("es-PR")   },  // _T("Spanish (Puerto Rico)")
  { 0x0c0a, _T("es")     },  // _T("es-ES")   },  // _T("Spanish (Spain)")
  { 0x040a, _T("es")     },  // _T("es-ES_tradnl")   },
                             // _T("Spanish (Spain, Traditional Sort)")
  { 0x540a, _T("es-419") },  // _T("es-US")   },  // _T("Spanish
                                                  //     (United States)")
  { 0x380a, _T("es-419") },  // _T("es-UY")   },  // _T("Spanish (Uruguay)")
  { 0x200a, _T("es-419") },  // _T("es-VE")   },  // _T("Spanish (Venezuela)")
  { 0x0441, _T("sw")  },  // _T("sw-KE")      },  // _T("Swahili (Kenya)")
  { 0x081d, _T("sv")  },  // _T("sv-FI")      },  // _T("Swedish (Finland)")
  { 0x041d, _T("sv")  },  // _T("sv-SE")      },  // _T("Swedish (Sweden)")
  // { 0x045a, _T("syr-SY")     },  // _T("Syriac (Syria)")
  // { 0x0428, _T("tg-Cyrl-TJ") },  // _T("Tajik (Tajikistan)")
  // { 0x085f, _T("tmz-Latn-DZ")},  // _T("Tamazight (Algeria, Latin)")
  { 0x0449, _T("ta")  },  // _T("ta-IN")      },  // _T("Tamil (India)")
  // { 0x0444, _T("tt-RU") },  // _T("Tatar (Russia)")
  { 0x044a, _T("te")  },  // _T("te-IN") }, // _T("Telugu (India)")
  { 0x041e, _T("th")  },  // _T("th-TH")      },  // _T("Thai (Thailand)")
  // { 0x0851, _T("bo-BT")      },  // _T("Tibetan (Bhutan)")
  // { 0x0451, _T("bo-CN")      },  // _T("Tibetan (PRC)")
  { 0x041f, _T("tr")  },  // _T("tr-TR")      },  // _T("Turkish (Turkey)")
  // { 0x0442, _T("tk-TM")      },  // _T("Turkmen (Turkmenistan)")
  // { 0x0480, _T("ug-CN")      },  // _T("Uighur (PRC)")
  { 0x0422, _T("uk")  },  // _T("uk-UA")      },  // _T("Ukrainian (Ukraine)")
  // { 0x042e, _T("wen-DE")     },  // _T("Upper Sorbian (Germany)")
  // { 0x0820,  _T("tr-IN")      },  // _T("Urdu(India)")
  { 0x0420, _T("ur")  },  // _T("ur-PK") },  // _T("Urdu (Pakistan)")
  // { 0x0843, _T("uz-Cyrl-UZ") }, // _T("Uzbek (Uzbekistan, Cyrillic)")
  // { 0x0443, _T("uz-Latn-UZ") },  // _T("Uzbek (Uzbekistan, Latin)")
  { 0x042a, _T("vi")  },  // _T("vi-VN")      },  // _T("Vietnamese (Vietnam)")
  // { 0x0452, _T("cy-GB")      },  // _T("Welsh (United Kingdom)")
  // { 0x0488, _T("wo-SN")      },  // _T("Wolof (Senegal)")
  // { 0x0434, _T("xh-ZA")      },  // _T("Xhosa/isiXhosa (South Africa)")
  // { 0x0485, _T("sah-RU")     },  // _T("Yakut (Russia)")
  // { 0x0478, _T("ii-CN")      },  // _T("Yi (PRC)")
  // { 0x046a, _T("yo-NG") }, // _T("Yoruba (Nigeria)")
  // { 0x0435, _T("zu-ZA") }, // _T("Zulu/isiZulu (South Africa)")
  { 0x0000, _T("")           }   // The list termination.
};
}

CString GetDefaultLanguage(bool is_system) {
  return GetLanguageForLangID(GetDefaultLanguageID(is_system));
}

LANGID GetDefaultLanguageID(bool is_system) {
  LANGID langid = is_system ? ::GetSystemDefaultLangID()
                            : ::GetUserDefaultLangID();
  if (IsLanguageIDSupported(langid)) {
    return langid;
  }

  return kFallbackLangID;
}

// If an exact match is not found, returns "en".
CString GetLanguageForLangID(LANGID langid) {
  ASSERT1(langid != 0);

  for (int i = 0; kLanguageTranslationTable[i].langid; ++i) {
    if (kLanguageTranslationTable[i].langid == langid) {
      return kLanguageTranslationTable[i].lang;
    }
  }

  return kFallbackLanguage;
}

bool IsLanguageIDSupported(LANGID langid) {
  for (int i = 0; kLanguageTranslationTable[i].langid; ++i) {
    if (langid == kLanguageTranslationTable[i].langid) {
      return true;
    }
  }

  return false;
}

bool IsLanguageSupported(const CString& language) {
  for (int i = 0; kLanguageTranslationTable[i].langid; ++i) {
    if (language.CompareNoCase(kLanguageTranslationTable[i].lang) == 0) {
      return true;
    }
  }

  return false;
}

void GetSupportedLanguages(std::vector<CString>* codes) {
  ASSERT1(codes);
  std::set<CString> languages;

  for (int i = 0; kLanguageTranslationTable[i].langid; ++i) {
    languages.insert(CString(kLanguageTranslationTable[i].lang));
  }

  for (std::set<CString>::const_iterator it = languages.begin();
       it != languages.end();
       ++it) {
    codes->push_back(*it);
  }
}

// Returns user default language if requested language is not supported.
CString GetLanguageForProcess(const CString& requested_language) {
  if (!requested_language.IsEmpty() &&
      IsLanguageSupported(requested_language)) {
    return requested_language;
  }

  return GetDefaultLanguage(false);
}

CString GetWrittenLanguage(const CString& requested_language) {
  CString written_language(requested_language);

  // Handle specific special cases
  // * Chinese (Hong Kong) - separate language but uses the zh-TW as written
  //   language.
  // * Hebrew synonyms - convert he to iw.
  if (!requested_language.CompareNoCase(_T("zh-HK"))) {
    written_language = _T("zh-TW");
  } else if (!requested_language.CompareNoCase(_T("he"))) {
    written_language = _T("iw");
  }

  ASSERT1(IsLanguageSupported(written_language));
  return written_language;
}

bool DoesSupportedLanguageUseDifferentId(const CString& requested_language) {
  return (requested_language.CompareNoCase(_T("zh-HK")) == 0);
}

}  // namespace lang

}  // namespace omaha
