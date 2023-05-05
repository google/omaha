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

#include <memory>
#include <set>
#include <vector>

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/setup/setup_files.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

// TODO(omaha3): Update the numbers in the else block as we build more files.
// Eventually use the original values in the if block.
const int kNumberOfLanguageDlls = 55;
const int kNumberOfCoreFiles = 10;
const int kNumberOfOptionalFiles = 2;
const int kNumberOfInstalledRequiredFiles =
    kNumberOfLanguageDlls + kNumberOfCoreFiles;
// FindFiles returns "." and ".." in addition to the actual files.
const int kExtraFilesReturnedByFindFiles = 2;
const int kExpectedFilesReturnedByFindFiles = kNumberOfInstalledRequiredFiles +
                                              kNumberOfOptionalFiles +
                                              kExtraFilesReturnedByFindFiles;

const TCHAR kFutureVersionString[] = _T("9.8.7.6");
const ULONGLONG kFutureVersion = 0x0009000800070006;

}  // namespace

void CopyGoopdateFiles(const CString& omaha_path, const CString& version) {
  EXPECT_SUCCEEDED(CreateDir(omaha_path, NULL));
  const CString version_path = ConcatenatePath(omaha_path, version);

  EXPECT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      kOmahaShellFileName),
      ConcatenatePath(omaha_path, kOmahaShellFileName),
      false));

  EXPECT_SUCCEEDED(CreateDir(version_path, NULL));

  const TCHAR* files[] = {kOmahaCoreFileName,
                          kCrashHandlerFileName,
                          kCrashHandler64FileName,
                          kOmahaShellFileName,
                          kOmahaCOMRegisterShell64,
                          kOmahaDllName,
                          kOmahaBrokerFileName,
                          kOmahaOnDemandFileName,
                          kPSFileNameMachine,
                          kPSFileNameMachine64,
                          kPSFileNameUser,
                          kPSFileNameUser64,
                          };
  for (size_t i = 0; i < arraysize(files); ++i) {
    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        files[i]),
        ConcatenatePath(version_path, files[i]),
        false)) << _T("Failed copying ") << files[i];
  }

  EXPECT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?.dll"),
                                       false));
  EXPECT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?\?.dll"),
                                       false));
  EXPECT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?-\?\?.dll"),
                                       false));
  EXPECT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?-\?\?\?.dll"),
                                       false));
}

class SetupFilesTest : public testing::Test {
 protected:
  explicit SetupFilesTest(bool is_machine)
      : is_machine_(is_machine),
        omaha_path_(is_machine ?
            GetGoogleUpdateMachinePath() : GetGoogleUpdateUserPath()),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  static void SetUpTestCase() {
    exe_parent_dir_ = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                      _T("unittest_support\\"));

    this_version_ = GetVersionString();

    expected_is_overinstall_ = !OFFICIAL_BUILD;
#ifdef DEBUG
    if (RegKey::HasValue(MACHINE_REG_UPDATE_DEV, kRegValueNameOverInstall)) {
      DWORD value = 0;
      EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                        kRegValueNameOverInstall,
                                        &value));
      expected_is_overinstall_ = value != 0;
    }
