// Copyright 2003-2010 Google Inc.
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

#include <ATLComTime.h>
#include <atltypes.h>
#include <atlwin.h>

#include <map>
#include <vector>

#include "base/rand_util.h"
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/dynamic_link_kernel32.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/shell.h"
#include "omaha/base/string.h"
#include "omaha/base/time.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/system_info.h"
#include "omaha/base/vistautil.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

// Make sure that the time functions work.
TEST(UtilsTest, Time) {
  // TODO(omaha): - add a test from string to time and back again.
  // Further test the time converters.
  time64 now = GetCurrent100NSTime();
  ASSERT_TRUE(StringToTime(TimeToString(now)) == now);

  // Test GetTimeCategory.
  ASSERT_EQ(PAST,
            GetTimeCategory(static_cast<time64>(0)));
  ASSERT_EQ(PRESENT,
            GetTimeCategory(static_cast<time64>(now)));
  ASSERT_EQ(PRESENT,
            GetTimeCategory(static_cast<time64>(now - kDaysTo100ns)));
  ASSERT_EQ(PRESENT,
            GetTimeCategory(static_cast<time64>(now - 365 * kDaysTo100ns)));
  // A little bit in the future is also considered present.
  ASSERT_EQ(PRESENT,
            GetTimeCategory(static_cast<time64>(now + kDaysTo100ns)));
  ASSERT_EQ(PRESENT,
            GetTimeCategory(static_cast<time64>(
                now - 30 * 365 * kDaysTo100ns)));
  ASSERT_EQ(PAST,
            GetTimeCategory(static_cast<time64>(
                now - 50 * 365 * kDaysTo100ns)));
  ASSERT_EQ(FUTURE,
            GetTimeCategory(static_cast<time64>(now + kDaysTo100ns * 6)));
  ASSERT_EQ(FUTURE,
            GetTimeCategory(static_cast<time64>(now + 365 * kDaysTo100ns)));

  // Test IsValidTime.
  ASSERT_FALSE(IsValidTime(static_cast<time64>(0)));
  ASSERT_TRUE(IsValidTime(static_cast<time64>(now)));
  ASSERT_TRUE(IsValidTime(static_cast<time64>(now - 365 * kDaysTo100ns)));
  ASSERT_TRUE(IsValidTime(static_cast<time64>(now - 10 * 365 * kDaysTo100ns)));
  ASSERT_TRUE(IsValidTime(static_cast<time64>(now + kDaysTo100ns)));
  ASSERT_FALSE(IsValidTime(static_cast<time64>(now - 50 * 365 * kDaysTo100ns)));
  ASSERT_FALSE(IsValidTime(static_cast<time64>(now + 50 * 365 * kDaysTo100ns)));
  ASSERT_FALSE(IsValidTime(static_cast<time64>(now + kDaysTo100ns * 6)));
}

TEST(UtilsTest, GetFolderPath_Success) {
  CString path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &path));
  BOOL isWow64 = FALSE;
  EXPECT_SUCCEEDED(Kernel32::IsWow64Process(GetCurrentProcess(), &isWow64));
  CString expected_path = isWow64 ?
      _T("C:\\Program Files (x86)") : _T("C:\\Program Files");
  EXPECT_STREQ(expected_path, path);
}

TEST(UtilsTest, GetFolderPath_Errors) {
  CString path;
  EXPECT_EQ(E_INVALIDARG, GetFolderPath(0x7fff, &path));
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_EQ(E_INVALIDARG, GetFolderPath(CSIDL_PROGRAM_FILES, NULL));
}

TEST(UtilsTest, GetFolderPath_ProgramFiles_RegistryRedirection) {
  if (!IsEnvironmentVariableSet(
    _T("TEST_GETFOLDERPATH_PROGRAMFILES_REGISTRYREDIRECTION"))) {
    std::wcout << _T("\tThis test needs to be run separately. Set ")
                  _T("'TEST_GETFOLDERPATH_PROGRAMFILES_REGISTRYREDIRECTION' in")
                  _T(" the environment, then run it separately.") << std::endl;
    return;
  }

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);

  CString path;
  EXPECT_EQ(S_FALSE, GetFolderPath(CSIDL_PROGRAM_FILES, &path));
  BOOL isWow64 = FALSE;
  EXPECT_TRUE(Kernel32::IsWow64Process(GetCurrentProcess(), &isWow64));
  CString expected_path = isWow64 ?
      _T("C:\\Program Files (x86)") : _T("C:\\Program Files");
  EXPECT_STREQ(expected_path, path);

  RestoreRegistryHives();
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST(UtilsTest, EnableSecureDllLoading) {
  if (!EnableSecureDllLoading()) {
    std::wcout << _T("\tThis test did not run because ")
                  _T("'::SetDefaultDllDirectories' is not supported on this ")
                  _T("system.") << std::endl;
    return;
  }
}

