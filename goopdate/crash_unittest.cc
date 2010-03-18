// Copyright 2007-2010 Google Inc.
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

#include <string>
#include "omaha/goopdate/crash.h"
#include "omaha/common/app_util.h"
#include "omaha/common/constants.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/file.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/time.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/testing/unit_test.h"

// TODO(omaha): Modify the tests to avoid writing files to the staging
// directory, which should not be modified after building.

using google_breakpad::ClientInfo;
using google_breakpad::CustomClientInfo;
using google_breakpad::CustomInfoEntry;
using google_breakpad::ExceptionHandler;

namespace omaha {

namespace {

const TCHAR kMiniDumpFilename[]     = _T("minidump.dmp");
const TCHAR kCustomInfoFilename[]   = _T("minidump.txt");
const TCHAR kTestFilenamePattern[]  = _T("minidump.*");

}  // namespace

class CrashTest : public testing::Test {
 protected:
  // Initialize the crash reporting for the machine case. The user case is
  // simpler and specific tests can reinitialize for the user case if needed.
  virtual void SetUp() {
    module_dir_ = app_util::GetModuleDirectory(NULL);
    EXPECT_HRESULT_SUCCEEDED(Crash::Initialize(true));
  }

  virtual void TearDown() {
    EXPECT_HRESULT_SUCCEEDED(DeleteDirectory(Crash::crash_dir_));
  }

  static void CallbackHelper(const wchar_t* dump_path,
                             const wchar_t* minidump_id) {
    CString postfix_string(kCrashVersionPostfixString);
    postfix_string.Append(_T(".ut"));
    CString crash_filename;
    crash_filename.Format(_T("%s\\%s.dmp"), dump_path, minidump_id);
    Crash::set_max_reports_per_day(kMaxReportsPerDayFromUnittests);
    Crash::set_version_postfix(postfix_string);
    Crash::set_crash_report_url(kUrlCrashReport);
    EXPECT_SUCCEEDED(Crash::Report(true, crash_filename, CString(), false));
  }

  static bool MinidumpCallback(const wchar_t* dump_path,
                               const wchar_t* minidump_id,
                               void* context,
                               EXCEPTION_POINTERS*,
                               MDRawAssertionInfo*,
                               bool succeeded) {
    EXPECT_TRUE(dump_path);
    EXPECT_TRUE(minidump_id);
    EXPECT_TRUE(!context);
    EXPECT_SUCCEEDED(succeeded);

    CallbackHelper(dump_path, minidump_id);
    return true;
  }

  static void BuildPipeSecurityAttributesTest(bool is_machine) {
    CSecurityAttributes pipe_sec_attrs;
    EXPECT_TRUE(
        Crash::BuildPipeSecurityAttributes(is_machine, &pipe_sec_attrs));
    LPVOID sec_desc(pipe_sec_attrs.lpSecurityDescriptor);
    CSecurityDesc sd1(*reinterpret_cast<SECURITY_DESCRIPTOR*>(sec_desc));
    CString sddl1;

    sd1.ToString(&sddl1, OWNER_SECURITY_INFORMATION |
                         GROUP_SECURITY_INFORMATION |
                         DACL_SECURITY_INFORMATION  |
                         SACL_SECURITY_INFORMATION  |
                         LABEL_SECURITY_INFORMATION);

    CSecurityDesc sd2;
    EXPECT_TRUE(Crash::AddPipeSecurityDaclToDesc(is_machine, &sd2));
    EXPECT_HRESULT_SUCCEEDED(
        vista_util::AddLowIntegritySaclToExistingDesc(&sd2));

    CString sddl2;
    sd2.ToString(&sddl2, OWNER_SECURITY_INFORMATION |
                         GROUP_SECURITY_INFORMATION |
                         DACL_SECURITY_INFORMATION  |
                         SACL_SECURITY_INFORMATION  |
                         LABEL_SECURITY_INFORMATION);

    EXPECT_STREQ(sddl2, sddl1);

    if (vista_util::IsVistaOrLater()) {
      // The low integrity SACL is at the end of the SDDL string.
      EXPECT_STREQ(LOW_INTEGRITY_SDDL_SACL,
                   sddl1.Right(arraysize(LOW_INTEGRITY_SDDL_SACL) - 1));
    }
  }


  static HRESULT StartSenderWithCommandLine(CString* cmd_line) {
    return Crash::StartSenderWithCommandLine(cmd_line);
  }

  CString module_dir_;

  static const int kMaxReportsPerDayFromUnittests = INT_MAX;
};

TEST_F(CrashTest, CreateCustomInfoFile) {
  CString expected_custom_info_file_path;
  expected_custom_info_file_path.Format(_T("%s\\%s"),
                                        module_dir_, kCustomInfoFilename);

  CString crash_filename;
  crash_filename.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);
  CustomInfoEntry info_entry(_T("foo"), _T("bar"));
  CustomClientInfo custom_client_info = {&info_entry, 1};

