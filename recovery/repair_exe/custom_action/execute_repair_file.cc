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
// Verifies and executes the repair file for the MSP custom action.

#include "omaha/recovery/repair_exe/custom_action/execute_repair_file.h"
#include <shlobj.h>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/string.h"
#include "omaha/common/system.h"
#include "omaha/common/utils.h"

namespace omaha {

namespace {

// TODO(omaha): Add a parameter to specify the process working directory
HRESULT ExecuteFile(const CString& filename, const CString& args) {
  HRESULT hr = System::ShellExecuteProcess(filename, args, NULL, NULL);

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ExecuteFile - failed to exec][%s][%s][0x%08x]"),
                  filename, args, hr));
    return hr;
  }

  return S_OK;
}

HRESULT GetDir(int csidl, const CString& path_tail, CString* dir) {
  ASSERT1(dir);

  CString path;
  HRESULT hr = GetFolderPath(csidl, &path);
  if (FAILED(hr)) {
    return hr;
  }
  if (!::PathAppend(CStrBuf(path, MAX_PATH), path_tail)) {
    return E_FAIL;
  }
  dir->SetString(path);

  // Try to create the directory. Continue if the directory can't be created.
  hr = CreateDir(path, NULL);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[GetDir failed to create dir][%s][0x%08x]"), path, hr));
  }
  return S_OK;
}

// Creates machine wide goopdate install dir: "Program Files/Google/Update".
CString GetMachineGoopdateInstallDir() {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_PROGRAM_FILES,
                           CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                           &path)));
  return path;
}

// Copies the file to "%ProgramFiles%\Google\Update\".
// Assumes %ProgramFiles% is secure, which it should be unless permissions have
// been changed.
// Note: Cannot use the temp dir because MSI uses the logon user's environment
// and the user's temp dir is unlikely to be secure.
HRESULT CopyToSecureLocation(const CString& source, CString* new_location) {
  if (source.IsEmpty() || !new_location) {
    return E_INVALIDARG;
  }

  CString filename = GetFileFromPath(source);
  *new_location = GetMachineGoopdateInstallDir();
  if (!::PathAppend(CStrBuf(*new_location, MAX_PATH), filename)) {
    return HRESULTFromLastError();
  }

  return File::Copy(source, *new_location, true);
}

}  // namespace

HRESULT VerifyIsValidRepairFile(const CString& filename);

// Assumes it is called elevated or with admin permissions.
// Copies the file to a secure location, before verifying and executing it.
// When the file is an Omaha metainstaller, it is important that the temp
// directory when elevated is also secure in order to maintain the security
// of all files being executed while elevated.
HRESULT VerifyFileAndExecute(const CString& filename, const CString& args) {
  UTIL_LOG(L1, (_T("[VerifyFileAndExecute][%s][%s]"), filename, args));

  CString secure_filename;
  HRESULT hr = CopyToSecureLocation(filename, &secure_filename);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[CopyToSecureLocation failed][error 0x%08x]"), hr));
    return hr;
  }

  hr = omaha::VerifyIsValidRepairFile(secure_filename);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[VerifyIsValidRepairFile failed][error 0x%08x]"), hr));
    return hr;
  }

  hr = ExecuteFile(secure_filename, args);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ExecuteFile failed][error 0x%08x]"), hr));
    return hr;
  }

  // Use the API directly rather than File::DeleteAfterReboot() because
  // File::DeleteAfterReboot() moves the file immediately, causing the
  // GetFileVersionInfoSize call in the metainstaller to fail.
  // Because this is always run as SYSTEM, the delayed delete will succeed.
  VERIFY1(::MoveFileEx(secure_filename, NULL, MOVEFILE_DELAY_UNTIL_REBOOT));

  return S_OK;
}

}  // namespace omaha