TEST(UtilsTest, CallEntryPoint0) {
  HRESULT hr(E_FAIL);
  ASSERT_FAILED(CallEntryPoint0(L"random-nonsense.dll", "foobar", &hr));
}

TEST(UtilsTest, ReadEntireFile) {
  CString directory = app_util::GetModuleDirectory(NULL);
  ASSERT_FALSE(directory.IsEmpty());
  CString file_name;
  file_name.Format(_T("%s\\unittest_support\\declaration.txt"), directory);

  std::vector<byte> buffer;
  ASSERT_FAILED(ReadEntireFile(L"C:\\F00Bar\\ImaginaryFile", 0, &buffer));

  ASSERT_SUCCEEDED(ReadEntireFile(file_name, 0, &buffer));
  ASSERT_TRUE(9405 == buffer.size() /*LF*/ || 9514 == buffer.size() /*CRLF*/);
  buffer.resize(0);
  ASSERT_FAILED(ReadEntireFile(L"C:\\WINDOWS\\Greenstone.bmp", 1000, &buffer));
}

// TODO(omaha): Need a test for WriteEntireFile
// TEST(UtilsTest, WriteEntireFile) {
// }

TEST(UtilsTest, ExpandEnvLikeStrings) {
  std::map<CString, CString> mapping;
  ASSERT_SUCCEEDED(Shell::GetSpecialFolderKeywordsMapping(&mapping));

  CString out;
  ASSERT_SUCCEEDED(ExpandEnvLikeStrings(
      L"Foo%WINDOWS%Bar%SYSTEM%Zebra%WINDOWS%%SYSTEM%", mapping, &out));

  // This should work, but CmpHelperSTRCASEEQ is not overloaded for wchars.
  // ASSERT_STRCASEEQ(out, L"FooC:\\WINDOWSBarC:\\WINDOWS\\system32Zebra"
  //                       L"C:\\WINDOWSC:\\WINDOWS\\system32");
  ASSERT_EQ(out.CompareNoCase(L"FooC:\\WINDOWSBarC:\\WINDOWS\\system32Zebra"
                              L"C:\\WINDOWSC:\\WINDOWS\\system32"),
            0);
  ASSERT_FAILED(ExpandEnvLikeStrings(L"Foo%WINDOWS%%BAR%Zebra", mapping, &out));
}

TEST(UtilsTest, GetCurrentProcessHandle) {
  scoped_process proc;
  ASSERT_SUCCEEDED(GetCurrentProcessHandle(address(proc)));
  ASSERT_TRUE(valid(proc));
}

TEST(UtilsTest, DuplicateTokenIntoCurrentProcess) {
  CAccessToken process_token;
  EXPECT_TRUE(process_token.GetProcessToken(TOKEN_ALL_ACCESS));

  CAccessToken duplicated_token;
  EXPECT_SUCCEEDED(DuplicateTokenIntoCurrentProcess(::GetCurrentProcess(),
                                                    process_token.GetHandle(),
                                                    &duplicated_token));

  CSid process_sid;
  EXPECT_TRUE(process_token.GetUser(&process_sid));

  CSid duplicated_sid;
  EXPECT_TRUE(duplicated_token.GetUser(&duplicated_sid));

  EXPECT_STREQ(process_sid.Sid(), duplicated_sid.Sid());
}

TEST(UtilsTest, IsGuid) {
  EXPECT_FALSE(IsGuid(NULL));
  EXPECT_FALSE(IsGuid(_T("")));
  EXPECT_FALSE(IsGuid(_T("{}")));
  EXPECT_FALSE(IsGuid(_T("a")));
  EXPECT_FALSE(IsGuid(_T("CA3045BFA6B14fb8A0EFA615CEFE452C")));

  // Missing {}
  EXPECT_FALSE(IsGuid(_T("CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C")));

  // Invalid char X
  EXPECT_FALSE(IsGuid(_T("{XA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}")));

  // Invalid binary char 0x200
  EXPECT_FALSE(IsGuid(_T("{\0x200a3045bf-a6b1-4fb8-a0ef-a615cefe452c}")));

  // Missing -
  EXPECT_FALSE(IsGuid(_T("{CA3045BFA6B14fb8A0EFA615CEFE452C}")));

  // Double quotes
  EXPECT_FALSE(IsGuid(_T("\"{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}\"")));

  EXPECT_TRUE(IsGuid(_T("{00000000-0000-0000-0000-000000000000}")));
  EXPECT_TRUE(IsGuid(_T("{CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}")));
  EXPECT_TRUE(IsGuid(_T("{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}")));
}

