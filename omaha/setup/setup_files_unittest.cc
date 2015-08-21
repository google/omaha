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

#include <vector>
#include "base/scoped_ptr.h"
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
const int kNumberOfMetainstallerFiles = 1;
const int kNumberOfOptionalFiles = 4;
const int kNumberOfInstalledRequiredFiles =
    kNumberOfLanguageDlls + kNumberOfCoreFiles;
// FindFiles returns "." and ".." in addition to the actual files.
const int kExtraFilesReturnedByFindFiles = 2;
const int kExpectedFilesReturnedByFindFiles =
    kNumberOfInstalledRequiredFiles + kNumberOfMetainstallerFiles +
    kNumberOfOptionalFiles + kExtraFilesReturnedByFindFiles;

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

  const TCHAR* files[] = {kCrashHandlerFileName,
                          kCrashHandler64FileName,
                          kOmahaShellFileName,
                          kHelperInstallerName,
                          kOmahaCOMRegisterShell64,
                          kOmahaDllName,
                          kOmahaMetainstallerFileName,
                          kOmahaBrokerFileName,
                          kOmahaOnDemandFileName,
                          kOmahaWebPluginFileName,
// TODO(omaha3): Enable once this is being built.
#if 0
                          _T("GoopdateBho.dll"),
#endif
                          UPDATE_PLUGIN_FILENAME,
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
    ASSERT_EQ(kNumberOfMetainstallerFiles,
              setup_files_->metainstaller_files_.size());
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
    EXPECT_STREQ(kCrashHandlerFileName, files[file_index++]);
    EXPECT_STREQ(kCrashHandler64FileName, files[file_index++]);
    EXPECT_STREQ(kOmahaShellFileName, files[file_index++]);
    EXPECT_STREQ(kOmahaBrokerFileName, files[file_index++]);
    EXPECT_STREQ(kOmahaCOMRegisterShell64, files[file_index++]);
    EXPECT_STREQ(kHelperInstallerName, files[file_index++]);
    EXPECT_STREQ(kOmahaOnDemandFileName, files[file_index++]);
    EXPECT_STREQ(kOmahaMetainstallerFileName, files[file_index++]);
    EXPECT_STREQ(kOmahaWebPluginFileName, files[file_index++]);
    EXPECT_STREQ(kOmahaDllName, files[file_index++]);
// TODO(omaha3): Enable as this is built.
#if 0
    EXPECT_STREQ(_T("GoopdateBho.dll"), files[file_index++]);
#endif
    EXPECT_STREQ(_T("goopdateres_am.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ar.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_bg.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_bn.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ca.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_cs.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_da.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_de.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_el.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_en-GB.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_en.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_es-419.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_es.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_et.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_fa.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_fi.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_fil.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_fr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_gu.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_hi.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_hr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_hu.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_id.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_is.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_it.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_iw.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ja.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_kn.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ko.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_lt.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_lv.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ml.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_mr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ms.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_nl.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_no.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_pl.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_pt-BR.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_pt-PT.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ro.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ru.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sk.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sl.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sv.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sw.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ta.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_te.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_th.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_tr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_uk.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ur.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_vi.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_zh-CN.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_zh-TW.dll"), files[file_index++]);
    EXPECT_STREQ(UPDATE_PLUGIN_FILENAME, files[file_index++]);
    EXPECT_STREQ(kPSFileNameMachine, files[file_index++]);
    EXPECT_STREQ(kPSFileNameMachine64, files[file_index++]);
    EXPECT_STREQ(kPSFileNameUser, files[file_index++]);
    EXPECT_STREQ(kPSFileNameUser64, files[file_index++]);

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
  scoped_ptr<SetupFiles> setup_files_;

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

TEST_F(SetupFilesUserTest,
       ShouldOverinstallSameVersion_SameVersionFilesMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));
  ASSERT_SUCCEEDED(
      DeleteDirectory(ConcatenatePath(omaha_path_, this_version_)));
  CString file_path = ConcatenatePath(
                          ConcatenatePath(omaha_path_, this_version_),
                          kOmahaDllName);
  ASSERT_FALSE(File::Exists(file_path));

  EXPECT_TRUE(setup_files_->ShouldOverinstallSameVersion());
}

TEST_F(SetupFilesUserTest,
       ShouldOverinstallSameVersion_SameVersionFilesPresent) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);

  EXPECT_FALSE(setup_files_->ShouldOverinstallSameVersion());
}

TEST_F(SetupFilesUserTest,
       ShouldOverinstallSameVersion_SameVersionRequiredFileMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString path = ConcatenatePath(ConcatenatePath(omaha_path_, this_version_),
                                 kOmahaDllName);
  ASSERT_SUCCEEDED(File::Remove(path));
  ASSERT_FALSE(File::Exists(path));

  EXPECT_TRUE(setup_files_->ShouldOverinstallSameVersion());
}

TEST_F(SetupFilesUserTest,
       ShouldOverinstallSameVersion_SameVersionOptionalFileMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString path = ConcatenatePath(ConcatenatePath(omaha_path_, this_version_),
                                 UPDATE_PLUGIN_FILENAME);
  ASSERT_SUCCEEDED(File::Remove(path));
  ASSERT_FALSE(File::Exists(path));

  EXPECT_TRUE(setup_files_->ShouldOverinstallSameVersion());
}

TEST_F(SetupFilesUserTest,
       ShouldOverinstallSameVersion_SameVersionShellMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    this_version_));

  CopyGoopdateFiles(omaha_path_, this_version_);
  CString shell_path = ConcatenatePath(omaha_path_, kOmahaShellFileName);
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  EXPECT_TRUE(setup_files_->ShouldOverinstallSameVersion());
}

TEST_F(SetupFilesUserTest,
       ShouldOverinstallSameVersion_NewerVersionShellMissing) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  CopyGoopdateFiles(omaha_path_, kFutureVersionString);
  CString shell_path = ConcatenatePath(omaha_path_, kOmahaShellFileName);
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  // Does not check the version.
  EXPECT_TRUE(setup_files_->ShouldOverinstallSameVersion());

  EXPECT_SUCCEEDED(
      DeleteDirectory(ConcatenatePath(omaha_path_, kFutureVersionString)));
}

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

  if (!ShouldRunLargeTest()) {
    return;
  }

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
