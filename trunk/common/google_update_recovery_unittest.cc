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
// Unit tests for the Google Update recovery mechanism.
// All apps that are using the mechanism must also run this test.
//
// Unlike the mechanism code, this code relies on code from common because it
// makes writing the tests much simpler and size is not a concern.

#include <windows.h>
#include <atlstr.h>
#include "omaha/common/app_util.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/google_update_recovery.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/signaturevalidator.h"
#include "omaha/common/system_info.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/net/browser_request.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/third_party/gtest/include/gtest/gtest.h"

namespace omaha {

namespace {

const TCHAR kDummyAppGuid[] = _T("{8E472B0D-3E8B-43b1-B89A-E8506AAF1F16}");
const TCHAR kDummyAppVersion[] = _T("3.4.5.6");
const TCHAR kDummyAppLang[] = _T("en-us");
const TCHAR kDummyMachineId[] = _T("{12A7B304-B4ED-4b5c-8122-31E7ECB8BE3C}");
const TCHAR kDummyUserId[] = _T("{29388E49-2995-437c-BF47-587C7E9247E5}");

const TCHAR kTempDirectory[] = _T("C:\\WINDOWS\\Temp");

const TCHAR kFullMachineOmahaMainKeyPath[] =
    _T("HKLM\\Software\\Google\\Update\\");
const TCHAR kFullUserOmahaMainKeyPath[] =
    _T("HKCU\\Software\\Google\\Update\\");
const TCHAR kFullMachineOmahaClientKeyPath[] =
    _T("HKLM\\Software\\Google\\Update\\Clients\\")
    _T("{430FD4D0-B729-4f61-AA34-91526481799D}");
const TCHAR kFullUserOmahaClientKeyPath[] =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{430FD4D0-B729-4f61-AA34-91526481799D}");

const HRESULT kDummyNoFileError = 0x80041234;

const TCHAR kArgumentSavingExecutableRelativePath[] =
    _T("unittest_support\\SaveArguments.exe");
const TCHAR kSavedArgumentsFileName[] = _T("saved_arguments.txt");
const TCHAR* const kInvalidFileAddress = _T("http://www.google.com/robots.txt");

#define MACHINE_KEY_NAME _T("HKLM")
#define MACHINE_KEY MACHINE_KEY_NAME _T("\\")
#define USER_KEY_NAME _T("HKCU")
#define USER_KEY USER_KEY_NAME _T("\\")

// These methods were copied from omaha/testing/omaha_unittest.cpp.
const TCHAR kRegistryHiveOverrideRoot[] =
    _T("HKCU\\Software\\Google\\Update\\UnitTest\\");

// Overrides the HKLM and HKCU registry hives so that accesses go to the
// specified registry key instead.
// This method is most often used in SetUp().
void OverrideRegistryHives(const CString& hive_override_key_name) {
  // Override the destinations of HKLM and HKCU to use a special location
  // for the unit tests so that we don't disturb the actual Omaha state.
  RegKey machine_key;
  RegKey user_key;
  ASSERT_HRESULT_SUCCEEDED(
      machine_key.Create(hive_override_key_name + MACHINE_KEY));
  ASSERT_HRESULT_SUCCEEDED(user_key.Create(hive_override_key_name + USER_KEY));
  ASSERT_HRESULT_SUCCEEDED(::RegOverridePredefKey(HKEY_LOCAL_MACHINE,
                                                  machine_key.Key()));
  ASSERT_HRESULT_SUCCEEDED(::RegOverridePredefKey(HKEY_CURRENT_USER,
                                                  user_key.Key()));
}

// Restores HKLM and HKCU registry accesses to the real hives.
// This method is most often used in TearDown().
void RestoreRegistryHives() {
  ASSERT_HRESULT_SUCCEEDED(::RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL));
  ASSERT_HRESULT_SUCCEEDED(::RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
}

CString GetTmp() {
  TCHAR temp_dir[MAX_PATH] = {0};
  EXPECT_NE(0, ::GetEnvironmentVariable(_T("TMP"), temp_dir, MAX_PATH));
  return temp_dir;
}

}  // namespace

HRESULT VerifyFileSignature(const CString& filename);
HRESULT VerifyRepairFileMarkup(const CString& filename);

class GoogleUpdateRecoveryTest : public testing::Test {
 public:
  static void set_saved_url(const CString& saved_url) {
    saved_url_ = saved_url;
  }