// GUIDs cannot be compared in GTest because there is no << operator. Therefore,
// we must treat them as strings. All these tests rely on GuidToString working.
#define EXPECT_GUID_EQ(expected, actual) \
    EXPECT_STREQ(GuidToString(expected), GuidToString(actual))

TEST(UtilsTest, StringToGuidSafe_InvalidString) {
  GUID guid = {0};

  EXPECT_EQ(E_INVALIDARG, StringToGuidSafe(_T(""), &guid));
  EXPECT_EQ(E_INVALIDARG, StringToGuidSafe(_T("{}"), &guid));
  EXPECT_EQ(E_INVALIDARG, StringToGuidSafe(_T("a"), &guid));
  EXPECT_EQ(E_INVALIDARG,
      StringToGuidSafe(_T("CA3045BFA6B14fb8A0EFA615CEFE452C"), &guid));

  // Missing {}
  EXPECT_EQ(E_INVALIDARG,
      StringToGuidSafe(_T("CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C"), &guid));

  // Invalid char X
  EXPECT_EQ(CO_E_IIDSTRING,
      StringToGuidSafe(_T("{XA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}"), &guid));

  // Invalid binary char 0x200
  EXPECT_EQ(E_INVALIDARG,
            StringToGuidSafe(_T("{\0x200a3045bf-a6b1-4fb8-a0ef-a615cefe452c}"),
                             &guid));

  // Missing -
  EXPECT_EQ(E_INVALIDARG,
            StringToGuidSafe(_T("{CA3045BFA6B14fb8A0EFA615CEFE452C}"), &guid));

  // Double quotes
  EXPECT_EQ(E_INVALIDARG,
            StringToGuidSafe(_T("\"{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}\""),
                             &guid));
}

TEST(UtilsTest, StringToGuidSafe_ValidString) {
  const GUID kExpectedGuid = {0xCA3045BF, 0xA6B1, 0x4FB8,
                              {0xA0, 0xEF, 0xA6, 0x15, 0xCE, 0xFE, 0x45, 0x2C}};
  GUID guid = kExpectedGuid;

  EXPECT_SUCCEEDED(
      StringToGuidSafe(_T("{00000000-0000-0000-0000-000000000000}"), &guid));
  EXPECT_GUID_EQ(GUID_NULL, guid);

  guid = GUID_NULL;
  EXPECT_SUCCEEDED(
      StringToGuidSafe(_T("{CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}"), &guid));
  EXPECT_GUID_EQ(kExpectedGuid, guid);

  guid = GUID_NULL;
  EXPECT_SUCCEEDED(
      StringToGuidSafe(_T("{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}"), &guid));
  EXPECT_GUID_EQ(kExpectedGuid, guid);
}

TEST(UtilsTest, VersionFromString_ValidVersion) {
  EXPECT_EQ(MAKEDLLVERULL(42, 1, 21, 12345),
            VersionFromString(_T("42.1.21.12345")));
}

TEST(UtilsTest, VersionFromString_VersionZero) {
  EXPECT_EQ(0, VersionFromString(_T("0.0.0.0")));
}

TEST(UtilsTest, VersionFromString_VersionUpperLimits) {
  EXPECT_EQ(MAKEDLLVERULL(0xffff, 0xffff, 0xffff, 0xffff),
            VersionFromString(_T("65535.65535.65535.65535")));
  EXPECT_EQ(0, VersionFromString(_T("65536.65536.65536.65536")));
  EXPECT_EQ(0, VersionFromString(_T("1.2.65536.65536")));
}

TEST(UtilsTest, VersionFromString_IntegerOverflow) {
  EXPECT_EQ(0, VersionFromString(_T("1.2.3.4294967296")));
}

TEST(UtilsTest, VersionFromString_NegativeVersion) {
  EXPECT_EQ(0, VersionFromString(_T("1.2.3.-22")));
}

TEST(UtilsTest, VersionFromString_TooFewElements) {
  EXPECT_EQ(0, VersionFromString(_T("1.1.1")));
}

TEST(UtilsTest, VersionFromString_ExtraPeriod) {
  EXPECT_EQ(0, VersionFromString(_T("1.1.2.3.")));
}

TEST(UtilsTest, VersionFromString_TooManyElements) {
  EXPECT_EQ(0, VersionFromString(_T("1.1.2.3.4")));
}

