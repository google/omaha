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

#ifndef OMAHA_GOOPDATE_PACKAGE_CACHE_INTERNAL_H_
#define OMAHA_GOOPDATE_PACKAGE_CACHE_INTERNAL_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/synchronized.h"

namespace omaha {

namespace internal {

enum CacheDirectoryType {
  CACHE_DIRECTORY_ROOT,
  CACHE_DIRECTORY_APP,
  CACHE_DIRECTORY_VERSION,
};

struct PackageInfo {
  PackageInfo() {
    file_time.dwLowDateTime = 0;
    file_time.dwHighDateTime = 0;
    file_size.QuadPart = 0;
  }

  CString file_name;
  FILETIME file_time;
  ULARGE_INTEGER file_size;
};

// TODO(omaha): add tests for some of the functions below.
bool PackageSortByTimePredicate(const PackageInfo& package1,
                                const PackageInfo& package2);

bool IsSpecialDirectoryFindData(const WIN32_FIND_DATA& find_data);

bool IsSubDirectoryFindData(const WIN32_FIND_DATA& find_data);

bool IsFileFindData(const WIN32_FIND_DATA& find_data);

HRESULT FindDirectoryPackagesInfo(const CString& dir_path,
                                  CacheDirectoryType dir_type,
                                  std::vector<PackageInfo>* packages_info);

HRESULT FindAllPackagesInfo(const CString& cache_root,
                            std::vector<PackageInfo>* packages_info);

HRESULT FindAppPackagesInfo(const CString& app_dir,
                            std::vector<PackageInfo>* packages_info);

HRESULT FindVersionPackagesInfo(const CString& version_dir,
                                std::vector<PackageInfo>* packages_info);

HRESULT FindFilePackagesInfo(const CString& version_dir,
                             const WIN32_FIND_DATA& find_data,
                             std::vector<PackageInfo>* packages_info);

HRESULT FindDirectoryPackagesInfo(const CString& dir_path,
                                  CacheDirectoryType dir_type,
                                  std::vector<PackageInfo>* packages_info);

void SortPackageInfoByTime(std::vector<PackageInfo>* packages_info);

HRESULT FileCopy(File* source_file, const CString& destination);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_PACKAGE_CACHE_INTERNAL_H_

