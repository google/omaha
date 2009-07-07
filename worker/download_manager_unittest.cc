// Copyright 2008-2009 Google Inc.
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


#include <windows.h>
#include <atlstr.h>
#include "omaha/common/app_util.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/scoped_ptr_cotask.h"
#include "omaha/common/system.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/download_manager.h"
#include "omaha/worker/job.h"
#include "omaha/worker/ping.h"

namespace omaha {

namespace {

// Hash = Igq6bYaeXFJCjH770knXyJ6V53s=, size = 479848
const TCHAR kTestExeHash[] = _T("Igq6bYaeXFJCjH770knXyJ6V53s=");
const int kTestExeSize = 479848;
const TCHAR kWrongTestExeHash[] = _T("F006bYaeXFJCjH770knXyJ6V53s=");
// Hash = ImV9skETZqGFMjs32vbZTvzAYJU=, size = 870400
const TCHAR kTestMsiHash[] = _T("ImV9skETZqGFMjs32vbZTvzAYJU=");
const int kTestMsiSize = 870400;

}  // namespace

class DownloadManagerTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    TCHAR module_path[MAX_PATH] = {0};
    ASSERT_NE(0, ::GetModuleFileName(NULL, module_path, MAX_PATH));
    CString path = GetDirectoryFromPath(module_path);

    cache_test_dir_ =
        ConcatenatePath(path, _T("unittest_support\\download_cache_test"));

    CString expected_path_exe = (_T("{7101D597-3481-4971-AD23-455542964072}")
                                 _T("\\livelysetup.exe"));
    expected_file_name_exe_ =
        ConcatenatePath(cache_test_dir_, expected_path_exe);

    CString expected_path_msi = (_T("{89640431-FE64-4da8-9860-1A1085A60E13}")
                                 _T("\\gears-win32-opt.msi"));
    expected_file_name_msi_ =
        ConcatenatePath(cache_test_dir_, expected_path_msi);
  }

  virtual void SetUp() {
    job_.reset(CreateJob());
  }

  void Initialize(bool is_machine) {
    download_manager_.reset(new DownloadManager(is_machine));
  }

  virtual void TearDown() {
    download_manager_.reset();
    job_.reset();
  }

  HRESULT BuildDestinationDirectory(CString* dir) {
    return download_manager_->BuildDestinationDirectory(dir);
  }

  HRESULT BuildUniqueDownloadFilePath(CString* file) {
    return download_manager_->BuildUniqueDownloadFilePath(file);
  }

  void SetJob(Job* job) {
    download_manager_->job_ = job;
  }

  bool IsCached(const CString& path) {
    return download_manager_->IsCached(path);
  }

  HRESULT GetFileNameFromDownloadUrl(const CString& url,
                                     CString* out) {
    return download_manager_->GetFileNameFromDownloadUrl(url, out);
  }

  HRESULT ValidateDownloadedFile(const CString& file_name) const {
    return download_manager_->ValidateDownloadedFile(file_name);
  }

  void SetErrorInfo(HRESULT hr) {
    download_manager_->SetErrorInfo(hr);
  }

  Job* CreateJob() {
    Initialize(false);
    UpdateResponseData response_data;
    response_data.set_url(_T("http://dl.google.com/update2/UpdateData.bin"));
    response_data.set_hash(_T("YF2z/br/S6E3KTca0MT7qziJN44="));
    scoped_ptr<Job> job(new Job(true, &ping_));
    job->set_is_background(true);
    job->set_update_response_data(response_data);
    return job.release();
  }

  Job* CreateJob(const UpdateResponseData& data) {
    scoped_ptr<Job> job(new Job(true, &ping_));
    job->set_is_background(true);
    job->set_update_response_data(data);
    return job.release();
  }

  void SetLocalDownloadFilepath(const CString download_file) {
    download_manager_->local_download_file_path_ = download_file;
  }

  void VerifyCompletionInfo(JobCompletionStatus status,
                            DWORD error_code,
                            const CString& text) {
    EXPECT_EQ(status, download_manager_->error_info().status);
    EXPECT_EQ(error_code, download_manager_->error_info().error_code);
    EXPECT_STREQ(text, download_manager_->error_info().text);
  }

  scoped_ptr<Job> job_;
  scoped_ptr<DownloadManager> download_manager_;
  Ping ping_;

  static CString cache_test_dir_;
  static CString expected_file_name_exe_;
  static CString expected_file_name_msi_;
};

CString DownloadManagerTest::cache_test_dir_;
CString DownloadManagerTest::expected_file_name_exe_;
CString DownloadManagerTest::expected_file_name_msi_;