  CString actual_custom_info_filepath;
  EXPECT_SUCCEEDED(Crash::CreateCustomInfoFile(crash_filename,
                                               custom_client_info,
                                               &actual_custom_info_filepath));
  EXPECT_STREQ(expected_custom_info_file_path, actual_custom_info_filepath);
  EXPECT_TRUE(File::Exists(actual_custom_info_filepath));
  EXPECT_TRUE(::DeleteFile(actual_custom_info_filepath));

  // Tests an invalid file name.
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_NAME),
            Crash::CreateCustomInfoFile(_T("C:\\\"minidump.dmp"),
                                        custom_client_info,
                                        &actual_custom_info_filepath));
}

// Tests sending an Omaha crash.
TEST_F(CrashTest, Report_OmahaCrash) {
  CString crash_filename;
  crash_filename.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);

  ::DeleteFile(crash_filename);

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            Crash::Report(true, crash_filename, _T(""), _T("")));

  // Copy the minidump and the corresponding info file.
  CString test_dir;
  test_dir.Format(_T("%s\\unittest_support"), module_dir_);
  ASSERT_SUCCEEDED(File::CopyWildcards(test_dir,          // From.
                                       module_dir_,       // To.
                                       kTestFilenamePattern,
                                       true));

  ASSERT_TRUE(File::Exists(crash_filename));

  ASSERT_SUCCEEDED(Crash::Report(true, crash_filename, _T(""), _T("")));

  // The crash artifacts should be deleted after the crash is reported.
  EXPECT_FALSE(File::Exists(crash_filename));
}

// Tests sending an out-of-process crash.
// This test will write an entry with the source "Update2" in the Event Log.
TEST_F(CrashTest, Report_ProductCrash) {
  CString crash_filename;
  CString custom_info_filename;
  crash_filename.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);
  custom_info_filename.Format(_T("%s\\%s"), module_dir_, kCustomInfoFilename);

  ::DeleteFile(crash_filename);
  ::DeleteFile(custom_info_filename);

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            Crash::Report(true, crash_filename, custom_info_filename, _T("")));

  // Copy the minidump and the corresponding info file.
  CString test_dir;
  test_dir.Format(_T("%s\\unittest_support"), module_dir_);
  ASSERT_SUCCEEDED(File::CopyWildcards(test_dir,          // From.
                                       module_dir_,       // To.
                                       kTestFilenamePattern,
                                       true));

  ASSERT_TRUE(File::Exists(crash_filename));
  ASSERT_TRUE(File::Exists(custom_info_filename));

  ASSERT_SUCCEEDED(Crash::Report(true, crash_filename,
                                 custom_info_filename, _T("")));

  // The crash artifacts should be deleted after the crash is reported.
  EXPECT_FALSE(File::Exists(crash_filename));
  EXPECT_FALSE(File::Exists(custom_info_filename));
}

// Tests generation of a minidump and uploading it to the staging server.
TEST_F(CrashTest, WriteMinidump) {
  ASSERT_TRUE(!Crash::crash_dir_.IsEmpty());
  ASSERT_TRUE(ExceptionHandler::WriteMinidump(Crash::crash_dir_.GetString(),
                                              &MinidumpCallback,
                                              NULL));
}

// Tests the retrieval of the exception information from an existing mini dump.
TEST_F(CrashTest, GetExceptionInfo) {
  const uint32 kExceptionAddress  = 0x12345670;
  const uint32 kExceptionCode     = 0xc0000005;

  CString filename;
  filename.AppendFormat(_T("%s\\unittest_support\\%s"),
                        module_dir_, kMiniDumpFilename);
  MINIDUMP_EXCEPTION ex_info = {0};
  ASSERT_SUCCEEDED(Crash::GetExceptionInfo(filename, &ex_info));
  EXPECT_EQ(kExceptionAddress, ex_info.ExceptionAddress);
  EXPECT_EQ(kExceptionCode, ex_info.ExceptionCode);
}

TEST_F(CrashTest, IsCrashReportProcess) {
  // Clear the environment variable.
  ::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName, NULL);

  bool is_crash_report_process = false;
  EXPECT_SUCCEEDED(Crash::IsCrashReportProcess(&is_crash_report_process));
  EXPECT_FALSE(is_crash_report_process);

  EXPECT_TRUE(::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName,
              _T("1")));
  is_crash_report_process = false;
  EXPECT_SUCCEEDED(Crash::IsCrashReportProcess(&is_crash_report_process));
  EXPECT_TRUE(is_crash_report_process);

  // Clear the environment variable.
  EXPECT_TRUE(::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName, NULL));
}

