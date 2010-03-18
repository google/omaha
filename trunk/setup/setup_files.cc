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

#include "omaha/setup/setup_files.h"

#include <atlpath.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/app_util.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/scoped_current_directory.h"
#include "omaha/common/signaturevalidator.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/setup/setup_metrics.h"

namespace omaha {

namespace {

const int kNumberOfCreateServiceRetries = 5;
const int kSleepBetweenCreateServiceRetryMs = 200;

}  // namespace

SetupFiles::SetupFiles(bool is_machine)
: is_machine_(is_machine) {
  SETUP_LOG(L2, (_T("[SetupFiles::SetupFiles]")));
}

SetupFiles::~SetupFiles() {
  SETUP_LOG(L2, (_T("[SetupFiles::~SetupFiles]")));

  if (!saved_shell_path_.IsEmpty()) {
    // Delete the saved copy of the previous shell.
    VERIFY1(SUCCEEDED(File::Remove(saved_shell_path_)));
  }
}

HRESULT SetupFiles::Init() {
  SETUP_LOG(L2, (_T("[SetupFiles::Init]")));

  HRESULT hr = BuildFileLists();
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// We only do these checks for the same exact version. This is especially true
// when doing file comparisons, because the filenames as well as the number of
// files can change from version to version. An earlier version should not
// overinstall a newer version by mistake because it is checking for files that
// no longer exist in the new version.
bool SetupFiles::ShouldOverinstallSameVersion() {
  SETUP_LOG(L2, (_T("[SetupFiles::ShouldOverinstallSameVersion]")));

  CPath install_dir = goopdate_utils::BuildInstallDirectory(is_machine_,
                                                            GetVersionString());
  for (size_t i = 0 ; i < core_program_files_.size(); ++i) {
    CString full_path = ConcatenatePath(install_dir, core_program_files_[i]);
    if (full_path.IsEmpty()) {
      ASSERT1(false);
      return true;
    }
    if (!File::Exists(full_path)) {
      SETUP_LOG(L2, (_T("[core file missing - overinstall][%s]]"), full_path));
      return true;
    }
  }

  for (size_t i = 0 ; i < optional_files_.size(); ++i) {
    CString full_path = ConcatenatePath(install_dir, optional_files_[i]);
    if (full_path.IsEmpty()) {
      ASSERT1(false);
      return true;
    }
    if (!File::Exists(full_path)) {
      SETUP_LOG(L2, (_T("[optional file missing - overinstall][%s]]"),
                     full_path));
      return true;
    }
  }

  CString shell_path = goopdate_utils::BuildGoogleUpdateExePath(is_machine_);
  if (!File::Exists(shell_path)) {
    SETUP_LOG(L2, (_T("[shell missing - overinstall][%s]]"), shell_path));
    return true;
  }

  return false;
}

// Install the required and optional files.
// Assumes that the user already has the appropriate permissions
// (e.g. is elevated for a machine install).
// Assumes ShouldInstall has been called and returned true.
// Assumes no other instances of GoogleUpdate.exe are running.
HRESULT SetupFiles::Install() {
  OPT_LOG(L1, (_T("[Install files]")));
  ASSERT1(vista_util::IsUserAdmin() || !is_machine_);

  ++metric_setup_files_total;
  HighresTimer metrics_timer;


  SETUP_LOG(L3,
      (_T("[IsAdmin=%d, IsNonElevatedAdmin=%d, SvcInstalled=%d, MachineApp=%d"),
       vista_util::IsUserAdmin(),
       vista_util::IsUserNonElevatedAdmin(),
       goopdate_utils::IsServiceInstalled(),
       is_machine_));

  const bool should_over_install = ConfigManager::Instance()->CanOverInstall();

  // Copy the core program files.
  CPath install_dir = goopdate_utils::BuildInstallDirectory(is_machine_,
                                                            GetVersionString());
  HRESULT hr = CopyInstallFiles(core_program_files_,
                                install_dir,
                                should_over_install);
  if (FAILED(hr)) {
    OPT_LOG(LEVEL_ERROR, (_T("[Failed to copy the files][0x%08x]"), hr));
    if (E_ACCESSDENIED == hr) {
      return GOOPDATE_E_ACCESSDENIED_COPYING_CORE_FILES;
    }
    return hr;
  }

  hr = CopyShell();
  if (FAILED(hr)) {
    OPT_LOG(LEVEL_ERROR, (_T("[Failed to copy shell][0x%08x]"), hr));
    if (E_ACCESSDENIED == hr) {
      return GOOPDATE_E_ACCESSDENIED_COPYING_SHELL;
    }
    return hr;
  }

  // Copy the optional files.
  VERIFY1(SUCCEEDED(CopyInstallFiles(optional_files_,
                                     install_dir,
                                     should_over_install)));

  metric_setup_files_ms.AddSample(metrics_timer.GetElapsedMs());
  ++metric_setup_files_verification_succeeded;
  return S_OK;
}

// Currently only rolls back the shell file.
HRESULT SetupFiles::RollBack() {
  OPT_LOG(L1, (_T("[Roll back files]")));
  ++metric_setup_rollback_files;

  if (!saved_shell_path_.IsEmpty()) {
    SETUP_LOG(L1, (_T("[Rolling back shell from %s]"), saved_shell_path_));
    ++metric_setup_files_rollback_shell;

    std::vector<CString> saved_paths;
    saved_paths.push_back(saved_shell_path_);
    std::vector<CString> install_paths;
    install_paths.push_back(
        goopdate_utils::BuildGoogleUpdateExePath(is_machine_));

    HRESULT hr = CopyAndValidateFiles(saved_paths, install_paths, true);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[CopyAndValidateFiles failed][0x%08x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

void SetupFiles::Uninstall() {
  SETUP_LOG(L2, (_T("[SetupFiles::Uninstall]")));

  // In case we are deleting the current directory as well, let's reset the
  // current directory to a temporary directory. On exit, we'll try to restore
  // the directory (if it still exists).
  scoped_current_directory root_dir(app_util::GetTempDir());

  // Delete the install and crash reports directories.
  CString install_dir(
      is_machine_ ? ConfigManager::Instance()->GetMachineGoopdateInstallDir() :
                    ConfigManager::Instance()->GetUserGoopdateInstallDir());
  HRESULT hr = DeleteDirectory(install_dir);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[DeleteDirectory failed][%s][0x%08x]"),
                   install_dir, hr));
  }

  // TODO(omaha): Remove this and GetMachineDownloadStorageDir() when legacy
  // support is removed.
  // This directory may have been used by previous Omaha versions. Delete it in
  // case this install was upgraded from one of those.
  if (is_machine_) {
    CString dir(ConfigManager::Instance()->GetMachineDownloadStorageDir());
    hr = DeleteDirectory(dir);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[DeleteDirectory failed][%s][0x%08x]"), dir, hr));
    }
  }
}

HRESULT SetupFiles::CopyShell() {
  bool should_copy = false;
  bool already_exists = false;
  CString shell_path = goopdate_utils::BuildGoogleUpdateExePath(is_machine_);

  HRESULT hr = ShouldCopyShell(shell_path,
                               &should_copy,
                               &already_exists);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[ShouldCopyShell failed][0x%08x]"), hr));
    return hr;
  }