  static void set_saved_file_path(const CString& saved_file_path) {
    saved_file_path_ = saved_file_path;
  }

  static void set_saved_context(void* context) {
    saved_context_ = context;
  }

 protected:
  GoogleUpdateRecoveryTest() {
    saved_url_.Empty();
    saved_file_path_.Empty();
    saved_context_ = NULL;
  }

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  void CheckSavedUrlOSFragment() {
    const TCHAR kExpectedOSXpSp2[] = _T("5.1&servicepack=Service%20Pack%202");
    const TCHAR kExpectedOSXpSp3[] = _T("5.1&servicepack=Service%20Pack%203");
    const TCHAR kExpectedOS2003Sp1[] = _T("5.2&servicepack=Service%20Pack%201");
    const TCHAR kExpectedOS2003Sp2[] = _T("5.2&servicepack=Service%20Pack%202");
    const TCHAR kExpectedOSVistaRtm[] = _T("6.0&servicepack=");
    const TCHAR kExpectedOSVistaSp1[] =
        _T("6.0&servicepack=Service%20Pack%201");

    EXPECT_TRUE((kExpectedOSXpSp2 ==
                 saved_url_.Right(arraysize(kExpectedOSXpSp2) - 1)) ||
                (kExpectedOSXpSp3 ==
                 saved_url_.Right(arraysize(kExpectedOSXpSp3) - 1)) ||
                (kExpectedOS2003Sp1 ==
                 saved_url_.Right(arraysize(kExpectedOS2003Sp1) - 1)) ||
                (kExpectedOS2003Sp2 ==
                 saved_url_.Right(arraysize(kExpectedOS2003Sp2) - 1)) ||
                (kExpectedOSVistaRtm ==
                 saved_url_.Right(arraysize(kExpectedOSVistaRtm) - 1)) ||
                (kExpectedOSVistaSp1 ==
                 saved_url_.Right(arraysize(kExpectedOSVistaSp1) - 1)));
  }

  void VerifySavedArgumentsFile(const CString& expected_string) {
    CString saved_arguments_path = ConcatenatePath(
                                       GetDirectoryFromPath(saved_file_path_),
                                       kSavedArgumentsFileName);
    bool is_found = false;
    for (int tries = 0; tries < 100 && !is_found; ++tries) {
      ::Sleep(50);
      is_found = File::Exists(saved_arguments_path);
    }
    ASSERT_TRUE(is_found);

    scoped_hfile file(::CreateFile(saved_arguments_path,
                                   GENERIC_READ,
                                   0,                     // do not share
                                   NULL,                  // default security
                                   OPEN_EXISTING,         // existing file only
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL));                // no template
    ASSERT_NE(INVALID_HANDLE_VALUE, get(file));

    const int kBufferLen = 50;
    TCHAR buffer[kBufferLen + 1] = {0};
    DWORD bytes_read = 0;

    EXPECT_TRUE(::ReadFile(get(file),
                           buffer,
                           kBufferLen * sizeof(TCHAR),
                           &bytes_read,
                           NULL));
    EXPECT_EQ(0, bytes_read % sizeof(TCHAR));
    buffer[bytes_read / sizeof(TCHAR)] = _T('\0');

    EXPECT_STREQ(expected_string, buffer);
  }