TEST(UtilsTest, VersionFromString_Char) {
  EXPECT_EQ(0, VersionFromString(_T("1.B.3.4")));
  EXPECT_EQ(0, VersionFromString(_T("1.2.3.B")));
  EXPECT_EQ(0, VersionFromString(_T("1.2.3.9B")));
}

TEST(UtilsTest, StringFromVersion_ValidVersion) {
  EXPECT_STREQ(_T("42.1.21.12345"),
               StringFromVersion(MAKEDLLVERULL(42, 1, 21, 12345)));
}

TEST(UtilsTest, StringFromVersion_VersionZero) {
  EXPECT_STREQ(_T("0.0.0.0"), StringFromVersion(0));
}

TEST(UtilsTest, StringFromVersion_VersionUpperLimits) {
  EXPECT_STREQ(
      _T("65535.65535.65535.65535"),
      StringFromVersion(MAKEDLLVERULL(0xffff, 0xffff, 0xffff, 0xffff)));
}

TEST(UtilsTest, IsLocalSystemSid) {
  EXPECT_TRUE(IsLocalSystemSid(kLocalSystemSid));
  EXPECT_TRUE(IsLocalSystemSid(_T("S-1-5-18")));
  EXPECT_TRUE(IsLocalSystemSid(_T("s-1-5-18")));

  EXPECT_FALSE(IsLocalSystemSid(_T("")));
  EXPECT_FALSE(IsLocalSystemSid(_T("S-1-5-17")));
}

// Counts instances of the class.
class Counter {
 public:
  Counter() {
    ++instance_count_;
  }
  ~Counter() {
    --instance_count_;
  }
  static int instance_count() { return instance_count_; }
 private:
  static int instance_count_;
  DISALLOW_COPY_AND_ASSIGN(Counter);
};

int Counter::instance_count_ = 0;

// Checks if the functor is actually calling the destructor of the type.
TEST(UtilsTest, DeleteFun) {
  EXPECT_EQ(Counter::instance_count(), 0);
  Counter* counter = new Counter;
  EXPECT_EQ(Counter::instance_count(), 1);
  DeleteFun().operator()(counter);
  EXPECT_EQ(Counter::instance_count(), 0);

  // Checks if the template can be instantiated for some common built in types.
  int* pointer_int = NULL;
  DeleteFun().operator()(pointer_int);

  const char* pointer_char = NULL;
  DeleteFun().operator()(pointer_char);
}

TEST(UtilsTest, IsUserLoggedOn) {
  bool is_logged_on(false);
  ASSERT_HRESULT_SUCCEEDED(IsUserLoggedOn(&is_logged_on));
  ASSERT_TRUE(is_logged_on);
}

TEST(UtilsTest, ConfigureRunAtStartup) {
  const TCHAR kRunKeyPath[] =
      _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);

  EXPECT_FALSE(RegKey::HasKey(kRunKeyPath));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            ConfigureRunAtStartup(USER_KEY_NAME, _T("FooApp"),
                                  _T("\"foo.exe\""), false));
  EXPECT_FALSE(RegKey::HasKey(kRunKeyPath));

  EXPECT_SUCCEEDED(ConfigureRunAtStartup(USER_KEY_NAME, _T("FooApp"),
                                         _T("\"C:\\foo.exe\" /x"), true));
  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKeyPath, _T("FooApp"), &value));
  EXPECT_STREQ(_T("\"C:\\foo.exe\" /x"), value);

  EXPECT_SUCCEEDED(ConfigureRunAtStartup(USER_KEY_NAME, _T("FooApp"),
                                         _T("\"foo.exe\""), false));
  EXPECT_FALSE(RegKey::HasValue(kRunKeyPath, _T("FooApp")));
  EXPECT_TRUE(RegKey::HasKey(kRunKeyPath));

  RestoreRegistryHives();
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST(UtilsTest, ValidPath) {
  CString cmd_line =
      _T("\"C:\\Program Files\\Internet Explorer\\iexplore.exe\" -nohome");
  CString exe_path;
  EXPECT_SUCCEEDED(GetExePathFromCommandLine(cmd_line, &exe_path));
  EXPECT_STREQ(_T("C:\\Program Files\\Internet Explorer\\iexplore.exe"),
               exe_path);
}

TEST(UtilsTest, InvalidPath) {
  CString cmd_line = _T("");
  CString exe_path;
  EXPECT_FAILED(GetExePathFromCommandLine(cmd_line, &exe_path));
  EXPECT_TRUE(exe_path.IsEmpty());
}