  if (should_copy) {
    if (already_exists) {
      ++metric_setup_files_replace_shell;
      VERIFY1(SUCCEEDED(SaveShellForRollback(shell_path)));
    }

    std::vector<CString> shell_files;
    shell_files.push_back(kGoopdateFileName);
    CPath shell_dir(shell_path);
    VERIFY1(shell_dir.RemoveFileSpec());
    hr = CopyInstallFiles(shell_files, shell_dir, already_exists);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[CopyInstallFiles of shell failed][0x%08x]"), hr));
      // TODO(omaha): If a shell already exists, we could try using the
      // existing one, but that may lead to unexpected behavior.
      return hr;
    }
  }

  return S_OK;
}

HRESULT SetupFiles::ShouldCopyShell(const CString& shell_install_path,
                                    bool* should_copy,
                                    bool* already_exists) const {
  ASSERT1(should_copy);
  ASSERT1(already_exists);
  *should_copy = false;
  *already_exists = false;

  CPath source_shell_path(app_util::GetCurrentModuleDirectory());
  if (!source_shell_path.Append(kGoopdateFileName)) {
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }

  if (!File::Exists(shell_install_path)) {
    SETUP_LOG(L3, (_T("[shell does not exist - copying]")));
    *should_copy = true;
    return S_OK;
  }
  *already_exists = true;

  ULONGLONG existing_version = app_util::GetVersionFromFile(shell_install_path);
  if (!existing_version) {
    ASSERT(false, (_T("[failed to get existing shell version - replacing]")));
    *should_copy = true;
    return S_OK;
  }

  ULONGLONG source_version = app_util::GetVersionFromFile(source_shell_path);
  if (!source_version) {
    ASSERT(false, (_T("[failed to get this shell version - not replacing]")));
    *should_copy = false;
    return E_FAIL;
  }

  if (existing_version > source_version) {
    SETUP_LOG(L2, (_T("[newer shell version exists - not copying]")));
    *should_copy = false;
  } else if (existing_version < source_version) {
    if (IsOlderShellVersionCompatible(existing_version)) {
      SETUP_LOG(L2, (_T("[compatible shell version exists - not copying]")));
      *should_copy = false;
    } else {
      SETUP_LOG(L2, (_T("[older shell version exists - copying]")));
      *should_copy = true;
    }
  } else {
    // Same version.
    *should_copy = ConfigManager::Instance()->CanOverInstall();
    SETUP_LOG(L2, (_T("[same version exists - %s copying]"),
                  *should_copy ? _T("") : _T("not")));
  }

  return S_OK;
}

