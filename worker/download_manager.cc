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
// The download manager uses the network request to download the remote file.
// When running as local system, the network request impersonates one of the
// logged on users. To save the file, the network request needs write access to
// a directory, both when running impersonated and not.
// The directory is obtained by calling SHGetFolderLocation with
// CSIDL_COMMON_APPDATA. In order to ensure the directory is accessible
// even in cases when impersonatation is used, BuildUniqueDownloadFilePath
// impersonates before calling SHGetFolderLocation.
//
// Once the download is complete, The download manager copies the file to either
// the machine secure location or the user secure location and then
// it validates the hash.

#include "omaha/worker/download_manager.h"

#include <vector>
#include <algorithm>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/scoped_impersonation.h"
#include "omaha/common/string.h"
#include "omaha/common/user_rights.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/browser_request.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_request.h"
#include "omaha/net/net_utils.h"
#include "omaha/net/simple_request.h"
#include "omaha/worker/job.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

namespace {

// Creates and initializes an instance of the NetworkRequest for the
// Downloadmanager to use. Defines a fallback chain: BITS, WinHttp, browser.
NetworkRequest* CreateNetworkRequest(bool is_logged_on) {
  const NetworkConfig::Session& session(NetworkConfig::Instance().session());
  NetworkRequest* network_request(new NetworkRequest(session));

  // TODO(omaha): provide a mechanism for different timeout values in
  // silent and interactive downloads.

  // TODO(omaha): background downloads are not supported yet.

  // BITS transfers files only when the job owner is logged on. If the
  // process "Run As" another user, an empty BITS job gets created in suspended
  // state but there is no way to manipulate the job, nor cancel it.
  if (is_logged_on) {
    BitsRequest* bits_request(new BitsRequest);
    bits_request->set_minimum_retry_delay(60);
    bits_request->set_no_progress_timeout(15);
    network_request->AddHttpRequest(bits_request);
  }

  network_request->AddHttpRequest(new SimpleRequest);
  network_request->AddHttpRequest(new BrowserRequest);

  network_request->set_num_retries(1);
  return network_request;
}

}  // namespace

DownloadManager::DownloadManager(bool is_machine)
    : job_(NULL),
      is_machine_(is_machine),
      impersonation_token_(NULL),
      is_logged_on_(false) {
  HRESULT hr = IsUserLoggedOn(&is_logged_on_);

  // Assumes the caller is not logged on if the function failed.
  ASSERT1(SUCCEEDED(hr) || !is_logged_on_);

  http_client_.reset(CreateHttpClient());
  ASSERT1(http_client_.get());
}

void DownloadManager::SetErrorInfo(HRESULT hr) {
  ASSERT1(job_);
  CString msg;
  // TODO(omaha):  job_->app_data().display_name() may not be correct
  // for bundles.
  if (!goopdate_utils::FormatMessageForNetworkError(
          hr,
          job_->app_data().display_name(),
          &msg)) {
    msg.FormatMessage(IDS_DOWNLOAD_ERROR, hr);
  }
  error_info_ = CompletionInfo(COMPLETION_ERROR, hr, msg);
}

HRESULT DownloadManager::DownloadFile(Job* job) {
  ASSERT1(job);
  ASSERT1(job_ == NULL);

  ASSERT1(ConfigManager::Instance()->CanUseNetwork(is_machine_));

  ++metric_worker_download_total;

  job_ = job;
  HRESULT hr = DownloadCurrentJob();
  if (SUCCEEDED(hr)) {
    ++metric_worker_download_succeeded;
  }
  job_ = NULL;
  return hr;
}

HRESULT DownloadManager::DownloadCurrentJob() {
  ASSERT1(job_);

  HRESULT hr = BuildUniqueDownloadFilePath(&local_download_file_path_);
  if (FAILED(hr)) {
    CORE_LOG(LW,
        (_T("[BuildUniqueDownloadFilePath failed][0x%08x]"), hr));
    SetErrorInfo(hr);
    return hr;
  }

  network_request_.reset(CreateNetworkRequest(is_logged_on_));
  network_request_->set_low_priority(job_->is_background());

  CString path = is_machine_ ?
      ConfigManager::Instance()->GetMachineSecureDownloadStorageDir() :
      ConfigManager::Instance()->GetUserDownloadStorageDir();

  if (IsCached(path)) {
    OPT_LOG(L1, (_T("[Using cached version of the download file %s]"),
                 local_download_file_path_));
    return S_OK;
  }

  network_request_->set_callback(job_);
  OPT_LOG(L1, (_T("[Starting file download from %s to %s]"),
               job_->response_data().url(),
               local_download_file_path_));
  hr = network_request_->DownloadFile(job_->response_data().url(),
                                      local_download_file_path_);
  if (FAILED(hr)) {
    goopdate_utils::AddNetworkRequestDataToEventLog(network_request_.get(), hr);
    SetErrorInfo(hr);
  }
  VERIFY1(SUCCEEDED(network_request_->Close()));
  if (FAILED(hr)) {
    return hr;
  }

  hr = MoveFile();
  if (FAILED(hr)) {
    SetErrorInfo(hr);
    return hr;
  }

  hr = ValidateDownloadedFile(local_download_file_path_);
  if (FAILED(hr)) {
    CString msg;
    msg.FormatMessage(IDS_DOWNLOAD_HASH_MISMATCH, hr);
    error_info_ = CompletionInfo(COMPLETION_ERROR, hr, msg);
    LogValidationFailure();
    return hr;
  }

  return S_OK;
}