  void VerifyExpectedSavedFilePath(const CString& expected_temp_directory) {
    const int kMaxUniqueChars = 4;
    const CString expected_path_part_a = expected_temp_directory + _T("\\GUR");
    const CString expected_path_part_b = _T(".exe");
    EXPECT_STREQ(expected_path_part_a,
                 saved_file_path_.Left(expected_path_part_a.GetLength()));
    EXPECT_STREQ(expected_path_part_b,
                 saved_file_path_.Right(expected_path_part_b.GetLength()));
    const int constant_chars = expected_path_part_a.GetLength() +
                               expected_path_part_b.GetLength();
    EXPECT_GT(saved_file_path_.GetLength(), constant_chars);
    EXPECT_LE(saved_file_path_.GetLength(), constant_chars + kMaxUniqueChars);
  }

  static CString saved_url_;
  static CString saved_file_path_;
  static void* saved_context_;

 protected:
  // Copies SaveArguments.exe to the specified location.
  static HRESULT DownloadArgumentSavingFile(const TCHAR* url,
                                            const TCHAR* file_path,
                                            void* context) {
    ASSERT1(url);
    ASSERT1(file_path);

    GoogleUpdateRecoveryTest::set_saved_url(url);
    GoogleUpdateRecoveryTest::set_saved_file_path(file_path);
    GoogleUpdateRecoveryTest::set_saved_context(context);

    CString executable_full_path(app_util::GetCurrentModuleDirectory());
    VERIFY1(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                         kArgumentSavingExecutableRelativePath));

    if (!::CopyFile(executable_full_path, file_path, false)) {
      HRESULT hr = HRESULTFromLastError();
      return hr;
    }

    return S_OK;
  }

  // Returns kDummyNoFileError, simulating no file to download.
  static HRESULT DownloadFileNoFile(const TCHAR* url,
                                    const TCHAR* file_path,
                                    void* context) {
    ASSERT1(url);
    ASSERT1(file_path);

    GoogleUpdateRecoveryTest::set_saved_url(url);
    GoogleUpdateRecoveryTest::set_saved_file_path(file_path);
    GoogleUpdateRecoveryTest::set_saved_context(context);

    return kDummyNoFileError;
  }

  // Overrides the address to cause a file to be downloaded via HTTP.
  // Uses a real HTTP stack, so it is similar to a real implementation.
  // The file is invalid, so signature verification should return
  // TRUST_E_SUBJECT_FORM_UNKNOWN.
  static HRESULT DownloadFileInvalidFile(const TCHAR* url,
                                         const TCHAR* file_path,
                                         void* context) {
    ASSERT1(url);
    CString test_url(url);

    VERIFY1(1 == test_url.Replace(kUrlCodeRedCheck, kInvalidFileAddress));

    return DownloadFileFromServer(test_url, file_path, context);
  }

  // Uses a real HTTP stack, so it is similar to a real implementation.
  static HRESULT DownloadFileFromServer(const TCHAR* url,
                                        const TCHAR* file_path,
                                        void* context) {
    UTIL_LOG(L2, (_T("[DownloadFileFromServer][%s][%s]"), url, file_path));

    ASSERT1(url);
    ASSERT1(file_path);

    GoogleUpdateRecoveryTest::set_saved_url(url);
    GoogleUpdateRecoveryTest::set_saved_file_path(file_path);
    GoogleUpdateRecoveryTest::set_saved_context(context);

    NetworkConfig& network_config = NetworkConfig::Instance();
    ON_SCOPE_EXIT_OBJ(network_config, &NetworkConfig::Clear);

    network_config.Clear();
    network_config.Add(new GoogleProxyDetector(MACHINE_REG_UPDATE_DEV));
    network_config.Add(new FirefoxProxyDetector());
    network_config.Add(new IEProxyDetector());

    NetworkRequest network_request(network_config.session());
    network_request.AddHttpRequest(new SimpleRequest);
    network_request.AddHttpRequest(new BrowserRequest);

    HRESULT hr = network_request.DownloadFile(url, CString(file_path));
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[DownloadFile failed][%s][0x%08x]"), url, hr));
      return hr;
    }