HRESULT SetupFiles::SaveShellForRollback(const CString& shell_install_path) {
  // Copy existing file to a temporary file in case we need to roll back.
  CString temp_file;
  if (!::GetTempFileName(app_util::GetTempDir(),
                         _T("gsh"),
                         0,
                         CStrBuf(temp_file, MAX_PATH))) {
    DWORD error = ::GetLastError();
    SETUP_LOG(LEVEL_WARNING, (_T("[::GetTempFileName failed][%d]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  HRESULT hr = File::Copy(shell_install_path, temp_file, true);
  if (FAILED(hr)) {
    return hr;
  }

  saved_shell_path_ = temp_file;
  return S_OK;
}

HRESULT SetupFiles::BuildFileLists() {
  ASSERT1(core_program_files_.empty());
  ASSERT1(optional_files_.empty());

  core_program_files_.clear();
  core_program_files_.push_back(kGoopdateFileName);
  core_program_files_.push_back(kGoopdateDllName);
  core_program_files_.push_back(kGoopdateCrashHandlerFileName);

  ResourceManager::GetSupportedLanguageDllNames(&core_program_files_);

  core_program_files_.push_back(kHelperInstallerName);

  optional_files_.clear();
  optional_files_.push_back(ACTIVEX_FILENAME);
  optional_files_.push_back(BHO_FILENAME);

  return S_OK;
}

// Assumes that an install is needed.
HRESULT SetupFiles::CopyInstallFiles(const std::vector<CString>& file_names,
                                     const CString& destination_dir,
                                     bool overwrite) {
  SETUP_LOG(L1, (_T("[SetupFiles::CopyInstallFiles]")
                _T("[destination dir=%s][overwrite=%d]"),
                destination_dir, overwrite));
  ASSERT1(!file_names.empty());

  CPath source_dir(app_util::GetCurrentModuleDirectory());
  SETUP_LOG(L2, (_T("[source_dir=%s]"),
                static_cast<const TCHAR*>(source_dir)));

  if (!File::Exists(destination_dir)) {
    // This creates the dir recursively.
    HRESULT hr = CreateDir(destination_dir, NULL);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Clean up any leftover pending removals that a previous uninstall
  // may have left behind.
  // Only do a prefix match if the directory is not the main Update directory.
  // Otherwise, we may remove entries for previous version directories.
  CPath install_path(destination_dir);
  install_path.Canonicalize();
  CPath goopdate_install_path(
      is_machine_ ?
      ConfigManager::Instance()->GetMachineGoopdateInstallDir() :
      ConfigManager::Instance()->GetUserGoopdateInstallDir());
  goopdate_install_path.Canonicalize();
  bool prefix_match = install_path.m_strPath != goopdate_install_path.m_strPath;
  HRESULT hr = File::RemoveFromMovesPendingReboot(destination_dir,
                                                  prefix_match);
  VERIFY1(SUCCEEDED(hr) || !vista_util::IsUserAdmin());

  std::vector<CString> source_file_paths;
  std::vector<CString> destination_file_paths;
  for (size_t i = 0; i < file_names.size(); ++i) {
    CPath file_from(source_dir);
    if (!file_from.Append(file_names[i])) {
      return GOOPDATE_E_PATH_APPEND_FAILED;
    }
    source_file_paths.push_back(file_from);

    CPath file(destination_dir);
    if (!file.Append(file_names[i])) {
      return GOOPDATE_E_PATH_APPEND_FAILED;
    }
    destination_file_paths.push_back(file);

    SETUP_LOG(L2, (_T("[from=%s][to=%s]"),
                  source_file_paths[i],
                  destination_file_paths[i]));
  }

  hr = CopyAndValidateFiles(source_file_paths,
                            destination_file_paths,
                            overwrite);

  SETUP_LOG(L2, (_T("[SetupFiles::CopyInstallFiles - done")));
  return hr;
}

HRESULT SetupFiles::CopyAndValidateFiles(
    const std::vector<CString>& source_file_paths,
    const std::vector<CString>& destination_file_paths,
    bool overwrite) {
  ASSERT1(!source_file_paths.empty());
  ASSERT1(!destination_file_paths.empty());
  ASSERT1(source_file_paths.size() == destination_file_paths.size());

  if (overwrite) {
    // Best effort attempt to delete the current set of files:
    //  * try to remove an .old file that might be there.
    //  * move the current file to a .old and delete it after reboot.
    // Because this is a best effort, we do not propogate errors.

    for (size_t i = 0; i != destination_file_paths.size(); ++i) {
      const CString cur_file = destination_file_paths[i];
      const CString dot_old(cur_file + _T(".old"));
      VERIFY1(SUCCEEDED(File::Remove(dot_old)));
      HRESULT hr = File::Move(cur_file, dot_old, true);
      if (SUCCEEDED(hr)) {
        // Delete after reboot only works for admins. .old files will be left
        // for user installs not being run by elevated admins.
        hr = File::DeleteAfterReboot(dot_old);
        if (FAILED(hr)) {
          SETUP_LOG(LW, (_T("DeleteAfterReboot of %s failed with 0x%08x."),
                        dot_old, hr));
        }
      } else {
        SETUP_LOG(L2, (_T("[failed to move][%s][0x%08x]"), cur_file, hr));
        ASSERT1(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
      }
    }
  }

  for (size_t i = 0; i != source_file_paths.size(); ++i) {
    extra_code1_ = i + 1;  // 1-based; reserves 0 for success or not set.

    HRESULT hr = VerifyFileSignature(source_file_paths[i]);
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[pre-copy signature validation failed][from=%s][0x%x]"),
                   source_file_paths[i], hr));
      ++metric_setup_files_verification_failed_pre;
      return hr;
    }

    hr = File::Copy(source_file_paths[i], destination_file_paths[i], true);
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[copy failed][from=%s][to=%s][0x%08x]"),
                   source_file_paths[i], destination_file_paths[i], hr));
      return hr;
    }

    hr = VerifyFileSignature(destination_file_paths[i]);
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[postcopy signature failed][from=%s][to=%s][0x%x]"),
                   source_file_paths[i], destination_file_paths[i], hr));
      ++metric_setup_files_verification_failed_post;
      VERIFY1(SUCCEEDED(File::Remove(destination_file_paths[i])));
      return hr;
    }
  }

  extra_code1_ = 0;
  return S_OK;
}

// The only secure location we copy to is Program Files, which only happens for
// machine installs.
HRESULT SetupFiles::VerifyFileSignature(const CString& filepath) {
  if (!is_machine_) {
    return S_OK;
  }

  // Verify the Authenticode signature but use use only the local cache for
  // revocation checks.
  HRESULT hr = VerifySignature(filepath, false);
#if TEST_CERTIFICATE
  // The chain of trust will not validate on builds signed with the test
  // certificate.
  if (CERT_E_UNTRUSTEDROOT == hr) {
    hr = S_OK;
  }
#endif
  if (FAILED(hr)) {
    return hr;
  }

  // Verify that there is a Google certificate and that it has not expired.
  if (!VerifySigneeIsGoogle(filepath)) {
    return GOOPDATE_E_VERIFY_SIGNEE_IS_GOOGLE_FAILED;
  }

  return S_OK;
}

bool SetupFiles::IsOlderShellVersionCompatible(ULONGLONG version) {
  for (int i = 0; i < arraysize(kCompatibleOlderShellVersions); ++i) {
    if (version == kCompatibleOlderShellVersions[i]) {
      return true;
    }
  }
  return false;
}

}  // namespace omaha