// Download a file via an http: URL.
TEST_F(DownloadManagerTest, DownloadViaHttp) {
  Initialize(false);
  scoped_ptr<Job> job1(CreateJob());
  EXPECT_HRESULT_SUCCEEDED(job1->Download(download_manager_.get()));
  EXPECT_TRUE(::DeleteFile(job1->download_file_name()));

  scoped_ptr<Job> job2(CreateJob());
  EXPECT_HRESULT_SUCCEEDED(job2->Download(download_manager_.get()));
  EXPECT_TRUE(::DeleteFile(job2->download_file_name()));
}

TEST_F(DownloadManagerTest, BuildDestinationDirectory_User) {
  Initialize(false);
  CString path = ConfigManager::Instance()->GetUserDownloadStorageDir();

  CString dir;
  EXPECT_HRESULT_SUCCEEDED(BuildDestinationDirectory(&dir));

  CString common_prefix;
  int common_path_len = ::PathCommonPrefix(dir,
                                           path,
                                           CStrBuf(common_prefix, MAX_PATH));
  EXPECT_EQ(path.GetLength(), common_path_len);
  EXPECT_STREQ(common_prefix, path);

  CString dir2;
  EXPECT_HRESULT_SUCCEEDED(BuildDestinationDirectory(&dir2));
  CString common_prefix2;
  common_path_len = ::PathCommonPrefix(dir2,
                                       path,
                                       CStrBuf(common_prefix2, MAX_PATH));
  EXPECT_EQ(path.GetLength(), common_path_len);
  EXPECT_STREQ(common_prefix2, path);

  EXPECT_STRNE(dir, dir2);
}

TEST_F(DownloadManagerTest, BuildUniqueDownloadFilePath_User) {
  Initialize(false);

  CString file;
  EXPECT_HRESULT_SUCCEEDED(BuildUniqueDownloadFilePath(&file));
  CString str_guid = GetFileFromPath(file);
  EXPECT_TRUE(!str_guid.IsEmpty());
  GUID guid = StringToGuid(str_guid);
  EXPECT_TRUE(!::IsEqualGUID(guid, GUID_NULL));
}

TEST_F(DownloadManagerTest, BuildDestinationDirectory_Machine) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  Initialize(true);
  CString path =
      ConfigManager::Instance()->GetMachineSecureDownloadStorageDir();

  CString dir;
  EXPECT_HRESULT_SUCCEEDED(BuildDestinationDirectory(&dir));

  CString common_prefix;
  int common_path_len = ::PathCommonPrefix(dir,
                                           path,
                                           CStrBuf(common_prefix, MAX_PATH));
  EXPECT_EQ(path.GetLength(), common_path_len);
  EXPECT_STREQ(common_prefix, path);

  CString dir2;
  EXPECT_HRESULT_SUCCEEDED(BuildDestinationDirectory(&dir2));
  CString common_prefix2;
  common_path_len = ::PathCommonPrefix(dir2,
                                       path,
                                       CStrBuf(common_prefix2, MAX_PATH));
  EXPECT_EQ(path.GetLength(), common_path_len);
  EXPECT_STREQ(common_prefix2, path);

  EXPECT_STRNE(dir, dir2);
}

TEST_F(DownloadManagerTest, BuildUniqueDownloadFilePath_Machine) {
  if (!vista_util::IsUserAdmin()) {
    return;
  }

  Initialize(true);
  CString file;
  EXPECT_HRESULT_SUCCEEDED(BuildUniqueDownloadFilePath(&file));
  CString str_guid = GetFileFromPath(file);
  EXPECT_TRUE(!str_guid.IsEmpty());
  GUID guid = StringToGuid(str_guid);
  EXPECT_TRUE(!::IsEqualGUID(guid, GUID_NULL));
}