TEST_F(CrashTest, GetProductName) {
  Crash::ParameterMap parameters;
  EXPECT_STREQ(_T("Google Error Reporting"), Crash::GetProductName(parameters));

  parameters[_T("prod")] = _T("Update2");
  EXPECT_STREQ(_T("Update2"), Crash::GetProductName(parameters));
}

TEST_F(CrashTest, SaveLastCrash) {
  // Copy a test file into the module directory to use as a crash file.
  CString test_file;    // The unit test support file.
  CString crash_file;   // The crash file to be backed up.
  test_file.AppendFormat(_T("%s\\unittest_support\\%s"),
                         module_dir_, kMiniDumpFilename);
  crash_file.AppendFormat(_T("%s\\%s"), Crash::crash_dir_, kMiniDumpFilename);
  EXPECT_TRUE(File::Exists(test_file));
  EXPECT_TRUE(::CopyFile(test_file, crash_file, false));
  EXPECT_TRUE(File::Exists(crash_file));

  EXPECT_HRESULT_SUCCEEDED(Crash::SaveLastCrash(crash_file, _T("test")));

  CString saved_crash_file;  // The name of backup crash file.
  saved_crash_file.AppendFormat(_T("%s\\test-last.dmp"), Crash::crash_dir_);
  EXPECT_TRUE(File::Exists(saved_crash_file));

  EXPECT_TRUE(::DeleteFile(saved_crash_file));
}

TEST_F(CrashTest, StartServer) {
  // Terminate all processes to avoid conflicts on the crash services pipe.
  TerminateAllGoogleUpdateProcesses();

  EXPECT_HRESULT_SUCCEEDED(Crash::StartServer());

  // Try opening the crash services pipe.
  CString user_sid;
  EXPECT_HRESULT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &user_sid));
  CString pipe_name;
  pipe_name.AppendFormat(_T("\\\\.\\pipe\\GoogleCrashServices\\%s"), user_sid);
  scoped_pipe pipe_handle(::CreateFile(pipe_name,
                                       GENERIC_READ | GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL,
                                       OPEN_EXISTING,
                                       0,
                                       NULL));
  EXPECT_TRUE(pipe_handle);

  Crash::StopServer();
}

TEST_F(CrashTest, CleanStaleCrashes) {
  // Copy a test file into the module directory to use as a crash file.
  CString test_file;    // The unit test support file.
  CString crash_file;   // The crash file to be backed up.
  test_file.AppendFormat(_T("%s\\unittest_support\\%s"),
                         module_dir_, kMiniDumpFilename);
  crash_file.AppendFormat(_T("%s\\%s.dmp"),
                          Crash::crash_dir_,
                          _T("5695F1E0-95BD-4bc2-99C0-E9DCC0AC5274"));
  EXPECT_TRUE(File::Exists(test_file));
  EXPECT_TRUE(::CopyFile(test_file, crash_file, false));
  EXPECT_TRUE(File::Exists(crash_file));

  FILETIME time_created = {0};
  time64 now = GetCurrent100NSTime();

  // Create a time value 23 hours in the past. Expect the crash file remains.
  Time64ToFileTime(now - 23 * kHoursTo100ns, &time_created);
  EXPECT_HRESULT_SUCCEEDED(File::SetFileTime(crash_file, &time_created,
                                             NULL, NULL));
  Crash::CleanStaleCrashes();
  EXPECT_TRUE(File::Exists(crash_file));

  // Create a time value 25 hours in the past. Expect the crash file is deleted.
  Time64ToFileTime(now - 25 * kHoursTo100ns, &time_created);
  EXPECT_HRESULT_SUCCEEDED(File::SetFileTime(crash_file, &time_created,
                                             NULL, NULL));
  Crash::CleanStaleCrashes();
  EXPECT_FALSE(File::Exists(crash_file));
}

// Installs and uninstalls the crash handler in the user case.
TEST_F(CrashTest, InstallCrashHandler) {
  EXPECT_HRESULT_SUCCEEDED(Crash::InstallCrashHandler(false));
  Crash::UninstallCrashHandler();
}

// Makes sure that the security descriptor that BuildPipeSecurityAttributes
// creates matches the security descriptor built by adding the DACL first, and
// then using AddLowIntegritySaclToExistingDesc(). The latter method uses an
// approach similar to what is documented in MSDN:
// http://msdn.microsoft.com/en-us/library/bb625960.aspx
//
// Also, makes sure that the security descriptor that
// BuildPipeSecurityAttributes creates has the low integrity SACL within it.
TEST_F(CrashTest, BuildPipeSecurityAttributes) {
  BuildPipeSecurityAttributesTest(true);
  BuildPipeSecurityAttributesTest(false);
}

TEST_F(CrashTest, StartSenderWithCommandLine) {
  // Negative test.
  CString filename(_T(""));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            StartSenderWithCommandLine(&filename));
}

}  // namespace omaha

