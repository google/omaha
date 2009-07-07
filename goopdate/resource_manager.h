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
// Omaha resource manager.
// Design Notes: Currently the resource manager only supports one language.

#ifndef OMAHA_GOOPDATE_RESOURCE_MANAGER_H__
#define OMAHA_GOOPDATE_RESOURCE_MANAGER_H__

#include <atlstr.h>
#include <map>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

class ResourceManager {
 public:
  ResourceManager(bool is_machine, const CString& resource_dir);
  ~ResourceManager();

  // Loads the resource dll and sets it as the default resource dll in ATL.
  // The resource manager tries to load the resource dll corresponding to
  // the language in the following order:
  // 1. Language parameter.
  // 2. Language in the registry.
  // 3. First file returned by NTFS in the module directory.
  HRESULT LoadResourceDll(const CString& language);
  static CString GetDefaultUserLanguage();
  static CString GetLanguageForLangID(LANGID langid);
  static bool IsLanguageStringSupported(const CString& language);
  static void GetSupportedLanguages(std::vector<CString>* codes);
  static void GetSupportedLanguageDllNames(std::vector<CString>* filenames);
  HMODULE resource_dll() const { return resource_dll_; }
  CString language() const { return language_; }
  CString resource_dll_filepath() const { return resource_dll_filepath_; }

 private:
  static CString GetResourceDllName(const CString& language);
  HRESULT LoadResourceDllInternal(const CString& language);
  HRESULT LoadLibraryAsDataFile(const CString& filename);

  // The bool is only here as a key for the map.  This could be a hash_set but
  // the compiler doesn't like hash_set<CString>.
  static void GetDistinctLanguageMapFromTranslationTable(
      std::map<CString, bool>* map_lang);

  HMODULE resource_dll_;
  bool is_machine_;
  CString resource_dir_;
  CString language_;
  CString resource_dll_filepath_;

  // This is the structure of the table which contains the language identifier
  // and the associated language string.
  struct LangIDAndPath {
    LANGID langid;
    TCHAR lang[12];
  };
  static const LangIDAndPath kLanguageTranslationTable[];

  friend class ResourceManagerTest;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceManager);
};

}  // namespace omaha.

#endif  // OMAHA_GOOPDATE_RESOURCE_MANAGER_H__