TEST_F(DownloadManagerTest, IsCached_TestExe) {
  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/livelysetup.exe"));
  response_data.set_hash(kTestExeHash);
  response_data.set_size(kTestExeSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_TRUE(IsCached(cache_test_dir_));
  EXPECT_STREQ(expected_file_name_exe_, job->download_file_name());
}

TEST_F(DownloadManagerTest, IsCached_TestMSI) {
  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/gears-win32-opt.msi"));
  response_data.set_hash(kTestMsiHash);
  response_data.set_size(kTestMsiSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_TRUE(IsCached(cache_test_dir_));
  EXPECT_STREQ(expected_file_name_msi_, job->download_file_name());
}

TEST_F(DownloadManagerTest, IsCached_FileNotPresent) {
  UpdateResponseData response_data;
  response_data.set_url(_T("dl.google.com/not_present.msi"));
  response_data.set_hash(kTestMsiHash);
  response_data.set_size(kTestMsiSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_FALSE(IsCached(cache_test_dir_));
  EXPECT_TRUE(job->download_file_name().IsEmpty());
}

TEST_F(DownloadManagerTest, IsCached_InvalidHash) {
  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/gears-win32-opt.msi"));
  response_data.set_hash(_T("BAADBAADBAADMjs32vbZTvzAYJU="));
  response_data.set_size(kTestMsiSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_FALSE(IsCached(cache_test_dir_));
  EXPECT_TRUE(job->download_file_name().IsEmpty());
}

TEST_F(DownloadManagerTest, IsCached_NotUpdateJob) {
  scoped_ptr<Job> job(new Job(false, &ping_));
  UpdateResponseData response_data;
  response_data.set_url(_T("dl.google.com/livelysetup.exe"));
  response_data.set_hash(kTestExeHash);
  response_data.set_size(kTestExeSize);
  job->set_update_response_data(response_data);
  SetJob(job.get());

  EXPECT_FALSE(IsCached(cache_test_dir_));
  EXPECT_TRUE(job->download_file_name().IsEmpty());
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_Valid) {
  UpdateResponseData response_data;
  response_data.set_url(_T("dl.google.com/livelysetup.exe"));
  response_data.set_hash(kTestExeHash);
  response_data.set_size(kTestExeSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());
  SetLocalDownloadFilepath(expected_file_name_exe_);

  EXPECT_SUCCEEDED(ValidateDownloadedFile(expected_file_name_exe_));
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_HashFailsCorrectSize) {
  UpdateResponseData response_data;
  response_data.set_url(_T("dl.google.com/livelysetup.exe"));
  response_data.set_hash(kWrongTestExeHash);
  response_data.set_size(kTestExeSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());
  SetLocalDownloadFilepath(expected_file_name_exe_);

  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE,
            ValidateDownloadedFile(expected_file_name_exe_));
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_ValidHashSizeIncorrect) {
  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/livelysetup.exe"));
  response_data.set_hash(kTestExeHash);
  response_data.set_size(kTestExeSize + 10);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_SUCCEEDED(ValidateDownloadedFile(expected_file_name_exe_));
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_HashFailsSizeZero) {
  const TCHAR kEmtpyFileName[] = _T("emptyfile.txt");

  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/livelysetup.exe"));
  response_data.set_hash(kWrongTestExeHash);
  response_data.set_size(kTestExeSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_SUCCEEDED(File::Remove(kEmtpyFileName));
  File empty_file;
  EXPECT_SUCCEEDED(empty_file.Open(kEmtpyFileName, true, false));
  EXPECT_SUCCEEDED(empty_file.Close());

  EXPECT_EQ(GOOPDATEDOWNLOAD_E_FILE_SIZE_ZERO,
            ValidateDownloadedFile(kEmtpyFileName));

  EXPECT_SUCCEEDED(File::Remove(kEmtpyFileName));
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_HashFailsSizeSmaller) {
  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/livelysetup.exe"));
  response_data.set_hash(kWrongTestExeHash);
  response_data.set_size(kTestExeSize + 1);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_EQ(GOOPDATEDOWNLOAD_E_FILE_SIZE_SMALLER,
            ValidateDownloadedFile(expected_file_name_exe_));
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_HashFailsSizeLarger) {
  UpdateResponseData response_data;
  response_data.set_url(_T("dl.google.com/livelysetup.exe"));
  response_data.set_hash(kWrongTestExeHash);
  response_data.set_size(kTestExeSize - 1);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  EXPECT_EQ(GOOPDATEDOWNLOAD_E_FILE_SIZE_LARGER,
            ValidateDownloadedFile(expected_file_name_exe_));
}

TEST_F(DownloadManagerTest, ValidateDownloadedFile_FileDoesNotExist) {
  UpdateResponseData response_data;
  response_data.set_url(_T("http://dl.google.com/livelysetup.exe"));
  response_data.set_hash(kWrongTestExeHash);
  response_data.set_size(kTestExeSize);
  scoped_ptr<Job> job(CreateJob(response_data));
  SetJob(job.get());

  ExpectAsserts expect_asserts;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            ValidateDownloadedFile(_T("nosuchfile.txt")));
}

TEST_F(DownloadManagerTest, GetFileNameFromDownloadUrl_QueryParams) {
  SetJob(CreateJob());

  CString url = _T("http://foo10.bar.google.com:26190/update2/test/machinefoo/test.msi?tttoken=1.DQAAA3_cykoeVgF7b8ZT1Ycsv_tfjbAofLplQp3xHrwOOJGZkb1ZakYVTIN0QMJvA5IlMzULa0AEP-JWYZVVKz-gQTD35u30pRAirXjKjsEC5KHKQKqBipa00ni8krbzYawfTKQKAAmlTan_eLHDOnH7NiHcrCLkLSE_5Un1S7p5_DegLgFGfUXffwHS6S-Z5LHCHdqUXCW&test=10&hello"); // NOLINT
  CString file_name;
  ASSERT_HRESULT_SUCCEEDED(GetFileNameFromDownloadUrl(url, &file_name));
  ASSERT_STREQ(_T("test.msi"), file_name);
}

TEST_F(DownloadManagerTest, GetFileNameFromDownloadUrl_NoQueryParams) {
  SetJob(CreateJob());

  CString url = _T("http://foo10.google.com:26190/test/machinefoo/test.msi");
  CString file_name;
  ASSERT_HRESULT_SUCCEEDED(GetFileNameFromDownloadUrl(url, &file_name));
  ASSERT_STREQ(_T("test.msi"), file_name);

  url = _T("http://dl.google.com/update2/1.2.121.9/GoogleUpdateSetup.exe");
  ASSERT_HRESULT_SUCCEEDED(GetFileNameFromDownloadUrl(url, &file_name));
  ASSERT_STREQ(_T("GoogleUpdateSetup.exe"), file_name);

  url = _T("http://dl.google.com/foo/plugin/4.3.9543.7852/foo-plugin-win.exe");
  ASSERT_HRESULT_SUCCEEDED(GetFileNameFromDownloadUrl(url, &file_name));
  ASSERT_STREQ(_T("foo-plugin-win.exe"), file_name);
}

TEST_F(DownloadManagerTest, GetFileNameFromDownloadUrl_WithOutPath) {
  SetJob(CreateJob());

  CString url = _T("http://foo10.bar.google.com:26190/test.exe");
  CString file_name;
  ASSERT_HRESULT_SUCCEEDED(GetFileNameFromDownloadUrl(url, &file_name));
  ASSERT_STREQ(_T("test.exe"), file_name);
}

TEST_F(DownloadManagerTest, GetFileNameFromDownloadUrl_InvalidPaths) {
  SetJob(CreateJob());

  CString url = _T("http://foo10.bar.google.com:26190/test/");
  CString file_name;
  EXPECT_HRESULT_FAILED(GetFileNameFromDownloadUrl(url, &file_name));

  url = _T("http://foo10.bar.google.com:26190/test/?");
  EXPECT_EQ(GOOPDATEDOWNLOAD_E_FILE_NAME_EMPTY,
            GetFileNameFromDownloadUrl(url, &file_name));

  url = _T("foo10.bar.google.com:26190test");
  EXPECT_HRESULT_FAILED(GetFileNameFromDownloadUrl(url, &file_name));
}


TEST_F(DownloadManagerTest, SetErrorInfo) {
  AppData app_data;
  app_data.set_display_name(_T("Test App"));
  Job job(false, &ping_);
  job.set_app_data(app_data);
  SetJob(&job);

  SetErrorInfo(GOOPDATE_E_NO_NETWORK);
  VerifyCompletionInfo(
      COMPLETION_ERROR,
      static_cast<DWORD>(GOOPDATE_E_NO_NETWORK),
      _T("Installation failed. Ensure that your computer is connected to the ")
      _T("Internet and that your firewall allows GoogleUpdate.exe to connect ")
      _T("and then try again. Error code = 0x80040801."));

  SetErrorInfo(GOOPDATE_E_NETWORK_UNAUTHORIZED);
  VerifyCompletionInfo(
      COMPLETION_ERROR,
      static_cast<DWORD>(GOOPDATE_E_NETWORK_UNAUTHORIZED),
      _T("The Test App installer could not connect to the Internet because of ")
      _T("an HTTP 401 Unauthorized response. This is likely a proxy ")
      _T("configuration issue.  Please configure the proxy server to allow ")
      _T("network access and try again or contact your network administrator. ")
      _T("Error code = 0x80042191"));

  SetErrorInfo(GOOPDATE_E_NETWORK_FORBIDDEN);
  VerifyCompletionInfo(
      COMPLETION_ERROR,
      static_cast<DWORD>(GOOPDATE_E_NETWORK_FORBIDDEN),
      _T("The Test App installer could not connect to the Internet because of ")
      _T("an HTTP 403 Forbidden response. This is likely a proxy ")
      _T("configuration issue.  Please configure the proxy server to allow ")
      _T("network access and try again or contact your network administrator. ")
      _T("Error code = 0x80042193"));

  SetErrorInfo(GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED);
  VerifyCompletionInfo(
      COMPLETION_ERROR,
      static_cast<DWORD>(GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED),
      _T("The Test App installer could not connect to the Internet because a ")
      _T("proxy server required user authentication. Please configure the ")
      _T("proxy server to allow network access and try again or contact your ")
      _T("network administrator. Error code = 0x80042197"));

  SetErrorInfo(E_FAIL);
  VerifyCompletionInfo(
      COMPLETION_ERROR,
      static_cast<DWORD>(E_FAIL),
      _T("Installer download failed. Error code = 0x80004005"));
}

}  // namespace omaha

