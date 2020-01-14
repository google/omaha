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
#include <map>
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
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "third_party/breakpad/src/client/windows/common/ipc_protocol.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

// TODO(omaha): Modify the tests to avoid writing files to the staging
// directory, which should not be modified after building.

namespace omaha {

class CrashUtilsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    module_dir_ = app_util::GetModuleDirectory(NULL);
  }

  static void BuildPipeSecurityAttributesTest(bool is_machine) {
    CSecurityDesc sd1;
    EXPECT_SUCCEEDED(crash_utils::BuildPipeSecurityAttributes(is_machine,
                                                              &sd1));
    CString sddl1;
    sd1.ToString(&sddl1, OWNER_SECURITY_INFORMATION |
                         GROUP_SECURITY_INFORMATION |
                         DACL_SECURITY_INFORMATION  |
                         SACL_SECURITY_INFORMATION  |
                         LABEL_SECURITY_INFORMATION);

    CSecurityDesc sd2;
    if (vista_util::IsVistaOrLater()) {
      EXPECT_TRUE(sd2.FromString(LOW_INTEGRITY_SDDL_SACL));
    }
    EXPECT_SUCCEEDED(crash_utils::AddPipeSecurityDaclToDesc(is_machine,
                                                            &sd2));
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

  CString module_dir_;
};

TEST_F(CrashUtilsTest, CreateCustomInfoFile) {
  const TCHAR kMiniDumpFilename[]   = _T("minidump.dmp");
  const TCHAR kCustomInfoFilename[] = _T("minidump.txt");

  CString expected_custom_info_file_path;
  expected_custom_info_file_path.Format(_T("%s\\%s"),
                                        module_dir_,
                                        kCustomInfoFilename);

  CString crash_filename;
  crash_filename.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);

  std::map<CString, CString> custom_info;
  custom_info[_T("foo")] = _T("bar");

  CString actual_custom_info_filepath;
  EXPECT_SUCCEEDED(crash_utils::CreateCustomInfoFile(
                       crash_filename,
                       custom_info,
                       &actual_custom_info_filepath));
  EXPECT_TRUE(File::Exists(actual_custom_info_filepath));
  EXPECT_TRUE(::DeleteFile(actual_custom_info_filepath));

  // Tests an invalid file name.
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_NAME),
            crash_utils::CreateCustomInfoFile(_T("C:\\\"minidump.dmp"),
                                              custom_info,
                                              &actual_custom_info_filepath));
}

// Makes sure that the security descriptor that BuildPipeSecurityAttributes
// creates matches the security descriptor built by using
// CSecurityDesc::FromString and AddPipeSecurityDaclToDesc. The latter method
// uses an approach similar to what is documented in MSDN:
// http://msdn.microsoft.com/en-us/library/bb625960.aspx
TEST_F(CrashUtilsTest, BuildPipeSecurityAttributes) {
  BuildPipeSecurityAttributesTest(true);
  BuildPipeSecurityAttributesTest(false);
}

TEST_F(CrashUtilsTest, StartProcessWithNoExceptionHandler) {
  // Negative test.
  CString filename(_T("DoesNotExist.exe"));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            crash_utils::StartProcessWithNoExceptionHandler(&filename));
}

TEST_F(CrashUtilsTest, IsCrashReportProcess) {
  ::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName, NULL);

  bool is_crash_report_process = false;
  EXPECT_SUCCEEDED(crash_utils::IsCrashReportProcess(&is_crash_report_process));
  EXPECT_FALSE(is_crash_report_process);

  EXPECT_TRUE(::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName,
              _T("1")));
  is_crash_report_process = false;
  EXPECT_SUCCEEDED(crash_utils::IsCrashReportProcess(&is_crash_report_process));
  EXPECT_TRUE(is_crash_report_process);

  EXPECT_TRUE(::SetEnvironmentVariable(kNoCrashHandlerEnvVariableName, NULL));
}

TEST_F(CrashUtilsTest, GetCustomInfoFilePath) {
  const TCHAR kMiniDumpFilename[]   = _T("minidump.dmp");
  const TCHAR kCustomInfoFilename[] = _T("minidump.txt");

  CString dump_file_path;
  dump_file_path.Format(_T("%s\\%s"), module_dir_, kMiniDumpFilename);

  CString expected_custom_info_file_path;
  expected_custom_info_file_path.Format(_T("%s\\%s"),
                                        module_dir_,
                                        kCustomInfoFilename);

  CString actual_custom_info_filepath;
  EXPECT_SUCCEEDED(crash_utils::GetCustomInfoFilePath(
                       dump_file_path, &actual_custom_info_filepath));
  EXPECT_STREQ(expected_custom_info_file_path, actual_custom_info_filepath);
}

