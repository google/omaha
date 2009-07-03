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
// Unit tests for the Omaha repair mechanism.

#include "omaha/common/app_util.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/vistautil.h"
#include "omaha/recovery/repair_exe/repair_goopdate.h"
#include "omaha/setup/msi_test_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR kArgumentSavingExecutableRelativePath[] =
    _T("unittest_support\\SaveArguments.exe");

extern const TCHAR kMsiProductPatchesKey[];
void InstallMsi();
void RemoveMsi();

class RepairGoopdateTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    valid_path = app_util::GetCurrentModuleDirectory();
    VERIFY1(::PathAppend(CStrBuf(valid_path, MAX_PATH),
                         kArgumentSavingExecutableRelativePath));
  }

  static CString valid_path;
};

CString RepairGoopdateTest::valid_path;

TEST_F(RepairGoopdateTest, LaunchRepairFileElevated_UserInstance) {
  HRESULT hr = E_FAIL;
  EXPECT_FALSE(LaunchRepairFileElevated(false, valid_path, _T("/update"), &hr));
  EXPECT_SUCCEEDED(hr);
}

// This test only runs on pre-Windows Vista OSes.
TEST_F(RepairGoopdateTest, LaunchRepairFileElevated_MachineInstancePreVista) {
  if (!vista_util::IsVistaOrLater()) {
    HRESULT hr = E_FAIL;
    EXPECT_FALSE(LaunchRepairFileElevated(true, valid_path, _T(""), &hr));
    EXPECT_SUCCEEDED(hr);
  }
}

// This test only runs on Windows Vista and later OSes.
TEST_F(RepairGoopdateTest,
       LaunchRepairFileElevated_MachineInstanceVistaWithMsiInstalledValidFile) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tThis test did not run because it requires Windows ")
                  _T("Vista or later.") << std::endl;
    return;
  }

  const TCHAR kArgs[] = _T("/update");
  CString saved_arguments_file_path =
      _T("%PROGRAMFILES%\\Google\\Update\\saved_arguments.txt");
  EXPECT_SUCCEEDED(ExpandStringWithSpecialFolders(&saved_arguments_file_path));

  ::DeleteFile(saved_arguments_file_path);

  bool is_msi_installed = IsMsiHelperInstalled();

  if (vista_util::IsUserAdmin() || is_msi_installed) {
    if (vista_util::IsUserAdmin()) {
      EXPECT_FALSE(File::Exists(saved_arguments_file_path));
    }

    InstallMsi();

    // Verify that no patch is installed.
    EXPECT_TRUE(RegKey::HasNativeKey(kMsiProductPatchesKey));
    RegKey product_patches_key;
    EXPECT_SUCCEEDED(product_patches_key.Open(kMsiProductPatchesKey,
                                              KEY_READ | KEY_WOW64_64KEY));
    EXPECT_EQ(0, product_patches_key.GetSubkeyCount());

    HRESULT hr = E_FAIL;
    EXPECT_TRUE(LaunchRepairFileElevated(true, valid_path, kArgs, &hr));
    EXPECT_SUCCEEDED(hr);

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

    BOOL succeeded = ::DeleteFile(saved_arguments_file_path);
    if (vista_util::IsUserAdmin()) {
      EXPECT_TRUE(succeeded);
    }

    RemoveMsi();
  } else {
    const bool expected_file_exists = File::Exists(saved_arguments_file_path);
    HRESULT hr = E_FAIL;
    EXPECT_FALSE(LaunchRepairFileElevated(true, valid_path, kArgs, &hr));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND), hr);

    // We can't force the file to be deleted, so make sure it wasn't created
    // or deleted by the above method.
    EXPECT_EQ(expected_file_exists, File::Exists(saved_arguments_file_path));
  }
}

// This test only runs on Windows Vista and later OSes.
// The MSP to runs, but fails because the specified file (the unit tests)
// is not a valid repair file.
TEST_F(RepairGoopdateTest,
       LaunchRepairFileElevated_MachineInstanceVistaWithMsiInstalledBadFile) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tThis test did not run because it requires Windows ")
                  _T("Vista or later.") << std::endl;
    return;
  }

  if (vista_util::IsUserAdmin() || IsMsiHelperInstalled()) {
    InstallMsi();

    // Verify that no patch is installed.
    EXPECT_TRUE(RegKey::HasNativeKey(kMsiProductPatchesKey));
    RegKey product_patches_key;
    EXPECT_SUCCEEDED(product_patches_key.Open(kMsiProductPatchesKey,
                                              KEY_READ | KEY_WOW64_64KEY));
    EXPECT_EQ(0, product_patches_key.GetSubkeyCount());

    HRESULT hr = E_FAIL;
    EXPECT_FALSE(LaunchRepairFileElevated(true,
                                          app_util::GetCurrentModulePath(),
                                          _T("/update"),
                                          &hr));
    EXPECT_EQ(TRUST_E_NOSIGNATURE, hr);

    // Verify that patch was uninstalled.
    // GetSubkeyCount fails if we don't re-open the key.
    EXPECT_SUCCEEDED(product_patches_key.Open(kMsiProductPatchesKey,
                                              KEY_READ | KEY_WOW64_64KEY));
    EXPECT_EQ(0, product_patches_key.GetSubkeyCount());

    RemoveMsi();
  } else {
    HRESULT hr = E_FAIL;
    EXPECT_FALSE(LaunchRepairFileElevated(true,
                                          app_util::GetCurrentModulePath(),
                                          _T("/update"),
                                          &hr));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND), hr);
  }
}

// This test only runs on Windows Vista and later OSes.
TEST_F(RepairGoopdateTest,
       LaunchRepairFileElevated_MachineInstanceVistaWithoutMsiInstalled) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tThis test did not run because it requires Windows ")
                  _T("Vista or later.") << std::endl;
    return;
  }

  if (vista_util::IsUserAdmin()) {
    RemoveMsi();
  } else if (IsMsiHelperInstalled()) {
    std::wcout << _T("\tThis test did not run because the user is not an ")
                  _T("admin and the MSI is already installed.") << std::endl;
    return;
  }
  // else the user is not an admin but the MSI is not installed, so continue.

  HRESULT hr = E_FAIL;
  EXPECT_FALSE(LaunchRepairFileElevated(true, valid_path, _T("/update"), &hr));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATCH_TARGET_NOT_FOUND), hr);
}

}  // namespace omaha
