// Copyright 2012 Google Inc.
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

#include "omaha/goopdate/google_app_command_verifier.h"

#include <Windows.h>
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/google_signaturevalidator.h"

namespace omaha {

namespace {

// Verifies the file contains the special markup resource to authorize
// application commands.
HRESULT VerifyAppCommandExecutableMarkup(const CString& filename) {
  const TCHAR* kMarkupResourceName = MAKEINTRESOURCE(1);
  const TCHAR* kMarkupResourceType = _T("GOOGLEUPDATEAPPLICATIONCOMMANDS");
  const DWORD kMarkupResourceExpectedValue = 1;

  scoped_library module(::LoadLibraryEx(filename, 0, LOAD_LIBRARY_AS_DATAFILE));
  if (!module) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  HRSRC resource(::FindResource(get(module),
                                kMarkupResourceName,
                                kMarkupResourceType));
  if (!resource) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (sizeof(kMarkupResourceExpectedValue) !=
      ::SizeofResource(get(module), resource)) {
    return E_UNEXPECTED;
  }

  HGLOBAL loaded_resource(::LoadResource(get(module), resource));
  if (!loaded_resource) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  const DWORD* value = static_cast<DWORD*>(::LockResource(loaded_resource));
  if (!value) {
    return E_HANDLE;
  }

  if (kMarkupResourceExpectedValue != *value) {
    return E_UNEXPECTED;
  }

  return S_OK;
}

}  // namespace

HRESULT GoogleAppCommandVerifier::VerifyExecutable(const CString& path) {
  CORE_LOG(L3, (_T("[GoogleAppCommandVerifier::VerifyExecutable][%s]"), path));

  DWORD dev_skip_command_verification = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueSkipCommandVerification,
                                 &dev_skip_command_verification)) &&
      (dev_skip_command_verification != 0)) {
    return S_OK;
  }

  // Don't allow UNC names.
  if (path.Find(_T("\\\\")) == 0) {
    return E_INVALIDARG;
  }

  if (!File::Exists(path)) {
    CORE_LOG(LE, (_T("[File does not exist]")));
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  const bool allow_network_check = true;
  HRESULT hr = VerifyGoogleAuthenticodeSignature(path, allow_network_check);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[VerifyGoogleAuthenticodeSignature failed][%#08x]"), hr));
    return hr;
  }

  hr = VerifyAppCommandExecutableMarkup(path);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[VerifyAppCommandExecutableMarkup failed][%#08x]"), hr));
  }

  return hr;
}

}  // namespace omaha
