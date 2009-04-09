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
#include "omaha/common/app_util.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/setup/setup_files.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const int kNumberOfLanguageDlls = 54;
const int kNumberOfRequiredFiles = 4;
const int kNumberOfOptionalFiles = 2;
const int kNumberOfInstalledRequiredFiles =
    kNumberOfLanguageDlls + kNumberOfRequiredFiles;
// FindFiles returns "." and ".." in addition to the actual files.
const int kExtraFilesReturnedByFindFiles = 2;
const int kExpectedFilesReturnedByFindFiles =
    kNumberOfInstalledRequiredFiles + kNumberOfOptionalFiles +
    kExtraFilesReturnedByFindFiles;

const TCHAR kFutureVersionString[] = _T("9.8.7.6");
const ULONGLONG kFutureVersion = 0x0009000800070006;

}  // namespace

void CopyGoopdateFiles(const CString& omaha_path, const CString& version) {
  const CString version_path = ConcatenatePath(omaha_path, version);

  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("GoogleUpdate.exe")),
      ConcatenatePath(omaha_path, _T("GoogleUpdate.exe")),
      false));

  ASSERT_SUCCEEDED(CreateDir(version_path, NULL));

  const TCHAR* files[] = {_T("GoogleUpdate.exe"),
                          _T("GoogleUpdateHelper.msi"),
                          _T("GoogleCrashHandler.exe"),
                          _T("goopdate.dll"),
                          _T("GoopdateBho.dll"),
                          ACTIVEX_FILENAME};
  for (size_t i = 0; i < arraysize(files); ++i) {
    ASSERT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        files[i]),
        ConcatenatePath(version_path, files[i]),
        false));
  }

  ASSERT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?.dll"),
                                       false));
  ASSERT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?\?.dll"),
                                       false));
  ASSERT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
                                       version_path,
                                       _T("goopdateres_\?\?-\?\?.dll"),
                                       false));
  ASSERT_SUCCEEDED(File::CopyWildcards(app_util::GetCurrentModuleDirectory(),
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

  void SetUp() {
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

  void InstallHelper(const CString& omaha_path) {
    const CString version_path = ConcatenatePath(omaha_path,
                                                 kFutureVersionString);

    ASSERT_EQ(kNumberOfInstalledRequiredFiles,
              setup_files_->core_program_files_.size());
    ASSERT_EQ(kNumberOfOptionalFiles, setup_files_->optional_files_.size());

    DeleteDirectory(version_path);
    ASSERT_FALSE(File::IsDirectory(version_path));

    // Fake the version
    ULONGLONG module_version = GetVersion();
    InitializeVersion(kFutureVersion);

    EXPECT_SUCCEEDED(setup_files_->Install());

    EXPECT_TRUE(File::Exists(ConcatenatePath(omaha_path,
                             _T("GoogleUpdate.exe"))));

    EXPECT_TRUE(File::IsDirectory(version_path));

    std::vector<CString> files;
    EXPECT_SUCCEEDED(FindFiles(version_path, _T("*.*"), &files));
    ASSERT_EQ(kExpectedFilesReturnedByFindFiles, files.size());
    int file_index = kExtraFilesReturnedByFindFiles;
    EXPECT_STREQ(_T("GoogleCrashHandler.exe"), files[file_index++]);
    EXPECT_STREQ(_T("GoogleUpdate.exe"), files[file_index++]);
    EXPECT_STREQ(_T("GoogleUpdateHelper.msi"), files[file_index++]);
    EXPECT_STREQ(_T("goopdate.dll"), files[file_index++]);
    EXPECT_STREQ(_T("GoopdateBho.dll"), files[file_index++]);
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
    EXPECT_STREQ(_T("goopdateres_or.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_pl.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_pt-BR.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_pt-PT.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ro.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ru.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sk.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sl.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_sv.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ta.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_te.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_th.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_tr.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_uk.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_ur.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_vi.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_zh-CN.dll"), files[file_index++]);
    EXPECT_STREQ(_T("goopdateres_zh-TW.dll"), files[file_index++]);
    EXPECT_STREQ(ACTIVEX_FILENAME, files[file_index++]);

    EXPECT_SUCCEEDED(DeleteDirectory(version_path));

    InitializeVersion(module_version);
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
                          _T("goopdate.dll"));
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
                                 _T("goopdate.dll"));
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
                                 _T("GoopdateBho.dll"));
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
  CString shell_path = ConcatenatePath(omaha_path_, _T("GoogleUpdate.exe"));
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
  CString shell_path = ConcatenatePath(omaha_path_, _T("GoogleUpdate.exe"));
  ASSERT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  ASSERT_FALSE(File::Exists(shell_path));

  // Does not check the version.
  EXPECT_TRUE(setup_files_->ShouldOverinstallSameVersion());

  EXPECT_SUCCEEDED(
      DeleteDirectory(ConcatenatePath(omaha_path_, kFutureVersionString)));
}

TEST_F(SetupFilesMachineTest, Install_NotOverInstall) {
  if (vista_util::IsUserAdmin()) {
    InstallHelper(omaha_path_);
  } else {
    // This method expects to be called elevated for machine installs.
    ExpectAsserts expect_asserts;
    EXPECT_EQ(GOOPDATE_E_ACCESSDENIED_COPYING_CORE_FILES,
              setup_files_->Install());
  }
}

TEST_F(SetupFilesUserTest, Install_NotOverInstall) {
  InstallHelper(omaha_path_);
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsNewer) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.2.x_newer")),
      _T("GoogleUpdate.exe"));
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_FALSE(should_copy);
  EXPECT_TRUE(already_exists);
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsSame) {
  CString target_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                        _T("GoogleUpdate.exe"));
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;

  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_EQ(expected_is_overinstall_, should_copy);
  EXPECT_TRUE(already_exists);

  if (ShouldRunLargeTest()) {
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
}

TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsOlderMinor) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.1.x")),
      _T("GoogleUpdate.exe"));
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}

// The 1.2.x directory will not always have an older GoogleUpdate.exe than the
// saved version that we use for official builds.
#if !OFFICIAL_BUILD
TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingIsOlderSameMinor) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("omaha_1.2.x")),
      _T("GoogleUpdate.exe"));
  ASSERT_TRUE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_TRUE(already_exists);
}
#endif

// Assumes LongRunningSilent.exe does not have a version resource.
TEST_F(SetupFilesUserTest, ShouldCopyShell_ExistingHasNoVersion) {
  CString target_path = ConcatenatePath(
      ConcatenatePath(exe_parent_dir_, _T("does_not_shutdown")),
      _T("GoogleUpdate.exe"));
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
      _T("GoogleUpdate.exe"));
  ASSERT_FALSE(File::Exists(target_path));
  bool should_copy = false;
  bool already_exists = false;
  EXPECT_SUCCEEDED(ShouldCopyShell(target_path, &should_copy, &already_exists));
  EXPECT_TRUE(should_copy);
  EXPECT_FALSE(already_exists);
}

}  // namespace omaha
