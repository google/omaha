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
// Download manager supports downloading one file at a time.
// The DownloadFile is a blocking call. All errors are reported through the
// return value of this method. Progress is reported on the
// NetworkRequestCallback callback that the job object implements.

#ifndef OMAHA_WORKER_DOWNLOAD_MANAGER_H__
#define OMAHA_WORKER_DOWNLOAD_MANAGER_H__

#include <windows.h>
#include <atlstr.h>
#include "omaha/net/http_client.h"
#include "base/scoped_ptr.h"
#include "omaha/worker/job.h"

namespace omaha {

class Job;
class NetworkRequest;

class DownloadManager {
 public:
  explicit DownloadManager(bool is_machine);
  HRESULT DownloadFile(Job* job);
  HRESULT Cancel();
  CompletionInfo error_info() const { return error_info_; }

  HANDLE impersonation_token() const { return impersonation_token_; }
  void set_impersonation_token(HANDLE token) { impersonation_token_ = token; }
 private:
  HRESULT GetFileNameFromDownloadUrl(const CString& url,
                                     CString* file_name) const;
  HRESULT BuildDestinationDirectory(CString* dest_path) const;
  HRESULT BuildUniqueDownloadFilePath(CString* file) const;
  HRESULT DownloadCurrentJob();
  bool IsCached(const CString& store);
  HRESULT ValidateDownloadedFile(const CString& file_name) const;
  HRESULT MoveFile();
  void SetErrorInfo(HRESULT hr);
  void LogValidationFailure() const;

  CString local_download_file_path_;
  CompletionInfo error_info_;
  Job* job_;
  bool is_machine_;
  scoped_ptr<NetworkRequest> network_request_;
  HANDLE impersonation_token_;    // Token to impersonate with, if available.

  // True if the code runs in the current interactive session. BITS only allows
  // jobs to run when the owner is logged in. Local System is always logged on.
  bool is_logged_on_;
  scoped_ptr<HttpClient> http_client_;

  friend class DownloadManagerTest;
  DISALLOW_EVIL_CONSTRUCTORS(DownloadManager);
};

}  // namespace omaha

#endif  // OMAHA_WORKER_DOWNLOAD_MANAGER_H__
