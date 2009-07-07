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
// Unit tests for the file execution module of the MSP custom action.

#include "omaha/common/app_util.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/vistautil.h"
#include "omaha/recovery/repair_exe/custom_action/execute_repair_file.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

// The valid repair file saves the arguments passed to it to a file.
void RunAndVerifySavedArgs(const CString& args) {
  CString expected_copy_path =
      _T("%PROGRAMFILES%\\Google\\Update\\SaveArguments.exe");
  EXPECT_SUCCEEDED(ExpandStringWithSpecialFolders(&expected_copy_path));
  CString saved_arguments_file_path =
      _T("%PROGRAMFILES%\\Google\\Update\\saved_arguments.txt");
  EXPECT_SUCCEEDED(ExpandStringWithSpecialFolders(&saved_arguments_file_path));

  CString repair_file(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(repair_file, MAX_PATH),
              _T("unittest_support\\SaveArguments.exe")));

  ::DeleteFile(saved_arguments_file_path);

  if (vista_util::IsUserAdmin()) {
    EXPECT_FALSE(File::Exists(saved_arguments_file_path));

    EXPECT_SUCCEEDED(omaha::VerifyFileAndExecute(repair_file, args));

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

    // Do not assume the buffer read by ReadFile remains zero-terminated.
    EXPECT_TRUE(::ReadFile(get(file),
                           buffer,
                           kBufferLen * sizeof(TCHAR),
                           &bytes_read,
                           NULL));
    EXPECT_EQ(0, bytes_read % sizeof(TCHAR));
    buffer[bytes_read / sizeof(TCHAR)] = _T('\0');
    EXPECT_STREQ(args, buffer);

    reset(file);

    ::DeleteFile(expected_copy_path);
    EXPECT_TRUE(::DeleteFile(saved_arguments_file_path));
  } else {
    const bool expected_file_exists = File::Exists(saved_arguments_file_path);
    EXPECT_EQ(E_ACCESSDENIED, omaha::VerifyFileAndExecute(repair_file, args));

    // We can't force the file to be deleted, so make sure it wasn't created
    // or deleted by the above method.
    EXPECT_EQ(expected_file_exists, File::Exists(saved_arguments_file_path));
  }
}

}  // namespace

TEST(ExecuteRepairFileTest, VerifyFileAndExecute_EmptyFilename) {
  EXPECT_EQ(E_INVALIDARG, VerifyFileAndExecute(_T(""), _T("")));
}

TEST(ExecuteRepairFileTest, VerifyFileAndExecute_FileDoesNotExist) {
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            VerifyFileAndExecute(_T("no_such_file.exe"), _T("")));
}

TEST(ExecuteRepairFileTest, VerifyFileAndExecute_FilenameIsDirectory) {
  EXPECT_EQ(E_ACCESSDENIED,
            VerifyFileAndExecute(_T("C:\\Windows"), _T("")));
}

TEST(ExecuteRepairFileTest, VerifyFileAndExecute_UnsignedFile) {
  CString expected_copy_path =
      _T("%PROGRAMFILES%\\Google\\Update\\GoogleUpdate_unsigned.exe");
  EXPECT_SUCCEEDED(ExpandStringWithSpecialFolders(&expected_copy_path));
  CString repair_file(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(repair_file, MAX_PATH),
              _T("GoogleUpdate_unsigned.exe")));

  if (vista_util::IsUserAdmin()) {
    EXPECT_EQ(TRUST_E_NOSIGNATURE, VerifyFileAndExecute(repair_file, _T("")));

    EXPECT_TRUE(File::Exists(expected_copy_path));
    EXPECT_TRUE(::DeleteFile(expected_copy_path));
  } else {
    const bool expected_file_exists = File::Exists(expected_copy_path);
    EXPECT_EQ(E_ACCESSDENIED, VerifyFileAndExecute(repair_file, _T("")));

    // We can't force the file to be deleted, so make sure it wasn't created
    // or deleted by the above method.
    EXPECT_EQ(expected_file_exists, File::Exists(expected_copy_path));
  }
}

TEST(ExecuteRepairFileTest, VerifyFileAndExecute_ValidRepairFileWithArgs) {
  RunAndVerifySavedArgs(_T("These /are the args."));
}

TEST(ExecuteRepairFileTest, VerifyFileAndExecute_ValidRepairFileWithoutArgs) {
  RunAndVerifySavedArgs(_T(""));
}

}  // namespace omaha