#endif
  }

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    // Do not override HKLM because it contains the CSIDL_* definitions.
    OverrideSpecifiedRegistryHives(hive_override_key_name_, false, true);

    setup_files_.reset(new SetupFiles(is_machine_));

    ASSERT_HRESULT_SUCCEEDED(setup_files_->Init());
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }

  static bool IsOlderShellVersionCompatible(ULONGLONG version) {
    return SetupFiles::IsOlderShellVersionCompatible(version);
  }

  // Assumes the executable version has been changed to the future version.
  void InstallHelper(const CString& omaha_path) {
    const CString version_path = ConcatenatePath(omaha_path,
                                                 kFutureVersionString);

    ASSERT_EQ(kNumberOfInstalledRequiredFiles,
              setup_files_->core_program_files_.size());
    ASSERT_EQ(kNumberOfOptionalFiles, setup_files_->optional_files_.size());

    DeleteDirectory(version_path);
    ASSERT_FALSE(File::IsDirectory(version_path));

    EXPECT_SUCCEEDED(setup_files_->Install());

    EXPECT_TRUE(File::Exists(ConcatenatePath(omaha_path,
                             kOmahaShellFileName)));

    EXPECT_TRUE(File::IsDirectory(version_path));

    std::vector<CString> files;
    EXPECT_SUCCEEDED(FindFiles(version_path, _T("*.*"), &files));
    ASSERT_EQ(kExpectedFilesReturnedByFindFiles, files.size());
    int file_index = kExtraFilesReturnedByFindFiles;

    std::set<CString> extra_files;
    for (int i = file_index; i < kExpectedFilesReturnedByFindFiles; ++i) {
      extra_files.insert(files[i]);
    }
    EXPECT_EQ(
        extra_files.size(),
        kExpectedFilesReturnedByFindFiles - kExtraFilesReturnedByFindFiles);

    EXPECT_NE(
        extra_files.find(kCrashHandlerFileName), extra_files.end());
    EXPECT_NE(
        extra_files.find(kCrashHandler64FileName), extra_files.end());
    EXPECT_NE(
        extra_files.find(kOmahaShellFileName), extra_files.end());
    EXPECT_NE(
        extra_files.find(kOmahaBrokerFileName), extra_files.end());
    EXPECT_NE(
        extra_files.find(kOmahaCOMRegisterShell64), extra_files.end());
    EXPECT_NE(
        extra_files.find(kOmahaCoreFileName), extra_files.end());
    EXPECT_NE(
        extra_files.find(kOmahaOnDemandFileName), extra_files.end());
    EXPECT_NE(
        extra_files.find(kOmahaDllName), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_am.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ar.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_bg.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_bn.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ca.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_cs.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_da.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_de.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_el.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_en-GB.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_en.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_es-419.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_es.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_et.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_fa.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_fi.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_fil.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_fr.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_gu.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_hi.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_hr.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_hu.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_id.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_is.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_it.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_iw.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ja.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_kn.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ko.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_lt.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_lv.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ml.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_mr.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ms.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_nl.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_no.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_pl.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_pt-BR.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_pt-PT.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ro.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ru.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_sk.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_sl.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_sr.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_sv.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_sw.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ta.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_te.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_th.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_tr.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_uk.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_ur.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_vi.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_zh-CN.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(_T("goopdateres_zh-TW.dll")), extra_files.end());
    EXPECT_NE(
        extra_files.find(kPSFileNameMachine), extra_files.end());
    EXPECT_NE(
        extra_files.find(kPSFileNameMachine64), extra_files.end());
    EXPECT_NE(
        extra_files.find(kPSFileNameUser), extra_files.end());
    EXPECT_NE(
        extra_files.find(kPSFileNameUser64), extra_files.end());

    EXPECT_SUCCEEDED(DeleteDirectory(version_path));
  }

  HRESULT ShouldCopyShell(const CString& shell_install_path,
                          bool* should_copy,
                          bool* already_exists) const {
    return setup_files_->ShouldCopyShell(shell_install_path,
                                         should_copy,
                                         already_exists);
  }

  const bool is_machine_;
  const CString omaha_path_;
  const CString hive_override_key_name_;
  std::unique_ptr<SetupFiles> setup_files_;

  static CString exe_parent_dir_;
  static CString this_version_;
  static bool expected_is_overinstall_;
};

CString SetupFilesTest::exe_parent_dir_;
CString SetupFilesTest::this_version_;
bool SetupFilesTest::expected_is_overinstall_;

class SetupFilesMachineTest : public SetupFilesTest {
 protected:
  SetupFilesMachineTest()
    : SetupFilesTest(true) {
  }
};

class SetupFilesUserTest : public SetupFilesTest {
 protected:
  SetupFilesUserTest()
    : SetupFilesTest(false) {
  }
};

