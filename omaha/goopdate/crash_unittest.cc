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
#include <regex>

#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/time.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/crash_utils.h"
#include "omaha/common/event_logger.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/crash.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "third_party/breakpad/src/client/windows/handler/exception_handler.h"

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

class CrashReporterTest : public testing::Test {
 protected:
  // Initialize the crash reporting for the machine case. The user case is
  // simpler and specific tests can reinitialize for the user case if needed.
  virtual void SetUp() {
    module_dir_ = app_util::GetModuleDirectory(NULL);
    EXPECT_HRESULT_SUCCEEDED(reporter_.Initialize(true));
    reporter_.SetMaxReportsPerDay(kMaxReportsPerDayFromUnittests);
    reporter_.SetCrashReportUrl(kUrlCrashReport);

    // For the duration of this test, append .ut to the version string that's
    // included in crash reports.
    CString postfix_string(kDefaultCrashVersionPostfix);
    postfix_string.Append(_T(".ut"));
    crash_utils::SetCrashVersionPostfix(postfix_string);
  }

  virtual void TearDown() {
    EXPECT_HRESULT_SUCCEEDED(DeleteDirectory(reporter_.crash_dir_));

    crash_utils::SetCrashVersionPostfix(kDefaultCrashVersionPostfix);
  }

  void CallbackHelper(const wchar_t* dump_path,
                      const wchar_t* minidump_id) {
    CString crash_filename;
    crash_filename.Format(_T("%s\\%s.dmp"), dump_path, minidump_id);
    EXPECT_HRESULT_SUCCEEDED(reporter_.Report(crash_filename, CString()));
  }

  static bool MinidumpCallback(const wchar_t* dump_path,
                               const wchar_t* minidump_id,
                               void* context,
                               EXCEPTION_POINTERS*,
                               MDRawAssertionInfo*,
                               bool succeeded) {
    EXPECT_TRUE(dump_path);
    EXPECT_TRUE(minidump_id);
    EXPECT_TRUE(context);
    EXPECT_SUCCEEDED(succeeded);

    CrashReporterTest* thisptr = reinterpret_cast<CrashReporterTest*>(context);
    thisptr->CallbackHelper(dump_path, minidump_id);
    return true;
  }

  // Returns the strings of the last 'Chrome' event in the event log.
  static CString GetLastCrashEventStrings() {
    const size_t kBufferSize = 1024;
    uint8 buffer[kBufferSize] = {0};
    EVENTLOGRECORD* rec = reinterpret_cast<EVENTLOGRECORD*>(buffer);

    rec->Length = kBufferSize;
    EXPECT_SUCCEEDED(EventLogger::ReadLastEvent(_T("Chrome"), rec));
    EXPECT_EQ(kCrashUploadEventId, rec->EventID);

    const TCHAR* strings = reinterpret_cast<const TCHAR*>(
        (reinterpret_cast<uint8*>(buffer + rec->StringOffset)));

    return CString(strings);
  }

  CString module_dir_;
  CrashReporter reporter_;

  static const int kMaxReportsPerDayFromUnittests = INT_MAX;
};

// Tests sending an Omaha crash.
//
// TODO(omaha): This test is disabled because it hangs on Zerg machines with
// network connectivity issues. A google_breakpad::RESULT_FAILED causes a 1 hour
// ::Sleep().
TEST_F(CrashReporterTest, DISABLED_Report_OmahaCrash) {
  CString crash_filename;
  crash_filename.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);

  ::DeleteFile(crash_filename);

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            reporter_.Report(crash_filename, _T("")));

  // Copy the minidump and the corresponding info file.
  CString test_dir;
  test_dir.Format(_T("%s\\unittest_support"), module_dir_);
  ASSERT_SUCCEEDED(File::CopyWildcards(test_dir,          // From.
                                       module_dir_,       // To.
                                       kTestFilenamePattern,
                                       true));

  ASSERT_TRUE(File::Exists(crash_filename));

  ASSERT_HRESULT_SUCCEEDED(reporter_.Report(crash_filename, _T("")));

  // The crash artifacts should be deleted after the crash is reported.
  EXPECT_FALSE(File::Exists(crash_filename));
}