// Assumes Windows is installed on the C: drive.
TEST(UtilsTest, GetEnvironmentVariableAsString) {
  EXPECT_STREQ(_T("C:"), GetEnvironmentVariableAsString(_T("SystemDrive")));
  EXPECT_STREQ(_T("Windows_NT"), GetEnvironmentVariableAsString(_T("OS")));
  EXPECT_STREQ(_T(""), GetEnvironmentVariableAsString(_T("FOO")));
}

TEST(UtilsTest, IsWindowsInstalling_Normal) {
  EXPECT_FALSE(IsWindowsInstalling());
}

TEST(UtilsTest, IsWindowsInstalling_Installing_Vista_InvalidValues) {
  if (!vista_util::IsVistaOrLater()) {
    return;
  }

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("")));
  EXPECT_FALSE(IsWindowsInstalling());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("foo")));
  EXPECT_FALSE(IsWindowsInstalling());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      static_cast<DWORD>(1)));
  ExpectAsserts expect_asserts;  // RegKey asserts because value type is wrong.
  EXPECT_FALSE(IsWindowsInstalling());

  RestoreRegistryHives();
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST(UtilsTest, IsWindowsInstalling_Installing_Vista_ValidStates) {
  if (!vista_util::IsVistaOrLater()) {
    return;
  }

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);

  // These states return false in the original implementation.
  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("IMAGE_STATE_COMPLETE")));
  EXPECT_FALSE(IsWindowsInstalling());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("IMAGE_STATE_GENERALIZE_RESEAL_TO_OOBE")));
  EXPECT_FALSE(IsWindowsInstalling());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("IMAGE_STATE_SPECIALIZE_RESEAL_TO_OOBE")));
  EXPECT_FALSE(IsWindowsInstalling());

  // These states are specified in the original implementation.
  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("IMAGE_STATE_UNDEPLOYABLE")));
  EXPECT_TRUE(IsWindowsInstalling());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("IMAGE_STATE_GENERALIZE_RESEAL_TO_AUDIT")));
  EXPECT_TRUE(IsWindowsInstalling());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
      _T("ImageState"),
      _T("IMAGE_STATE_SPECIALIZE_RESEAL_TO_AUDIT")));
  EXPECT_TRUE(IsWindowsInstalling());

  RestoreRegistryHives();
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST(UtilsTest, GetCurrentUserDefaultSecurityAttributes) {
  CSecurityAttributes sa;
  EXPECT_TRUE(GetCurrentUserDefaultSecurityAttributes(&sa));
}

TEST(UtilsTest, AddAllowedAce) {
  const CString test_file_path(GetTempFilename(_T("AddAllowedAce_")));

  CDacl dacl;
  EXPECT_TRUE(AtlGetDacl(test_file_path, SE_FILE_OBJECT, &dacl));
  const int original_ace_count = dacl.GetAceCount();

  EXPECT_SUCCEEDED(AddAllowedAce(test_file_path,
                                 SE_FILE_OBJECT,
                                 Sids::Dialup(),
                                 FILE_GENERIC_WRITE,
                                 0));

  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(test_file_path, SE_FILE_OBJECT, &dacl));
  EXPECT_EQ(original_ace_count + 1, dacl.GetAceCount());

  // Add the same access. No ACE is added.
  EXPECT_SUCCEEDED(AddAllowedAce(test_file_path,
                                 SE_FILE_OBJECT,
                                 Sids::Dialup(),
                                 FILE_GENERIC_WRITE,
                                 0));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(test_file_path, SE_FILE_OBJECT, &dacl));
  EXPECT_EQ(original_ace_count + 1, dacl.GetAceCount());

  // Add a subset of the existing access. No ACE is added.
  EXPECT_EQ(FILE_WRITE_ATTRIBUTES, FILE_GENERIC_WRITE & FILE_WRITE_ATTRIBUTES);
  EXPECT_SUCCEEDED(AddAllowedAce(test_file_path,
                                 SE_FILE_OBJECT,
                                 Sids::Dialup(),
                                 FILE_WRITE_ATTRIBUTES,
                                 0));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(test_file_path, SE_FILE_OBJECT, &dacl));
  EXPECT_EQ(original_ace_count + 1, dacl.GetAceCount());

  // Add more access. An ACE is added.
  EXPECT_SUCCEEDED(AddAllowedAce(test_file_path,
                                 SE_FILE_OBJECT,
                                 Sids::Dialup(),
                                 FILE_ALL_ACCESS,
                                 0));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(test_file_path, SE_FILE_OBJECT, &dacl));
  EXPECT_EQ(original_ace_count + 2, dacl.GetAceCount());

  EXPECT_SUCCEEDED(File::Remove(test_file_path));
}

