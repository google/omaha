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

#include "omaha/goopdate/offline_utils.h"
#include <atlpath.h>
#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"

namespace omaha {

namespace offline_utils {

CString GetV2OfflineManifest(const CString& app_id,
                             const CString& offline_dir) {
  CPath manifest_filename(app_id);
  VERIFY1(manifest_filename.AddExtension(_T(".gup")));
  return ConcatenatePath(offline_dir, manifest_filename);
}

HRESULT FindV2OfflinePackagePath(const CString& offline_app_dir,
                                 CString* package_path) {
  ASSERT1(!offline_app_dir.IsEmpty());
  ASSERT1(package_path);
  package_path->Empty();

  CORE_LOG(L3, (_T("[FindV2OfflinePackagePath][%s]"), offline_app_dir));

  CString pattern(_T("*"));
  std::vector<CString> files;
  HRESULT hr = FindFiles(offline_app_dir, pattern, &files);
  if (FAILED(hr)) {
    return hr;
  }

  if (files.size() != 3) {
    CORE_LOG(LE, (_T("[Cannot guess filename with multiple files]")));
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  CString local_package_path;
  // Skip over "." and "..".
  size_t i = 0;
  for (; i < files.size(); ++i) {
    local_package_path = ConcatenatePath(offline_app_dir, files[i]);
    if (!File::IsDirectory(local_package_path)) {
      break;
    }
  }
  if (i == files.size()) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  CORE_LOG(L3, (_T("[Non-standard/legacy package][%s]"), local_package_path));
  *package_path = local_package_path;
  return S_OK;
}

HRESULT ParseOfflineManifest(const CString& app_id,
                             const CString& offline_dir,
                             xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[ParseOfflineManifest][%s][%s]"), app_id, offline_dir));
  ASSERT1(update_response);

  CString manifest_path(ConcatenatePath(offline_dir, kOfflineManifestFileName));
  if (!File::Exists(manifest_path)) {
    manifest_path = GetV2OfflineManifest(app_id, offline_dir);
  }

  HRESULT hr = update_response->DeserializeFromFile(manifest_path);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DeserializeFromFile failed][%s][0x%x]"),
                  manifest_path, hr));
    return hr;
  }

  return S_OK;
}

}  // namespace offline_utils

}  // namespace omaha
