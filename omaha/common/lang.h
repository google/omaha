// Copyright 2007-2010 Google Inc.
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

#ifndef OMAHA_COMMON_LANG_H_
#define OMAHA_COMMON_LANG_H_

#include <atlstr.h>
#include <map>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

namespace lang {

CString GetDefaultLanguage(bool is_system);
LANGID GetDefaultLanguageID(bool is_system);

CString GetLanguageForLangID(LANGID langid);

bool IsLanguageIDSupported(LANGID langid);
bool IsLanguageSupported(const CString& language);
void GetSupportedLanguages(std::vector<CString>* codes);

// Returns requested_language if supported. Otherwise returns current user's
// language if supported or "en" if both of them are not supported.
CString GetLanguageForProcess(const CString& requested_language);

// Returns the written language name for a given language. For most languages,
// the names are exactly same. There are very few exceptions. For example, the
// written language of zh-HK is zh-TW.
CString GetWrittenLanguage(const CString& requested_language);

// Returns whether a language and its corresponding supported language have
// different names.
bool DoesSupportedLanguageUseDifferentId(const CString& requested_language);

}  // namespace lang

}  // namespace omaha

#endif  // OMAHA_COMMON_LANG_H_