TEST(UtilsTest, AddAllowedAceRegistry) {
  const CString object_name = _T("CURRENT_USER\\SOFTWARE\\") GOOPDATE_MAIN_KEY
      _T("AddAllowedAceRegistryTest");

  // The registry and DACL security APIs expect different prefixes for the
  // registry hives.
  CString registry_key_name = object_name;
  registry_key_name.Replace(_T("CURRENT_USER"), _T("HKCU"));
  EXPECT_SUCCEEDED(RegKey::CreateKey(registry_key_name));

  CDacl dacl;
  EXPECT_TRUE(AtlGetDacl(object_name, SE_REGISTRY_KEY, &dacl));
  const int original_ace_count = dacl.GetAceCount();

  // Add more access. An ACE is added.
  EXPECT_SUCCEEDED(AddAllowedAce(object_name,
                                 SE_REGISTRY_KEY,
                                 Sids::Dialup(),
                                 KEY_ALL_ACCESS,
                                 0));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(object_name, SE_REGISTRY_KEY, &dacl));
  EXPECT_EQ(original_ace_count + 1, dacl.GetAceCount());

  // Different ACE flags. An ACE is added.
  const BYTE kTestAce = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
  const BYTE kTestAceSubset = CONTAINER_INHERIT_ACE;
  EXPECT_SUCCEEDED(AddAllowedAce(object_name,
                                 SE_REGISTRY_KEY,
                                 Sids::Dialup(),
                                 KEY_ALL_ACCESS,
                                 kTestAce));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(object_name, SE_REGISTRY_KEY, &dacl));
  EXPECT_EQ(original_ace_count + 2, dacl.GetAceCount());

  // Subset of existing ACE flags. An ACE is added because flags must be exact.
  EXPECT_SUCCEEDED(AddAllowedAce(object_name,
                                 SE_REGISTRY_KEY,
                                 Sids::Dialup(),
                                 KEY_ALL_ACCESS,
                                 kTestAceSubset));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(object_name, SE_REGISTRY_KEY, &dacl));
  EXPECT_EQ(original_ace_count + 3, dacl.GetAceCount());

  // Same flags. An ACE should not be added because all values match.
  EXPECT_SUCCEEDED(AddAllowedAce(object_name,
                                 SE_REGISTRY_KEY,
                                 Sids::Dialup(),
                                 KEY_ALL_ACCESS,
                                 kTestAceSubset));
  dacl.SetEmpty();
  EXPECT_TRUE(AtlGetDacl(object_name, SE_REGISTRY_KEY, &dacl));
  EXPECT_EQ(original_ace_count + 3, dacl.GetAceCount());

  EXPECT_SUCCEEDED(RegKey::DeleteKey(registry_key_name));
}

TEST(UtilsTest, CreateForegroundParentWindowForUAC) {
  CWindow foreground_parent;
  foreground_parent.Attach(CreateForegroundParentWindowForUAC());
  EXPECT_TRUE(foreground_parent.IsWindow());
  EXPECT_TRUE(foreground_parent.IsWindowVisible());

  CRect foreground_rect;
  EXPECT_TRUE(foreground_parent.GetWindowRect(&foreground_rect));
  EXPECT_EQ(0, foreground_rect.Width());
  EXPECT_EQ(0, foreground_rect.Height());

  EXPECT_TRUE((WS_POPUP | WS_VISIBLE) & foreground_parent.GetStyle());
  EXPECT_TRUE(WS_EX_TOOLWINDOW & foreground_parent.GetExStyle());

  EXPECT_TRUE(foreground_parent.DestroyWindow());
}

// Test the atomic exchange of pointer values.
TEST(UtilsTest, interlocked_exchange_pointer) {
  const int i = 10;
  const int j = 20;

  const int* pi = &i;
  const int* pj = &j;

  const int* old_pi = pi;

  // pi and pj point to i and j respectively.
  EXPECT_EQ(10, *pi);
  EXPECT_EQ(20, *pj);

  // After the exchange pi<-pj, both pointers point to the same value, in this
  // case j.
  int* result = interlocked_exchange_pointer(const_cast<int**>(&pi), pj);
  EXPECT_EQ(*pj, *pi);
  EXPECT_EQ(old_pi, result);
  EXPECT_EQ(10, *old_pi);
  EXPECT_EQ(20, *pi);

  // Exchanging a pointer with self is idempotent.
  old_pi = interlocked_exchange_pointer(const_cast<int**>(&pi), pi);
  EXPECT_EQ(pi, old_pi);
  EXPECT_EQ(20, *pi);

  // Exchanging a pointer with NULL.
  interlocked_exchange_pointer(const_cast<int**>(&pi), static_cast<int*>(NULL));
  EXPECT_EQ(static_cast<int*>(NULL), pi);
}

