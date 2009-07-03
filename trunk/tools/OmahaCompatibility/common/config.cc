// Copyright 2008-2009 Google Inc.
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


#include "omaha/tools/omahacompatibility/common/config.h"
#include <windows.h>
#include <wincrypt.h>
#include "base/scoped_ptr.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/signatures.h"
#include "omaha/common/path.h"
#include "omaha/common/utils.h"

namespace omaha {

const TCHAR* const kConfigApplicationProfile = _T("Application");
const TCHAR* const kConfigAppName = _T("AppName");
const TCHAR* const kConfigAppGuid = _T("AppGuid");
const TCHAR* const kConfigAppNeedsAdmin = _T("NeedsAdmin");
const TCHAR* const kConfigAppLanguage = _T("Language");
const TCHAR* const kConfigAppVersion1 = _T("Version1");
const TCHAR* const kConfigAppInstaller1 = _T("Installer1");
const TCHAR* const kConfigAppVersion2 = _T("Version2");
const TCHAR* const kConfigAppInstaller2 = _T("Installer2");

bool ComputeSHA(const CString& file_name, ConfigResponse* response) {
  ASSERT1(response);

  std::vector<CString> files;
  std::vector<byte> hash_vector;
  files.push_back(file_name);

  // Check if the file exists
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  BOOL success = GetFileAttributesEx(file_name, GetFileExInfoStandard, &attrs);
  if (!success) {
    return false;
  }

  // Calculate the hash
  CryptoHash crypto;
  crypto.Compute(files, 512000000L, &hash_vector);
  CString encoded;
  Base64::Encode(hash_vector, &encoded);
  response->hash = encoded;
  response->size = attrs.nFileSizeLow;

  return true;
}

HRESULT ReadProfileString(const CString& file_name,
                          const CString& key_name,
                          CString* value) {
  CString val;
  DWORD ret = ::GetPrivateProfileString(kConfigApplicationProfile,
                                        key_name,
                                        _T(""),
                                        CStrBuf(val, MAX_PATH),
                                        MAX_PATH,
                                        file_name);
  if (ret == MAX_PATH - 1) {
    return E_FAIL;
  }
  *value = val;

  return S_OK;
}

HRESULT ReadConfigFile(const CString& file_name,
                       const CString& download_url_prefix,
                       ConfigResponses* config_responses) {
  ASSERT1(config_responses);

  ConfigResponse config_response;

  CString app_name;
  HRESULT hr = ReadProfileString(file_name, kConfigAppName, &app_name);
  if (FAILED(hr)) {
    return hr;
  }
  config_response.app_name = app_name;

  CString app_guid;
  hr = ReadProfileString(file_name, kConfigAppGuid, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }
  config_response.guid = StringToGuid(app_guid);

  CString needs_admin;
  hr = ReadProfileString(file_name, kConfigAppNeedsAdmin, &needs_admin);
  if (FAILED(hr)) {
    return hr;
  }
  const TCHAR* const kFalse = _T("false");
  if (_wcsnicmp(kFalse, needs_admin, wcslen(kFalse)) == 0) {
    config_response.needs_admin = false;
  } else {
    config_response.needs_admin = true;
  }

  CString language;
  hr = ReadProfileString(file_name, kConfigAppLanguage, &language);
  if (FAILED(hr)) {
    return hr;
  }
  config_response.language = language;


  // Read the first config.
  ConfigResponse config_response1 = config_response;
  CString installer1;
  hr = ReadProfileString(file_name, kConfigAppInstaller1, &installer1);
  if (FAILED(hr)) {
    return hr;
  }

  if (!File::Exists(installer1)) {
    printf("Error: Could not open file %s\n", installer1);
    printf("Make sure you specify an absolute path to the file\n");
    return E_FAIL;
  }

  if (!ComputeSHA(installer1, &config_response1)) {
    return E_FAIL;
  }

  CString version1;
  hr = ReadProfileString(file_name, kConfigAppVersion1, &version1);
  if (FAILED(hr)) {
    return hr;
  }
  config_response1.version = version1;
  config_response1.local_file_name = installer1;
  config_response1.url = download_url_prefix + _T("/") +
                         GetFileFromPath(installer1);

  // Read the second config.
  ConfigResponse config_response2 = config_response;
  CString installer2;
  hr = ReadProfileString(file_name, kConfigAppInstaller2, &installer2);
  if (FAILED(hr)) {
    return hr;
  }
  if (!File::Exists(installer2)) {
    printf("Error: Could not open file %s\n", installer1);
    printf("Make sure you specify an absolute path to the file\n");
    return E_FAIL;
  }
  if (!ComputeSHA(installer2, &config_response2)) {
    return E_FAIL;
  }

  CString version2;
  hr = ReadProfileString(file_name, kConfigAppVersion2, &version2);
  if (FAILED(hr)) {
    return hr;
  }
  config_response2.version = version2;
  config_response2.local_file_name = installer2;
  config_response2.url = download_url_prefix + _T("/") +
                         GetFileFromPath(installer2);

  // Return the results.
  config_responses->push_back(config_response1);
  config_responses->push_back(config_response2);

  return S_OK;
}

}  // namespace omaha