// Tests sending an out-of-process crash.
// This test will write an entry with the source "Chrome" in the Event Log.
// TODO(omaha): This test is disabled because it hangs on machines with
// network connectivity issues. A google_breakpad::RESULT_FAILED causes a 1 hour
// ::Sleep().
TEST_F(CrashReporterTest, DISABLED_Report_ProductCrash) {
  CString crash_filename;
  CString custom_info_filename;
  crash_filename.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);
  custom_info_filename.Format(_T("%s\\%s"), module_dir_, kCustomInfoFilename);

  ::DeleteFile(crash_filename);
  ::DeleteFile(custom_info_filename);

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            reporter_.Report(crash_filename, custom_info_filename));

  // Copy the minidump and the corresponding info file.
  CString test_dir;
  test_dir.Format(_T("%s\\unittest_support"), module_dir_);
  ASSERT_SUCCEEDED(File::CopyWildcards(test_dir,          // From.
                                       module_dir_,       // To.
                                       kTestFilenamePattern,
                                       true));

  ASSERT_TRUE(File::Exists(crash_filename));
  ASSERT_TRUE(File::Exists(custom_info_filename));

  ASSERT_SUCCEEDED(reporter_.Report(crash_filename, custom_info_filename));

  // Check the 'crash uploaded' event log.
  const CString strings = GetLastCrashEventStrings();

  // Verify that the strings include the Id token.
  const std::wregex crash_id_regex {_T("Id=[[:xdigit:]]+\\.")};
  EXPECT_TRUE(std::regex_search(strings.GetString(), crash_id_regex));

  // The crash artifacts should be deleted after the crash is reported.
  EXPECT_FALSE(File::Exists(crash_filename));
  EXPECT_FALSE(File::Exists(custom_info_filename));
}

// Tests generation of a minidump and uploading it to the staging server.
TEST_F(CrashReporterTest, WriteMinidump) {
  ASSERT_TRUE(!reporter_.crash_dir_.IsEmpty());

  MINIDUMP_TYPE dump_type = MiniDumpNormal;
  ExceptionHandler handler(reporter_.crash_dir_.GetString(), NULL,
                           &MinidumpCallback, reinterpret_cast<void*>(this),
                           ExceptionHandler::HANDLER_NONE,
                           dump_type, static_cast<wchar_t*>(NULL), NULL);
  ASSERT_TRUE(handler.WriteMinidump());
}

TEST_F(CrashReporterTest, GetProductName) {
  CrashReporter::ParameterMap parameters;
  EXPECT_STREQ(SHORT_COMPANY_NAME _T(" Error Reporting"),
               CrashReporter::ReadMapProductName(parameters));

  parameters[_T("prod")] = _T("Update2");
  EXPECT_STREQ(_T("Update2"), CrashReporter::ReadMapProductName(parameters));
}

TEST_F(CrashReporterTest, SaveLastCrash) {
  // Copy a test file into the module directory to use as a crash file.
  CString test_file;    // The unit test support file.
  CString crash_file;   // The crash file to be backed up.
  test_file.AppendFormat(_T("%s\\unittest_support\\%s"),
                         module_dir_,
                         kMiniDumpFilename);
  crash_file.AppendFormat(_T("%s\\%s"),
                          reporter_.crash_dir_,
                          kMiniDumpFilename);
  EXPECT_TRUE(File::Exists(test_file));
  EXPECT_TRUE(::CopyFile(test_file, crash_file, false));
  EXPECT_TRUE(File::Exists(crash_file));

  EXPECT_HRESULT_SUCCEEDED(reporter_.SaveLastCrash(crash_file, _T("test")));

  CString saved_crash_file;  // The name of backup crash file.
  saved_crash_file.AppendFormat(_T("%s\\test-last.dmp"), reporter_.crash_dir_);
  EXPECT_TRUE(File::Exists(saved_crash_file));

  EXPECT_TRUE(::DeleteFile(saved_crash_file));
}

TEST_F(CrashReporterTest, CleanStaleCrashes) {
  // Copy a test file into the module directory to use as a crash file.
  CString test_file;    // The unit test support file.
  CString crash_file;   // The crash file to be backed up.
  test_file.AppendFormat(_T("%s\\unittest_support\\%s"),
                         module_dir_, kMiniDumpFilename);
  crash_file.AppendFormat(_T("%s\\%s.dmp"),
                          reporter_.crash_dir_,
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
  reporter_.CleanStaleCrashes();
  EXPECT_TRUE(File::Exists(crash_file));

  // Create a time value 25 hours in the past. Expect the crash file is deleted.
  Time64ToFileTime(now - 25 * kHoursTo100ns, &time_created);
  EXPECT_HRESULT_SUCCEEDED(File::SetFileTime(crash_file, &time_created,
                                             NULL, NULL));
  reporter_.CleanStaleCrashes();
  EXPECT_FALSE(File::Exists(crash_file));
}

}  // namespace omaha