void DownloadManager::LogValidationFailure() const {
  const int kDownloadFileBytesToLog = 256;

  bool exists = File::Exists(local_download_file_path_);
  uint32 file_size(0);
  std::vector<char> download_file_bytes(kDownloadFileBytesToLog + 1);
  if (exists) {
    if (FAILED(File::GetFileSizeUnopen(local_download_file_path_,
                                       &file_size))) {
      return;
    }

    File downloaded_file;
    if (SUCCEEDED(downloaded_file.Open(local_download_file_path_,
                                       false, false))) {
      uint32 bytes_read = 0;
      if (SUCCEEDED(downloaded_file.ReadFromStartOfFile(
              kDownloadFileBytesToLog,
              reinterpret_cast<unsigned char*>(&download_file_bytes.front()),
              &bytes_read))) {
        download_file_bytes.resize(bytes_read);
        std::replace_if(download_file_bytes.begin(), download_file_bytes.end(),
                        std::not1(std::ptr_fun(isprint)), '.');
        download_file_bytes.push_back('\0');
      }
    }
  }

  REPORT_LOG(L1, (_T("[DownloadValidationFail filename=%s exists=%d size=%d ")
                  _T("expected size=%d expected hash=%s filebytes=%hS]"),
                  local_download_file_path_,
                  exists,
                  file_size,
                  job_->response_data().size(),
                  job_->response_data().hash(),
                  &download_file_bytes.front()));
}

HRESULT DownloadManager::Cancel() {
  return network_request_.get() ? network_request_->Cancel() : S_OK;
}

// The installer is initially downloaded to a temporary unique name.
// Once the download succeeds the file is copied to
// DownloadDir\<Guid>\<name> where
// DownloadDir = User or machine download dir returned by ConfigManager.
// guid        = Guid used for temp name.
// name        = Name specified in the update response.
// The reason to copy the file using a sub-directory structure is to account
// for the case where the same file is downloaded by multiple processes or
// threads.
HRESULT DownloadManager::BuildUniqueDownloadFilePath(CString* file) const {
  ASSERT1(file);

  // Impersonate the user if a valid impersonation token is presented.
  // Continue unimpersonated if the impersonation fails. We do the
  // impersonation here to get the correct download folder for
  // impersonated clients. (For more information refer to the comment
  // at the top of the file.)
  scoped_impersonation impersonate_user(impersonation_token_);
  if (impersonation_token_) {
    DWORD result = impersonate_user.result();
    ASSERT(result == ERROR_SUCCESS, (_T("impersonation failed %d"), result));
  }

  GUID guid(GUID_NULL);
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[CoCreateGuid failed 0x%08x]"), hr));
    return hr;
  }

  CString path(ConfigManager::Instance()->GetDownloadStorage());
  *file = ConcatenatePath(path, GuidToString(guid));
  if (file->IsEmpty()) {
    ASSERT1(false);
    return GOOPDATEDOWNLOAD_E_UNIQUE_FILE_PATH_EMPTY;
  }
  return S_OK;
}

HRESULT DownloadManager::GetFileNameFromDownloadUrl(const CString& url,
                                                    CString* file_name) const {
  CORE_LOG(L3, (_T("[DownloadManager::GetFileNameFromDownloadUrl]")));
  ASSERT1(job_);
  ASSERT1(http_client_.get());
  ASSERT1(file_name);

  CString url_path;
  int port = 0;
  CString extra_info;
  HRESULT hr = http_client_->CrackUrl(url, 0, NULL, NULL, &port,
                                      &url_path, &extra_info);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[CrackUrl failed 0x%08x]"), hr));
    return GOOPDATEDOWNLOAD_E_CRACKURL_FAILED;
  }

  int start_file_name_idx = url_path.ReverseFind(_T('/'));
  if (start_file_name_idx == -1) {
    CORE_LOG(LW, (_T("[No filename found in download url.]")));
    return GOOPDATEDOWNLOAD_E_INVALID_PATH;
  }
  ASSERT1(url_path.GetLength() >= start_file_name_idx - 1);
  CString dst_file_name =
      url_path.Right(url_path.GetLength() - start_file_name_idx - 1);
  if (dst_file_name.IsEmpty()) {
    OPT_LOG(LE, (_T("[Empty filename in download url]")));
    return GOOPDATEDOWNLOAD_E_FILE_NAME_EMPTY;
  }
  ASSERT1(!dst_file_name.IsEmpty());
  *file_name = dst_file_name;

  return S_OK;
}

