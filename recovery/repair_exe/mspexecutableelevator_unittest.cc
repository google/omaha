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
// Unit tests for msp_executable_elevator.
//
// Note for Windows Vista and later: These tests will fail because the Msi*
// methods return an error because they do not elevate due to the UI level NONE.
// Enabling the UI would cause UAC prompts and the creation of restore points.
// The workaround is to run from an administrator command prompt.

#include <windows.h>
#include <msi.h>
#include "omaha/common/app_util.h"
#include "omaha/common/constants.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/recovery/repair_exe/mspexecutableelevator.h"
#include "omaha/setup/msi_test_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Note: For some reason, the product ID GUIDs are swizzled in the registry.
extern const TCHAR kMsiProductPatchesKey[] =
    _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\")
    _T("Installer\\UserData\\S-1-5-18\\Products\\")
    _T("93BAD29AC2E44034A96BCB446EB8552E\\Patches");

void InstallMsi() {
  CString msi_path(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(msi_path, MAX_PATH),
                           kHelperInstallerName));
  ::MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

  UINT res = ::MsiInstallProduct(msi_path, _T(""));

  if (vista_util::IsUserAdmin()) {
    if (ERROR_SUCCESS != res) {
      EXPECT_EQ(ERROR_PRODUCT_VERSION, res);
      // The product may already be installed. Force a reinstall of everything.
      res = ::MsiInstallProduct(msi_path,
                                _T("REINSTALL=ALL REINSTALLMODE=vamus"));
    }
    EXPECT_EQ(ERROR_SUCCESS, res);
  } else {
    if (IsMsiHelperInstalled()) {
      EXPECT_EQ(ERROR_INSTALL_PACKAGE_REJECTED, res);
    } else {
      EXPECT_EQ(ERROR_INSTALL_FAILURE, res);
    }
  }
}

void RemoveMsi() {
  ::MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
  UINT res = ::MsiConfigureProduct(kHelperInstallerProductGuid,
                                   INSTALLLEVEL_DEFAULT,
                                   INSTALLSTATE_ABSENT);
  if (vista_util::IsUserAdmin()) {
    EXPECT_TRUE((ERROR_SUCCESS == res) ||
                ((ERROR_UNKNOWN_PRODUCT == res) && !IsMsiHelperInstalled()));
  } else {
    if (IsMsiHelperInstalled()) {
      EXPECT_EQ(ERROR_INSTALL_FAILURE, res);
    } else {
      EXPECT_EQ(ERROR_UNKNOWN_PRODUCT, res);
    }
  }
}

HRESULT ExecuteGoogleSignedExeWithCorrectPatchInfo(const TCHAR* executable,
                                                   const TCHAR* arguments,
                                                   HANDLE* process) {
  return msp_executable_elevator::ExecuteGoogleSignedExe(
                                      executable,
                                      arguments,
                                      kHelperInstallerProductGuid,
                                      kHelperPatchGuid,
                                      kHelperPatchName,
                                      process);
}

// Base class for tests that expect the MSI to be installed.
// The elevation mechanism will not be installed when these test complete.
class RepairGoopdateWithMsiInstalledTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    InstallMsi();
  }

  static void TearDownTestCase() {
    RemoveMsi();
  }
};

// Base class for tests that expect the MSI not to be installed.
// The elevation mechanism will not be installed when these test complete.
class RepairGoopdateWithoutMsiInstalledTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    RemoveMsi();
  }
};

TEST_F(RepairGoopdateWithMsiInstalledTest,
       ExecuteGoogleSignedExe_RepairFileDoesNotExist) {
  CString repair_file(_T("no_such_file.exe"));
  HANDLE process = NULL;
  HRESULT hr = ExecuteGoogleSignedExeWithCorrectPatchInfo(repair_file,
                                                          _T(""),
                                                          &process);
  if (vista_util::IsUserAdmin() || IsMsiHelperInstalled()) {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), hr);
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND), hr);
  }

  EXPECT_TRUE(NULL == process);
}

TEST_F(RepairGoopdateWithMsiInstalledTest,
       ExecuteGoogleSignedExe_UnsignedFile) {
  CString repair_file(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(repair_file, MAX_PATH),
              _T("GoogleUpdate_unsigned.exe")));
  HANDLE process = NULL;
  HRESULT hr = ExecuteGoogleSignedExeWithCorrectPatchInfo(repair_file,
                                                          _T(""),
                                                          &process);
  if (vista_util::IsUserAdmin() || IsMsiHelperInstalled()) {
    EXPECT_EQ(TRUST_E_NOSIGNATURE, hr);
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND), hr);
  }
  EXPECT_TRUE(NULL == process);
}