TEST(UtilsTest, GetGuid) {
  CString guid;
  EXPECT_HRESULT_SUCCEEDED(GetGuid(&guid));

  IID iid = {0};
  EXPECT_HRESULT_SUCCEEDED(::IIDFromString(guid, &iid));

  // Missing {}.
  EXPECT_HRESULT_FAILED(
      ::IIDFromString(_T("5F5280C6-9674-429b-9FEB-551914EF96B8"), &iid));

  // Missing -.
  EXPECT_HRESULT_FAILED(
      ::IIDFromString(_T("{5F5280C6.9674-429b-9FEB-551914EF96B8}"), &iid));

  // Whitespaces.
  EXPECT_HRESULT_FAILED(
      ::IIDFromString(_T(" {5F5280C6-9674-429b-9FEB-551914EF96B8}"), &iid));
  EXPECT_HRESULT_FAILED(
      ::IIDFromString(_T("{5F5280C6-9674-429b-9FEB-551914EF96B8} "), &iid));

  // Empty string.
    EXPECT_HRESULT_FAILED(::IIDFromString(_T(""), &iid));
}

TEST(UtilsTest, GetMessageForSystemErrorCode) {
  CString message = GetMessageForSystemErrorCode(99);
  EXPECT_TRUE(message.IsEmpty());

  message = GetMessageForSystemErrorCode(ERROR_TOO_MANY_SEMAPHORES);
  EXPECT_FALSE(message.IsEmpty());
}

TEST(UtilsTest, CeilingDivide) {
  EXPECT_EQ(0, CeilingDivide(0, 1));
  EXPECT_EQ(1, CeilingDivide(1, 1));
  EXPECT_EQ(1, CeilingDivide(1, 2));
  EXPECT_EQ(2, CeilingDivide(6, 3));
  EXPECT_EQ(4, CeilingDivide(7, 2));
}

TEST(UtilsTest, GetTempFilename) {
  CString name1 = GetTempFilename(kTemporaryFilenamePrefix);
  EXPECT_FALSE(name1.IsEmpty());
  EXPECT_NE(-1, name1.Find(kTemporaryFilenamePrefix));
  EXPECT_TRUE(File::Exists(name1));

  CString name2 = GetTempFilename(kTemporaryFilenamePrefix);
  EXPECT_NE(name1, name2);

  EXPECT_TRUE(::DeleteFile(name1));
  EXPECT_TRUE(::DeleteFile(name2));
}

TEST(UtilsTest, WaitForAllObjects) {
  const DWORD kTimeoutMs = 100;
  const size_t kNumHandles = MAXIMUM_WAIT_OBJECTS * 2 + 1;

  // Create a large number of manual reset events to test with, all signaled.
  HANDLE handles[kNumHandles] = {};
  for (size_t i = 0; i < arraysize(handles); ++i) {
    handles[i] = ::CreateEvent(NULL, TRUE, TRUE, NULL);
    ASSERT_TRUE(handles[i]);
  }

  // Succeed if they're all signaled, regardless of timeout.
  EXPECT_EQ(WAIT_OBJECT_0, WaitForAllObjects(arraysize(handles),
                                             handles,
                                             kTimeoutMs));

  EXPECT_EQ(WAIT_OBJECT_0, WaitForAllObjects(arraysize(handles),
                                             handles,
                                             0));

  EXPECT_EQ(WAIT_OBJECT_0, WaitForAllObjects(arraysize(handles),
                                             handles,
                                             INFINITE));

  // Set a single object in the first group to unsignaled; we should time out.
  ::ResetEvent(handles[0]);
  EXPECT_EQ(WAIT_TIMEOUT, WaitForAllObjects(arraysize(handles),
                                            handles,
                                            kTimeoutMs));
  ::SetEvent(handles[0]);

  // Set a single object in a later group to unsignaled; we should time out.
  ::ResetEvent(handles[arraysize(handles) - 1]);
  EXPECT_EQ(WAIT_TIMEOUT, WaitForAllObjects(arraysize(handles),
                                            handles,
                                            kTimeoutMs));
  ::SetEvent(handles[arraysize(handles) - 1]);

  for (size_t i = 0; i < arraysize(handles); ++i) {
    ::CloseHandle(handles[i]);
  }
}

TEST(UtilsTest, IsEnrolledToDomain_TRUE) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    1UL));

  EXPECT_TRUE(IsEnrolledToDomain());

  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                       kRegValueIsEnrolledToDomain));
}