// "NotOverInstall" refers to there not being files in the directory.
// should_over_install/overwrite will be true for unofficial builds.
TEST_F(SetupFilesMachineTest, Install_NotOverInstall) {
  if (vista_util::IsUserAdmin()) {
    // Fake the version
    const ULONGLONG module_version = GetVersion();
    InitializeVersion(kFutureVersion);

    InstallHelper(omaha_path_);

    InitializeVersion(module_version);
  } else {
    // This method expects to be called elevated for machine installs.
    ExpectAsserts expect_asserts;
    EXPECT_EQ(GOOPDATE_E_ACCESSDENIED_COPYING_CORE_FILES,
              setup_files_->Install());
  }
}

TEST_F(SetupFilesUserTest, Install_NotOverInstall) {
  // Fake the version
  const ULONGLONG module_version = GetVersion();
  InitializeVersion(kFutureVersion);

  InstallHelper(omaha_path_);

  InitializeVersion(module_version);
}

// TODO(omaha3): Need a 1.3.x_newer directory.
TEST_F(SetupFilesUserTest, DISABLED_ShouldCopyShell_ExistingIsNewer) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.3.x_newer")),
      kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_FALSE(should_copy);
  EXPECT_TRUE(already_exists);
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsSame) {
  CString target_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                        kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;

  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_EQ(expected_is_overinstall_, should_copy);
  EXPECT_TRUE(already_exists);

  // Override OverInstall to test official behavior on non-official builds.

  DWORD existing_overinstall(0);
  bool had_existing_overinstall = SUCCEEDED(RegKey::GetValue(
                                                MACHINE_REG_UPDATE_DEV,
                                                kRegValueNameOverInstall,
                                                &existing_overinstall));

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueNameOverInstall,
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(
      ShouldCopyShell(target_path, &should_copy, &already_exists));
#ifdef DEBUG
  EXPECT_FALSE(should_copy);
#else
  EXPECT_EQ(expected_is_overinstall_, should_copy);
#endif
  EXPECT_TRUE(already_exists);

  // Restore "overinstall"
  if (had_existing_overinstall) {
    EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                      kRegValueNameOverInstall,
                                      existing_overinstall));
  } else {
    EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                         kRegValueNameOverInstall));
  }
}

TEST_F(SetupFilesUserTest, IsOlderShellVersionCompatible_Compatible) {
  EXPECT_TRUE(IsOlderShellVersionCompatible(MAKEDLLVERULL(1, 3, 26, 1)));
  EXPECT_TRUE(IsOlderShellVersionCompatible(_UI64_MAX));
}

TEST_F(SetupFilesUserTest, IsOlderShellVersionCompatible_Incompatible) {
  EXPECT_FALSE(IsOlderShellVersionCompatible(MAKEDLLVERULL(1, 3, 21, 103)));
  EXPECT_FALSE(IsOlderShellVersionCompatible(MAKEDLLVERULL(1, 2, 183, 21)));
  EXPECT_FALSE(IsOlderShellVersionCompatible(MAKEDLLVERULL(1, 2, 183, 9)));
  EXPECT_FALSE(IsOlderShellVersionCompatible(MAKEDLLVERULL(1, 2, 131, 7)));
  EXPECT_FALSE(IsOlderShellVersionCompatible(MAKEDLLVERULL(1, 2, 131, 5)));
  EXPECT_FALSE(IsOlderShellVersionCompatible(1));
  EXPECT_FALSE(IsOlderShellVersionCompatible(0));
}

TEST_F(SetupFilesUserTest,
       ShouldCopyShell_ExistingIsOlderButCompatible_1_2_131_7) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.2.131.7_shell")),
      kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}

TEST_F(SetupFilesUserTest,
       ShouldCopyShell_ExistingIsOlderButCompatible_1_2_183_9) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.2.183.9_shell")),
      kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsOlderMinor) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.2.x")),
      kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsOlderSameMinor) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.3.x")),
      kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}

// Assumes LongRunningSilent.exe does not have a version resource.
TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingHasNoVersion) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("does_not_shutdown")),
      kOmahaShellFileName);
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  ExpectAsserts expect_asserts;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_NoExistingFile) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("no_such_dir")),
      kOmahaShellFileName);
  ASSERT_FALSE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_FALSE(already_exists);
}

}  // namespace omaha