// This valid repair file saves the arguments passed to it to a file.
TEST_F(RepairGoopdateWithMsiInstalledTest,
       ExecuteGoogleSignedExe_ValidRepairFile) {
  const TCHAR kArgs[] = _T("These /are the args.");
  CString repair_file(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(repair_file, MAX_PATH),
              _T("unittest_support\\SaveArguments.exe")));
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  CString saved_arguments_file_path =
      program_files_path + _T("\\Google\\Update\\saved_arguments.txt");

  ::DeleteFile(saved_arguments_file_path);

  bool is_msi_installed = IsMsiHelperInstalled();

  if (vista_util::IsUserAdmin() || is_msi_installed) {
    if (vista_util::IsUserAdmin()) {
      EXPECT_FALSE(File::Exists(saved_arguments_file_path));
    }

    // Verify that no patch is installed.
    EXPECT_TRUE(RegKey::HasNativeKey(kMsiProductPatchesKey));
    RegKey product_patches_key;
    EXPECT_SUCCEEDED(product_patches_key.Open(kMsiProductPatchesKey,
                                              KEY_READ | KEY_WOW64_64KEY));
    EXPECT_EQ(0, product_patches_key.GetSubkeyCount());

    HANDLE process = NULL;
    EXPECT_SUCCEEDED(ExecuteGoogleSignedExeWithCorrectPatchInfo(repair_file,
                                                                kArgs,
                                                                &process));
    EXPECT_TRUE(NULL == process);

    // Verify that patch was uninstalled.
    // GetSubkeyCount fails if we don't re-open the key.
    EXPECT_SUCCEEDED(product_patches_key.Open(kMsiProductPatchesKey,
                                              KEY_READ | KEY_WOW64_64KEY));
    EXPECT_EQ(0, product_patches_key.GetSubkeyCount());

    bool is_found = false;
    for (int tries = 0; tries < 100 && !is_found; ++tries) {
      ::Sleep(50);
      is_found = File::Exists(saved_arguments_file_path);
    }
    ASSERT_TRUE(is_found);

    scoped_hfile file;
    for (int tries = 0; tries < 100 && !valid(file); ++tries) {
      ::Sleep(50);
      reset(file, ::CreateFile(saved_arguments_file_path,
                               GENERIC_READ,
                               0,                        // do not share
                               NULL,                     // default security
                               OPEN_EXISTING,            // existing file only
                               FILE_ATTRIBUTE_NORMAL,
                               NULL));                   // no template
    }
    ASSERT_TRUE(valid(file));

    const int kBufferLen = 50;
    TCHAR buffer[kBufferLen + 1] = {0};
    DWORD bytes_read = 0;

    EXPECT_TRUE(::ReadFile(get(file),
                           buffer,
                           kBufferLen * sizeof(TCHAR),
                           &bytes_read,
                           NULL));
    EXPECT_EQ(0, bytes_read % sizeof(TCHAR));
    buffer[bytes_read / sizeof(TCHAR)] = _T('\0');
    EXPECT_STREQ(kArgs, buffer);

    reset(file);

    bool succeeded = !!::DeleteFile(saved_arguments_file_path);
    if (vista_util::IsUserAdmin()) {
      EXPECT_TRUE(succeeded);
    }
  } else {
    bool expected_file_exists = File::Exists(saved_arguments_file_path);
    HANDLE process = NULL;
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND),
              ExecuteGoogleSignedExeWithCorrectPatchInfo(repair_file,
                                                         kArgs,
                                                         &process));
    EXPECT_TRUE(NULL == process);

    // We can't force the file to be deleted, so make sure it wasn't created
    // or deleted by the above method.
    EXPECT_EQ(expected_file_exists, File::Exists(saved_arguments_file_path));
  }
}

TEST_F(RepairGoopdateWithoutMsiInstalledTest,
       ExecuteGoogleSignedExe_MsiNotInstalled) {
  if (!vista_util::IsUserAdmin() && IsMsiHelperInstalled()) {
    std::wcout << _T("\tThis test did not run because the user is not an ")
                  _T("admin and the MSI is already installed.") << std::endl;
    return;
  }

  CString repair_file(_T("notepad.exe"));
  HANDLE process = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND),
            ExecuteGoogleSignedExeWithCorrectPatchInfo(repair_file,
                                                       _T(""),
                                                       &process));
  EXPECT_TRUE(NULL == process);
}

}  // namespace omaha