bool DownloadManager::IsCached(const CString& store) {
  OPT_LOG(L3, (_T("[DownloadManager::IsCached]")));
  ASSERT1(job_);

  if (!job_->is_background()) {
    return false;
  }

  CString dst_file_name;
  HRESULT hr = GetFileNameFromDownloadUrl(job_->response_data().url(),
                                          &dst_file_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetFileNameFromDownloadUrl failed][0x%08x]"), hr));
    return false;
  }
  ASSERT1(!dst_file_name.IsEmpty());

  std::vector<CString> files;
  hr = FindFileRecursive(store, dst_file_name, &files);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[FindFileRecursive failed][0x%08x]"), hr));
    return false;
  }

  for (size_t i = 0; i < files.size(); ++i) {
    ASSERT1(File::Exists(files[i]));
    if (SUCCEEDED(ValidateDownloadedFile(files[i]))) {
      OPT_LOG(L2, (_T("[Found cached file %s.]"), files[i]));
      local_download_file_path_ = files[i];
      job_->set_download_file_name(local_download_file_path_);
      return true;
    } else {
      OPT_LOG(L2, (_T("[Found cached file %s validation failed.]"), files[i]));
    }
  }

  return false;
}

HRESULT DownloadManager::ValidateDownloadedFile(
    const CString& file_name) const {
  return goopdate_utils::ValidateDownloadedFile(file_name,
      job_->response_data().hash(),
      static_cast<uint32>(job_->response_data().size()));
}

HRESULT DownloadManager::BuildDestinationDirectory(CString* dest_path) const {
  ASSERT1(dest_path);
  dest_path->Empty();

  const CString path = is_machine_ ?
      ConfigManager::Instance()->GetMachineSecureDownloadStorageDir() :
      ConfigManager::Instance()->GetUserDownloadStorageDir();

  CORE_LOG(L3, (_T("[Download Storage Dir][%s]"), path));

  if (!File::Exists(path)) {
    return GOOPDATEDOWNLOAD_E_STORAGE_DIR_NOT_EXIST;
  }

  GUID guid(GUID_NULL);
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[CoCreateGuid failed 0x%08x]"), hr));
    return hr;
  }

  CString destination_path = ConcatenatePath(path, GuidToString(guid));
  if (destination_path.IsEmpty()) {
    ASSERT1(false);
    return GOOPDATEDOWNLOAD_E_DEST_PATH_EMPTY;
  }

  hr = CreateDir(destination_path, NULL);
  if (FAILED(hr)) {
    // Since the directory creation failed, we will fall back to the destination
    // directory returned by the ConfigManager.
    OPT_LOG(LW, (_T("[CreateDir '%s' failed][0x%08x]"), destination_path, hr));
    destination_path = path;
  }

  OPT_LOG(L1, (_T("[The destination directory is '%s']"), destination_path));
  *dest_path = destination_path;

  return S_OK;
}

HRESULT DownloadManager::MoveFile() {
  ++metric_worker_download_move_total;

  CString dest_path;
  HRESULT hr = BuildDestinationDirectory(&dest_path);
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[Build destination directory failed][0x%08x]"), hr));
    return hr;
  }
  CORE_LOG(L3, (_T("[Download Directory][%s]"), dest_path));
  ASSERT1(!dest_path.IsEmpty());
  ASSERT1(File::Exists(dest_path));

  CString dst_file_name;
  hr = GetFileNameFromDownloadUrl(job_->response_data().url(),
                                  &dst_file_name);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[GetFileNameFromDownloadUrl failed][0x%08x]"), hr));
    return hr;
  }

  if (dst_file_name.IsEmpty()) {
    ASSERT1(false);
    return GOOPDATEDOWNLOAD_E_DEST_FILENAME_EMPTY;
  }
  CORE_LOG(L3, (_T("[Destination filename][%s]"), dst_file_name));

  CString dest_file_path = ConcatenatePath(dest_path, dst_file_name);
  if (dest_file_path.IsEmpty()) {
    ASSERT1(false);
    return GOOPDATEDOWNLOAD_E_DEST_FILE_PATH_EMPTY;
  }

  OPT_LOG(L1, (_T("[Moving download file from %s to %s]"),
               local_download_file_path_, dest_file_path));
  // Uses ::CopyFile. ::CopyFile, done without impersonation, will reset the
  // ownership of the destination file, and make sure that it inherits ACEs from
  // the new parent directory.
  hr = File::Copy(local_download_file_path_, dest_file_path, true);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Could not copy '%s' to '%s'][0x%08x]"),
                 local_download_file_path_, dest_file_path, hr));
    job_->set_extra_code1(hr);
    return GOOPDATEDOWNLOAD_E_FAILED_MOVE;
  }
  VERIFY1(SUCCEEDED(File::Remove(local_download_file_path_)));

  local_download_file_path_ = dest_file_path;
  job_->set_download_file_name(dest_file_path);

  ++metric_worker_download_move_succeeded;
  return S_OK;
}

}  // namespace omaha