    int status_code = network_request.http_status_code();
    UTIL_LOG(L2, (_T("[HTTP status][%u]"), status_code));

    if (HTTP_STATUS_OK == status_code) {
      return S_OK;
    } else if (HTTP_STATUS_NO_CONTENT == status_code) {
      return kDummyNoFileError;
    } else {
      // Apps would not have this assumption.
      ASSERT(false, (_T("Status code %i received. Expected 200 or 204."),
                     status_code));
      return E_FAIL;
    }
  }
};

CString GoogleUpdateRecoveryTest::saved_url_;
CString GoogleUpdateRecoveryTest::saved_file_path_;
void* GoogleUpdateRecoveryTest::saved_context_;

class GoogleUpdateRecoveryRegistryProtectedTest
    : public GoogleUpdateRecoveryTest {
 protected:
  GoogleUpdateRecoveryRegistryProtectedTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  CString hive_override_key_name_;

  virtual void SetUp() {
    GoogleUpdateRecoveryTest::SetUp();
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_HRESULT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
    GoogleUpdateRecoveryTest::TearDown();
  }
};

//
// FixGoogleUpdate Tests
//

TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_UseRealHttpClient) {
  EXPECT_EQ(TRUST_E_SUBJECT_FORM_UNKNOWN,
            FixGoogleUpdate(kDummyAppGuid,
                            kDummyAppVersion,
                            kDummyAppLang,
                            true,
                            DownloadFileInvalidFile,
                            NULL));
}

TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_FileReturned_Machine) {
  CString saved_arguments_path = ConcatenatePath(app_util::GetTempDir(),
                                                 kSavedArgumentsFileName);

  ::DeleteFile(saved_arguments_path);
  ASSERT_FALSE(File::Exists(saved_arguments_path));

  CString context_string(_T("some context"));
  EXPECT_HRESULT_SUCCEEDED(FixGoogleUpdate(kDummyAppGuid,
                                           kDummyAppVersion,
                                           kDummyAppLang,
                                           true,
                                           DownloadArgumentSavingFile,
                                           &context_string));

  EXPECT_EQ(&context_string, saved_context_);
  EXPECT_STREQ(_T("some context"), *static_cast<CString*>(saved_context_));

  ::Sleep(200);
  EXPECT_TRUE(File::Exists(saved_file_path_));
  VerifySavedArgumentsFile(_T("/recover /machine"));

  EXPECT_TRUE(::DeleteFile(saved_file_path_));
  EXPECT_TRUE(::DeleteFile(saved_arguments_path));
}

TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_FileReturned_User) {
  CString saved_arguments_path = ConcatenatePath(app_util::GetTempDir(),
                                                 kSavedArgumentsFileName);

  ::DeleteFile(saved_arguments_path);
  ASSERT_FALSE(File::Exists(saved_arguments_path));

  CString context_string(_T("more context"));
  EXPECT_HRESULT_SUCCEEDED(FixGoogleUpdate(kDummyAppGuid,
                                           kDummyAppVersion,
                                           kDummyAppLang,
                                           false,
                                           DownloadArgumentSavingFile,
                                           &context_string));

  EXPECT_EQ(&context_string, saved_context_);
  EXPECT_STREQ(_T("more context"), *static_cast<CString*>(saved_context_));

  ::Sleep(200);
  EXPECT_TRUE(File::Exists(saved_file_path_));
  VerifySavedArgumentsFile(_T("/recover"));

  EXPECT_TRUE(::DeleteFile(saved_file_path_));
  EXPECT_TRUE(::DeleteFile(saved_arguments_path));
}

TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_NoFile_Machine) {
  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(kDummyAppGuid,
                                               kDummyAppVersion,
                                               kDummyAppLang,
                                               true,
                                               DownloadFileNoFile,
                                               NULL));

  EXPECT_EQ(static_cast<void*>(NULL), saved_context_);
  EXPECT_FALSE(File::Exists(saved_file_path_));

  TCHAR temp_dir[MAX_PATH] = {0};
  EXPECT_TRUE(::GetEnvironmentVariable(_T("TMP"), temp_dir, MAX_PATH));
  EXPECT_TRUE(File::Exists(temp_dir))
      << _T("The temp directory was deleted or not created.");
}

TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_NoFile_User) {
  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(kDummyAppGuid,
                                               kDummyAppVersion,
                                               kDummyAppLang,
                                               false,
                                               DownloadFileNoFile,
                                               NULL));

  EXPECT_EQ(static_cast<void*>(NULL), saved_context_);
  EXPECT_FALSE(File::Exists(saved_file_path_));

  TCHAR temp_dir[MAX_PATH] = {0};
  EXPECT_TRUE(::GetEnvironmentVariable(_T("TMP"), temp_dir, MAX_PATH));
  EXPECT_TRUE(File::Exists(temp_dir))
      << _T("The temp directory was deleted or not created.");
}

TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,
       FixGoogleUpdate_AllValues_MachineApp) {
  const TCHAR kExpectedUrl[] = _T("http://cr-tools.clients.google.com/service/check2?appid=%7B8E472B0D-3E8B-43b1-B89A-E8506AAF1F16%7D&appversion=3.4.5.6&applang=en-us&machine=1&version=5.6.78.1&machineid=%7B12A7B304-B4ED-4b5c-8122-31E7ECB8BE3C%7D&userid=%7B29388E49-2995-437c-BF47-587C7E9247E5%7D&osversion=");  // NOLINT

  const CString prev_tmp = GetTmp();
  ASSERT_TRUE(::SetEnvironmentVariable(_T("TMP"), kTempDirectory));

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullMachineOmahaClientKeyPath,
                                            _T("pv"),
                                            _T("5.6.78.1")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullMachineOmahaMainKeyPath,
                                            _T("mi"),
                                            kDummyMachineId));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullMachineOmahaMainKeyPath,
                                            _T("ui"),
                                            kDummyUserId));

  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(kDummyAppGuid,
                                               kDummyAppVersion,
                                               kDummyAppLang,
                                               true,
                                               DownloadFileNoFile,
                                               NULL));

  EXPECT_STREQ(kExpectedUrl, saved_url_.Left(arraysize(kExpectedUrl) - 1));
  CheckSavedUrlOSFragment();
  VerifyExpectedSavedFilePath(kTempDirectory);

  EXPECT_TRUE(::SetEnvironmentVariable(_T("TMP"), prev_tmp));
}

TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,
       FixGoogleUpdate_AllValues_UserApp) {
  const TCHAR kExpectedUrl[] = _T("http://cr-tools.clients.google.com/service/check2?appid=%7B8E472B0D-3E8B-43b1-B89A-E8506AAF1F16%7D&appversion=3.4.5.6&applang=en-us&machine=0&version=5.6.78.1&machineid=%7B12A7B304-B4ED-4b5c-8122-31E7ECB8BE3C%7D&userid=%7B29388E49-2995-437c-BF47-587C7E9247E5%7D&osversion=");  // NOLINT

  const CString prev_tmp = GetTmp();
  ASSERT_TRUE(::SetEnvironmentVariable(_T("TMP"), kTempDirectory));

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullUserOmahaClientKeyPath,
                                            _T("pv"),
                                            _T("5.6.78.1")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullMachineOmahaMainKeyPath,
                                            _T("mi"),
                                            kDummyMachineId));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullUserOmahaMainKeyPath,
                                            _T("ui"),
                                            kDummyUserId));

  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(kDummyAppGuid,
                                               kDummyAppVersion,
                                               kDummyAppLang,
                                               false,
                                               DownloadFileNoFile,
                                               NULL));

  EXPECT_STREQ(kExpectedUrl, saved_url_.Left(arraysize(kExpectedUrl) - 1));
  CheckSavedUrlOSFragment();
  VerifyExpectedSavedFilePath(kTempDirectory);

  EXPECT_TRUE(::SetEnvironmentVariable(_T("TMP"), prev_tmp));
}

TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,
       FixGoogleUpdate_NoOmahaRegKeys) {
  const TCHAR kExpectedUrl[] = _T("http://cr-tools.clients.google.com/service/check2?appid=%7B8E472B0D-3E8B-43b1-B89A-E8506AAF1F16%7D&appversion=3.4.5.6&applang=en-us&machine=1&version=0.0.0.0&machineid=&userid=&osversion=");  // NOLINT

  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(kDummyAppGuid,
                                               kDummyAppVersion,
                                               kDummyAppLang,
                                               true,
                                               DownloadFileNoFile,
                                               NULL));
  EXPECT_STREQ(kExpectedUrl, saved_url_.Left(arraysize(kExpectedUrl) - 1));
  CheckSavedUrlOSFragment();
}

TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,
       FixGoogleUpdate_EmptyAppInfo) {
  const TCHAR kExpectedUrl[] = _T("http://cr-tools.clients.google.com/service/check2?appid=&appversion=&applang=&machine=1&version=0.0.0.0&machineid=&userid=&osversion=");  // NOLINT

  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(_T(""),
                                               _T(""),
                                               _T(""),
                                               true,
                                               DownloadFileNoFile,
                                               NULL));
  EXPECT_STREQ(kExpectedUrl, saved_url_.Left(arraysize(kExpectedUrl) - 1));
  CheckSavedUrlOSFragment();
}

TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,
       FixGoogleUpdate_NullArgs) {
  EXPECT_EQ(E_INVALIDARG, FixGoogleUpdate(NULL,
                                          _T(""),
                                          _T(""),
                                          true,
                                          DownloadFileNoFile,
                                          NULL));
  EXPECT_EQ(E_INVALIDARG, FixGoogleUpdate(_T(""),
                                          NULL,
                                          _T(""),
                                          true,
                                          DownloadFileNoFile,
                                          NULL));
  EXPECT_EQ(E_INVALIDARG, FixGoogleUpdate(_T(""),
                                          _T(""),
                                          NULL,
                                          true,
                                          DownloadFileNoFile,
                                          NULL));
  EXPECT_EQ(E_INVALIDARG, FixGoogleUpdate(_T(""),
                                          _T(""),
                                          _T(""),
                                          true,
                                          NULL,
                                          NULL));
}

// Verifies that the file is saved even if the temp directory doesn't exist.
TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_SaveToNonExistantDirectory) {
  const TCHAR kNonExistantDirectory[] = _T("c:\\directory_does_not_exist");
  DeleteDirectory(kNonExistantDirectory);
  ASSERT_FALSE(File::Exists(kNonExistantDirectory));

  const CString prev_tmp = GetTmp();
  ASSERT_TRUE(::SetEnvironmentVariable(_T("TMP"), kNonExistantDirectory));

  EXPECT_EQ(TRUST_E_SUBJECT_FORM_UNKNOWN,
            FixGoogleUpdate(kDummyAppGuid,
                            kDummyAppVersion,
                            kDummyAppLang,
                            true,
                            DownloadFileInvalidFile,
                            NULL));

  VerifyExpectedSavedFilePath(kNonExistantDirectory);

  EXPECT_TRUE(::SetEnvironmentVariable(_T("TMP"), prev_tmp));
}