TEST_F(CrashUtilsTest, ConvertCustomClientInfoToMap) {
  const WCHAR kNameOne[] = L"name1";
  const WCHAR kNameTwo[] = L"name2";
  const WCHAR kValueOne[] = L"value1";
  const WCHAR kValueTwo[] = L"value2";
  const WCHAR kValueOther[] = L"overwrite";

  google_breakpad::CustomInfoEntry entries[3] = {
      google_breakpad::CustomInfoEntry(kNameOne, kValueOne),
      google_breakpad::CustomInfoEntry(kNameTwo, kValueTwo),
      google_breakpad::CustomInfoEntry(kNameTwo, kValueOther)
      };

  google_breakpad::CustomClientInfo info = { entries, 2 };

  std::map<CStringW, CStringW> map;

  // Start with base case -- two entries with unique keys.
  EXPECT_EQ(S_OK, crash_utils::ConvertCustomClientInfoToMap(info, &map));
  ASSERT_EQ(2, map.size());
  EXPECT_STREQ(kValueOne, map[kNameOne]);
  EXPECT_STREQ(kValueTwo, map[kNameTwo]);

  // Try three entries where the same key appears twice.  Should override.
  info.count = 3;
  EXPECT_EQ(S_OK, crash_utils::ConvertCustomClientInfoToMap(info, &map));
  ASSERT_EQ(2, map.size());
  EXPECT_STREQ(kValueOne, map[kNameOne]);
  EXPECT_STREQ(kValueOther, map[kNameTwo]);

  // Modify the third entry so that the value is not null-terminated.  Should
  // succeed, but return S_FALSE.
  for (int i = 0; i < google_breakpad::CustomInfoEntry::kValueMaxLength; ++i) {
    entries[2].value[i] = _T('A');
  }
  EXPECT_EQ(S_FALSE, crash_utils::ConvertCustomClientInfoToMap(info, &map));
  ASSERT_EQ(2, map.size());
  EXPECT_STREQ(kValueOne, map[kNameOne]);

  // Supply a CustomClientInfo with 0 entries but non-NULL ptr.  Should succeed.
  info.count = 0;
  EXPECT_EQ(S_OK, crash_utils::ConvertCustomClientInfoToMap(info, &map));
  ASSERT_EQ(0, map.size());

  // Supply a CustomClientInfo with 0 entries and a NULL ptr.  Should succeed.
  info.entries = 0;
  EXPECT_EQ(S_OK, crash_utils::ConvertCustomClientInfoToMap(info, &map));
  ASSERT_EQ(0, map.size());

  // Supply a corrupt CustomClientInfo (NULL entries, non-zero count).  Should
  // treat it as empty and succeed, returning an empty map.
  info.count = 1;
  EXPECT_EQ(S_OK, crash_utils::ConvertCustomClientInfoToMap(info, &map));
  ASSERT_EQ(0, map.size());
}

TEST_F(CrashUtilsTest, IsUploadDeferralRequested) {
  const TCHAR kSentinelNormal[] = _T("deferred-upload");
  const TCHAR kSentinelAlternate[] = _T("dEfErReD-UpLoAd");

  std::map<CString, CString> map;

  // Return false for an empty map.
  EXPECT_FALSE(crash_utils::IsUploadDeferralRequested(map));

  // Add some values that aren't the sentinel value; should return false.
  map[_T("key1")] = _T("value1");
  map[_T("key2")] = _T("value2");
  EXPECT_FALSE(crash_utils::IsUploadDeferralRequested(map));

  // Add the sentinel key with a value of "true"; should return true.
  map[kSentinelNormal] = _T("true");
  EXPECT_TRUE(crash_utils::IsUploadDeferralRequested(map));

  // Use values other than "true", including empty string.  Should return false.
  map[kSentinelNormal] = _T("false");
  EXPECT_FALSE(crash_utils::IsUploadDeferralRequested(map));
  map[kSentinelNormal] = _T("");
  EXPECT_FALSE(crash_utils::IsUploadDeferralRequested(map));

  // It should have similar behavior with alternate capitalizations.
  map.erase(kSentinelNormal);

  map[kSentinelAlternate] = _T("TrUe");
  EXPECT_TRUE(crash_utils::IsUploadDeferralRequested(map));
  map[kSentinelAlternate] = _T("FaLsE");
  EXPECT_FALSE(crash_utils::IsUploadDeferralRequested(map));
  map[kSentinelAlternate] = _T("");
  EXPECT_FALSE(crash_utils::IsUploadDeferralRequested(map));
}

}  // namespace omaha


