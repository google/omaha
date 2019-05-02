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
//
// Omaha resource manager.

#ifndef OMAHA_GOOPDATE_RESOURCE_MANAGER_H__
#define OMAHA_GOOPDATE_RESOURCE_MANAGER_H__

#include <atlstr.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "base/synchronized.h"

namespace omaha {

class ResourceManager {
 public:
  // Create must be called before going multithreaded.
  static HRESULT CreateForDefaultLanguage(bool is_machine,
                                          const CString& resource_dir);
  static HRESULT Create(bool is_machine,
                        const CString& resource_dir,
                        const CString& lang);
  static void Delete();

  static ResourceManager& Instance();

  static void GetSupportedLanguageDllNames(std::vector<CString>* filenames);

  // Gets resource DLL handle for the given language. DLL will be loaded if
  // necessary.
  HRESULT GetResourceDll(const CString& language, HINSTANCE* dll_handle);

 private:
  struct ResourceDllInfo {
    ResourceDllInfo() : dll_handle(NULL) {}

    HMODULE dll_handle;
    CString file_path;
    CString language;
  };

  ResourceManager(bool is_machine, const CString& resource_dir);
  ~ResourceManager();

  // The resource manager tries to load the resource dll corresponding to
  // the language in the following order:
  // 1. Language parameter.
  // 2. Language in the registry.
  // 3. First file returned by NTFS in the module directory.
  HRESULT LoadResourceDll(const CString& language, ResourceDllInfo* dll_info);
  HRESULT SetDefaultResourceByLanguage(const CString& language);
  HRESULT LoadLibraryAsDataFile(const CString& filename,
                                ResourceDllInfo* dll_info) const;

  // Gets resource DLL info for the given language. DLL will be loaded if
  // necessary.
  HRESULT GetResourceDllInfo(const CString& language,
                             ResourceDllInfo* dll_info);

  static CString GetResourceDllName(const CString& language);

  LLock lock_;
  typedef std::map<CString, ResourceDllInfo> LanguageToResourceMap;

  bool is_machine_;
  CString resource_dir_;
  LanguageToResourceMap resource_map_;
  HINSTANCE saved_atl_resource_;

  static ResourceManager* instance_;

  friend class ResourceManagerTest;

  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_RESOURCE_MANAGER_H__