TEST_F(GoogleUpdateRecoveryTest, FixGoogleUpdate_FileCollision) {
  const CString prev_tmp = GetTmp();
  ASSERT_TRUE(::SetEnvironmentVariable(_T("TMP"), kTempDirectory));

  CString saved_arguments_path = ConcatenatePath(app_util::GetTempDir(),
                                                 kSavedArgumentsFileName);

  EXPECT_HRESULT_SUCCEEDED(FixGoogleUpdate(kDummyAppGuid,
                                           kDummyAppVersion,
                                           kDummyAppLang,
                                           false,
                                           DownloadArgumentSavingFile,
                                           NULL));

  EXPECT_TRUE(File::Exists(saved_file_path_));
  VerifyExpectedSavedFilePath(kTempDirectory);

  CString first_saved_file_path = saved_file_path_;

  // Ensure that the first downloaded file is in use.
  FileLock lock;
  EXPECT_HRESULT_SUCCEEDED(lock.Lock(first_saved_file_path));

  EXPECT_HRESULT_SUCCEEDED(FixGoogleUpdate(kDummyAppGuid,
                                           kDummyAppVersion,
                                           kDummyAppLang,
                                           false,
                                           DownloadArgumentSavingFile,
                                           NULL));
  EXPECT_TRUE(File::Exists(saved_file_path_));
  VerifyExpectedSavedFilePath(kTempDirectory);

  EXPECT_STRNE(first_saved_file_path, saved_file_path_);

  EXPECT_HRESULT_SUCCEEDED(lock.Unlock());

  bool is_deleted = false;
  for (int tries = 0; tries < 100 && !is_deleted; ++tries) {
    ::Sleep(50);
    is_deleted = !!::DeleteFile(saved_file_path_);
  }
  EXPECT_TRUE(is_deleted);

  EXPECT_TRUE(::DeleteFile(first_saved_file_path));
  EXPECT_TRUE(::DeleteFile(saved_arguments_path));

  EXPECT_TRUE(::SetEnvironmentVariable(_T("TMP"), prev_tmp));
}

//
// VerifyFileSignature Tests
//
TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_SignedValid) {
  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kArgumentSavingExecutableRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_HRESULT_SUCCEEDED(VerifyFileSignature(executable_full_path));
}

TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_NotSigned) {
  const TCHAR kUnsignedExecutable[] = _T("GoogleUpdate_unsigned.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kUnsignedExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_EQ(TRUST_E_NOSIGNATURE, VerifyFileSignature(executable_full_path));
}

// The certificate is still valid, but the executable was signed more than N
// days ago.
TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_SignedOldWithValidCert) {
  const TCHAR kUnsignedExecutable[] =
      _T("unittest_support\\GoogleUpdate_old_signature.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kUnsignedExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_EQ(TRUST_E_TIME_STAMP, VerifyFileSignature(executable_full_path));
}

// The certificate was valid when it was used to sign the executable, but it has
// since expired.
// The error returned is inappropriate for this case because of the way
// expiration checking is implemented.
TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_SignedWithNowExpiredCert) {
  const TCHAR kUnsignedExecutable[] =
      _T("unittest_support\\GoogleUpdate_now_expired_cert.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kUnsignedExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_EQ(CERT_E_CN_NO_MATCH, VerifyFileSignature(executable_full_path));
}

TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_UntrustedChain) {
  const TCHAR kUntrustedChainExecutable[] =
      _T("unittest_support\\SaveArguments_OmahaTestSigned.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kUntrustedChainExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_EQ(CERT_E_UNTRUSTEDROOT, VerifyFileSignature(executable_full_path));
}

TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_HashFails) {
  const TCHAR kCorruptedExecutable[] =
      _T("unittest_support\\GoogleUpdate_corrupted.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kCorruptedExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_EQ(TRUST_E_BAD_DIGEST, VerifyFileSignature(executable_full_path));
}

TEST_F(GoogleUpdateRecoveryTest,
       VerifyFileSignature_NonGoogleSignature) {
  CString file_path = SystemInfo::IsRunningOnVistaOrLater() ?
      _T("%SYSTEM%\\rcagent.exe") : _T("%SYSTEM%\\wuauclt.exe");
  ASSERT_HRESULT_SUCCEEDED(ExpandStringWithSpecialFolders(&file_path));
  ASSERT_TRUE(File::Exists(file_path));
  ASSERT_TRUE(SignatureIsValid(file_path, false));
  EXPECT_EQ(CERT_E_CN_NO_MATCH, VerifyFileSignature(file_path));
}

TEST_F(GoogleUpdateRecoveryTest, VerifyFileSignature_BadFilenames) {
  EXPECT_EQ(CRYPT_E_FILE_ERROR, VerifyFileSignature(_T("NoSuchFile.exe")));

  EXPECT_EQ(CRYPT_E_FILE_ERROR, VerifyFileSignature(NULL));

  EXPECT_EQ(CRYPT_E_FILE_ERROR, VerifyFileSignature(_T("")));
}

//
// VerifyRepairFileMarkup Tests
//
TEST_F(GoogleUpdateRecoveryTest, VerifyRepairFileMarkup_ValidMarkup) {
  const TCHAR kExecutableWithMarkup[] =
      _T("unittest_support\\SaveArguments.exe");
  EXPECT_HRESULT_SUCCEEDED(VerifyRepairFileMarkup(kExecutableWithMarkup));
}

TEST_F(GoogleUpdateRecoveryTest, VerifyRepairFileMarkup_InvalidMarkups) {
  const TCHAR kNoResourcesExecutable[] = _T("RecoveryTest.exe");
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_RESOURCE_DATA_NOT_FOUND),
            VerifyRepairFileMarkup(kNoResourcesExecutable));

  const TCHAR kResourcesButNoMarkupExecutable[] = _T("GoogleUpdate.exe");
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_RESOURCE_TYPE_NOT_FOUND),
            VerifyRepairFileMarkup(kResourcesButNoMarkupExecutable));

  const TCHAR kWrongMarkupResourceNameExecutable[] =
      _T("unittest_support\\SaveArguments_unsigned_wrong_resource_name.exe");
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND),
            VerifyRepairFileMarkup(kWrongMarkupResourceNameExecutable));

  const TCHAR kWrongMarkupSizeExecutable[] =
      _T("unittest_support\\SaveArguments_unsigned_wrong_markup_size.exe");
  EXPECT_EQ(E_UNEXPECTED, VerifyRepairFileMarkup(kWrongMarkupSizeExecutable));

  const TCHAR kWrongMarkupValueExecutable[] =
      _T("unittest_support\\SaveArguments_unsigned_wrong_markup_value.exe");
  EXPECT_EQ(E_UNEXPECTED, VerifyRepairFileMarkup(kWrongMarkupValueExecutable));
}

TEST_F(GoogleUpdateRecoveryTest, VerifyRepairFileMarkup_BadFilenames) {
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            VerifyRepairFileMarkup(_T("NoSuchFile.exe")));

  EXPECT_EQ(E_INVALIDARG, VerifyRepairFileMarkup(NULL));

  EXPECT_EQ(E_INVALIDARG, VerifyRepairFileMarkup(_T("")));
}

//
// VerifyRepairFileMarkup Tests
//
// TODO(omaha): Unit test VerifyIsValidRepairFile.

//
// Production Server Response Tests Tests
//
TEST_F(GoogleUpdateRecoveryTest, ProductionServerResponseTest) {
  EXPECT_EQ(kDummyNoFileError, FixGoogleUpdate(kDummyAppGuid,
                                               kDummyAppVersion,
                                               kDummyAppLang,
                                               true,
                                               DownloadFileFromServer,
                                               NULL)) <<
      _T("The production server did not return 204. This may indicate network ")
      _T("issues or that the Code Red server is configured incorrectly");
}

}  // namespace omaha