TEST(UtilsTest, IsEnrolledToDomain_FALSE) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    0UL));

  EXPECT_FALSE(IsEnrolledToDomain());

  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                       kRegValueIsEnrolledToDomain));
}

TEST(UtilsTest, DeleteDirectoryContents_DirDoesNotExist) {
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND),
            DeleteDirectoryContents(_T("##")));
}

TEST(UtilsTest, DeleteDirectoryContents_InvalidDirName) {
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_NAME),
            DeleteDirectoryContents(_T("*")));
}

TEST(UtilsTest, DeleteDirectoryContents_NoFiles) {
  const CString source_dir = GetUniqueTempDirectoryName();
  EXPECT_SUCCEEDED(CreateDir(source_dir, NULL));
  EXPECT_TRUE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectoryContents(source_dir));

  EXPECT_TRUE(File::Exists(source_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(source_dir));
}

TEST(UtilsTest, DeleteDirectoryContents_OnlyFilesNoDirs) {
  const CString source_dir = GetUniqueTempDirectoryName();
  EXPECT_SUCCEEDED(CreateDir(source_dir, NULL));

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8876.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_NORMAL, },
  };

  CreateFiles(source_dir, kFiles, arraysize(kFiles));
  EXPECT_FALSE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectoryContents(source_dir));

  EXPECT_TRUE(File::Exists(source_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(source_dir));
}

TEST(UtilsTest, DeleteDirectoryContents_MixedAttributeFiles) {
  const CString source_dir = GetUniqueTempDirectoryName();
  EXPECT_SUCCEEDED(CreateDir(source_dir, NULL));

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_SYSTEM, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_HIDDEN, },
  };

  CreateFiles(source_dir, kFiles, arraysize(kFiles));
  EXPECT_FALSE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectoryContents(source_dir));

  EXPECT_TRUE(File::Exists(source_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(source_dir));
}

TEST(UtilsTest, DeleteDirectoryContents_ReadOnlyFiles) {
  const CString source_dir = GetUniqueTempDirectoryName();
  EXPECT_SUCCEEDED(CreateDir(source_dir, NULL));

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_SYSTEM, },
    { _T("tst8876.tmp"), FILE_ATTRIBUTE_READONLY, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_HIDDEN, },
  };

  CreateFiles(source_dir, kFiles, arraysize(kFiles));
  EXPECT_FALSE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectoryContents(source_dir));

  EXPECT_TRUE(File::Exists(source_dir));
  EXPECT_FALSE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(source_dir));
}

TEST(UtilsTest, DeleteDirectoryContents_FilesAndDirs) {
  const CString source_dir = GetUniqueTempDirectoryName();
  EXPECT_SUCCEEDED(CreateDir(source_dir, NULL));

  const TCHAR* const kDirectories[] = {
    _T("DirB"),
    _T("DirB\\DirC"),
    _T("DirB\\DirC\\DirD"),
    _T("DirB\\DirC\\DirE"),
    _T("DirB\\DirF"),
    _T("DirB\\DirF\\DirG"),
    _T("DirB\\DirF\\DirH"),
    _T("DirI"),
    _T("DirI\\DirJ"),
    _T("DirI\\DirJ\\DirK"),
    _T("DirI\\DirJ\\DirL"),
    _T("DirI\\DirM"),
    _T("DirI\\DirM\\DirN"),
    _T("DirI\\DirM\\DirO"),
  };

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8876.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\tst8888.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\tst8898.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\tst88A9.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirC\\tst88C9.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirC\\tst88DA.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirC\\tst88FA.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirF\\tst8959.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirF\\tst8969.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirF\\tst898A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\tst89E8.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\tst89F9.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\tst8A0A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirJ\\tst8A2A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirJ\\tst8A3A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirJ\\tst8A5B.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirM\\tst8A8B.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirM\\tst8A9B.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirM\\tst8A9C.tmp"), FILE_ATTRIBUTE_NORMAL, },
  };

  CreateDirs(source_dir, kDirectories, arraysize(kDirectories));
  CreateFiles(source_dir, kFiles, arraysize(kFiles));

  EXPECT_FALSE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectoryContents(source_dir));

  EXPECT_TRUE(File::Exists(source_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(source_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(source_dir));
}

TEST(UtilsTest, LoadSystemLibrary) {
 scoped_library winhttp_dll(LoadSystemLibrary(_T("winhttp.dll")));
 EXPECT_TRUE(winhttp_dll);
 scoped_library no_dll(LoadSystemLibrary(_T("no_such_dll.dll")));
 EXPECT_FALSE(no_dll);
}

}  // namespace omaha

