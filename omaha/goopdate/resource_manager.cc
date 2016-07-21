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

#include "omaha/goopdate/resource_manager.h"
#include <windows.h>
#include <map>
#include <vector>
#include "omaha/base/constants.h"
#include "omaha/base/commontypes.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/file_ver.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/utils.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/lang.h"

namespace omaha {

ResourceManager* ResourceManager::instance_ = NULL;

HRESULT ResourceManager::CreateForDefaultLanguage(bool is_machine,
                                                  const CString& resource_dir) {
  return Create(is_machine,
                resource_dir,
                lang::GetDefaultLanguage(is_machine));
}

HRESULT ResourceManager::Create(
    bool is_machine, const CString& resource_dir, const CString& lang) {
  CString language = lang;
  if (language.IsEmpty() || !lang::IsLanguageSupported(language)) {
    language = lang::GetDefaultLanguage(is_machine);
  }

  if (!instance_) {
    instance_ = new ResourceManager(is_machine, resource_dir);
    return instance_->SetDefaultResourceByLanguage(language);
  }
  return S_OK;
}

void ResourceManager::Delete() {
  ResourceManager* instance = NULL;
  instance = omaha::interlocked_exchange_pointer(&instance_, instance);

  delete instance;
}

ResourceManager& ResourceManager::Instance() {
  ASSERT1(instance_);
  return *instance_;
}

ResourceManager::ResourceManager(bool is_machine, const CString& resource_dir)
    : is_machine_(is_machine),
      resource_dir_(resource_dir),
      saved_atl_resource_(NULL) {
}

ResourceManager::~ResourceManager() {
  if (saved_atl_resource_) {
    _AtlBaseModule.SetResourceInstance(saved_atl_resource_);
  }
}

HRESULT ResourceManager::SetDefaultResourceByLanguage(const CString& language) {
  ResourceDllInfo dll_info;
  HRESULT hr = GetResourceDllInfo(language, &dll_info);
  if (FAILED(hr)) {
    return hr;
  }

  // All CString.LoadString and CreateDialog calls should use the resource of
  // the default language.
  saved_atl_resource_ = _AtlBaseModule.SetResourceInstance(dll_info.dll_handle);

  return hr;
}

HRESULT ResourceManager::GetResourceDll(const CString& language,
                                        HINSTANCE* dll_handle) {
  ASSERT1(dll_handle);

  ResourceDllInfo dll_info;
  HRESULT hr = GetResourceDllInfo(language, &dll_info);
  if (FAILED(hr)) {
    return  hr;
  }
  *dll_handle = dll_info.dll_handle;
  return S_OK;
}

HRESULT ResourceManager::GetResourceDllInfo(const CString& language,
                                            ResourceDllInfo* dll_info) {
  ASSERT1(dll_info);
  __mutexScope(lock_);

  ASSERT1(lang::IsLanguageSupported(language));

  LanguageToResourceMap::const_iterator it = resource_map_.find(language);
  if (it != resource_map_.end()) {
    *dll_info = it->second;
    return S_OK;
  }
  return LoadResourceDll(language, dll_info);
}

// Assumes that the language has not been loaded previously.
HRESULT ResourceManager::LoadResourceDll(const CString& language,
                                         ResourceDllInfo* dll_info) {
  ASSERT1(dll_info);
  ASSERT1(lang::IsLanguageSupported(language));
  dll_info->dll_handle = NULL;

  __mutexScope(lock_);

  ASSERT1(resource_map_.find(language) == resource_map_.end());

  // First try to load the resource dll for the language parameter.
  HRESULT hr = LoadLibraryAsDataFile(GetResourceDllName(language), dll_info);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Resource dll load failed.][0x%08x]"), hr));
    return hr;
  }

  FileVer file_ver;
  VERIFY1(file_ver.Open(dll_info->file_path));
  dll_info->language = file_ver.QueryValue(kLanguageVersionName);
  CORE_LOG(L1, (_T("[Loaded resource dll %s]"), dll_info->file_path));

  resource_map_.insert(std::make_pair(language, *dll_info));
  return S_OK;
}

HRESULT ResourceManager::LoadLibraryAsDataFile(
    const CString& filename,
    ResourceDllInfo* dll_info) const {
  ASSERT1(!filename.IsEmpty());
  ASSERT1(dll_info);
  ASSERT1(!resource_dir_.IsEmpty());

  dll_info->file_path = ConcatenatePath(resource_dir_, filename);
  if (dll_info->file_path.IsEmpty()) {
    ASSERT1(false);
    return GOOPDATE_E_RESOURCE_DLL_PATH_EMPTY;
  }
  dll_info->dll_handle = ::LoadLibraryEx(dll_info->file_path,
                                         NULL,
                                         LOAD_LIBRARY_AS_DATAFILE);
  if (!dll_info->dll_handle) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(L2, (_T("[Could not load resource dll %s.]"),
                 dll_info->file_path));
    return hr;
  }

  return S_OK;
}

CString ResourceManager::GetResourceDllName(const CString& language) {
  ASSERT1(!language.IsEmpty());

  CString filename;
  SafeCStringFormat(&filename, kOmahaResourceDllNameFormat,
                    lang::GetWrittenLanguage(language));
  return filename;
}

void ResourceManager::GetSupportedLanguageDllNames(
    std::vector<CString>* filenames) {
  std::vector<CString> codes;
  lang::GetSupportedLanguages(&codes);

  for (size_t i = 0; i < codes.size(); ++i) {
    if (lang::DoesSupportedLanguageUseDifferentId(codes[i])) {
      // There is not a separate DLL for this language.
      continue;
    }
    filenames->push_back(GetResourceDllName(codes[i]));
  }
}

}  // namespace omaha
