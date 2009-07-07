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

#ifndef OMAHA_SETUP_SETUP_FILES_H__
#define OMAHA_SETUP_SETUP_FILES_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

struct Files {
  const TCHAR* file_name;
};

class SetupFiles {
 public:
  explicit SetupFiles(bool is_machine);
  virtual ~SetupFiles();

  HRESULT Init();

  // Returns whether the same version of Google Update should be over-installed.
  bool ShouldOverinstallSameVersion();

  // Installs Google Update files but does not register or install any other
  // applications. Returns whether Google Update was installed.
  // TODO(omaha3): Remove the virtual methods from this class. They are only
  // needed for unit tests and can be eliminated in the upcoming redesign.
  virtual HRESULT Install();

  // Rolls back the changes made during Install(). Call when Setup fails.
  // Returns S_OK if there is nothing to do.
  HRESULT RollBack();

  // Uninstalls Google Update files installed by Install().
  void Uninstall();

  int extra_code1() const { return extra_code1_; }

 private:
  // Copies the shell to the version-independent location if needed.
  HRESULT CopyShell();

  // Determines whether to copy the shell to the version-independent location.
  HRESULT ShouldCopyShell(const CString& shell_dir,
                          bool* should_copy,
                          bool* already_exists) const;

  // Saves the previous version of the shell in case we need to roll it back.
  HRESULT SaveShellForRollback(const CString& shell_install_path);

  // Creates the lists of files that belong to Google Update.
  HRESULT BuildFileLists();

  // Copies file_names from the current directory to the destination directory.
  HRESULT CopyInstallFiles(const std::vector<CString>& file_names,
                           const CString& destination_dir,
                           bool overwrite);

  // Copies each file from the source path to its corresponding destination
  // path. Verifies the signature of the file on each side of the copy.
  // If overwrite is true, files are moved to .old and scheduled for delete
  // after reboot, which only works for elevated admins.
  HRESULT CopyAndValidateFiles(
      const std::vector<CString>& source_file_paths,
      const std::vector<CString>& destination_file_paths,
      bool overwrite);

  // Verifies the file is signed with the Google Certificate.
  // Returns true if filepath is properly signed.
  // Only verifies files if they are being installed to a secure location.
  // If not an official build, allows the Google Test Certificate.
  // Only checks certain extensions since not all extensions can be signed.
  HRESULT VerifyFileSignature(const CString& filepath);

  const bool is_machine_;
  CString saved_shell_path_;  // Path of the previous shell saved for roll back.
  std::vector<CString> core_program_files_;
  std::vector<CString> optional_files_;

  int extra_code1_;

  friend class SetupFilesTest;

  DISALLOW_EVIL_CONSTRUCTORS(SetupFiles);
};

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_FILES_H__

