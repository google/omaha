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
//
// job_creator.h:  Class that handles creating jobs out of ProductData that's
// being requested to be installed/updated and the UpdateResponse objects
// received from the server.  Builds up proper job hierarchy, ping data, error
// handling, and event logging since this overall operation is pretty complex.

#ifndef OMAHA_WORKER_JOB_CREATOR_H__
#define OMAHA_WORKER_JOB_CREATOR_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/update_response.h"
#include "omaha/worker/job.h"
#include "omaha/worker/product_data.h"

namespace omaha {

class AppData;

class JobCreator {
 public:
  // The caller retains ownership of ping. Delete this object and the Jobs it
  // creates before deleting the Ping.
  JobCreator(bool is_machine, bool is_update, Ping* ping)
      : ping_(ping),
        is_machine_(is_machine),
        is_update_(is_update),
        is_auto_update_(false),
        is_update_check_only_(false),
        fail_if_update_not_available_(false) {}
  ~JobCreator() {}
  HRESULT CreateJobsFromResponses(const UpdateResponses& responses,
                                  const ProductDataVector& products,
                                  Jobs* jobs,
                                  Request* ping_request,
                                  CString* event_log_text,
                                  CompletionInfo* completion_info);

  // Creates jobs based on offline installers that are stored in {guid}
  // subdirectories under offline_dir. offline_dir is typically
  // Google\Update\Offline\.
  HRESULT CreateOfflineJobs(const CString& offline_dir,
                            const ProductDataVector& products,
                            Jobs* jobs,
                            Request* ping_request,
                            CString* event_log_text,
                            CompletionInfo* completion_info);

  void set_is_auto_update(bool is_auto_update) {
    is_auto_update_ = is_auto_update;
  }
  void set_is_update_check_only(bool is_update_check_only) {
    is_update_check_only_ = is_update_check_only;
  }
  void set_fail_if_update_not_available(bool fail_if_update_not_available) {
    fail_if_update_not_available_ = fail_if_update_not_available;
  }

 private:
  // Read offline manifest offline_dir\{guid}.gup.
  static HRESULT ReadOfflineManifest(const CString& offline_dir,
                                     const CString& app_guid,
                                     UpdateResponse* response);

  // Finds offline installer stored under offline_dir\{guid}.
  static HRESULT FindOfflineFilePath(const CString& offline_dir,
                                     const CString& app_guid,
                                     CString* file_path);

  HRESULT CreateJobsFromResponsesInternal(
      const UpdateResponses& responses,
      const ProductDataVector& products,
      Jobs* jobs,
      Request* ping_request,
      CString* event_log_text,
      CompletionInfo* completion_info);

  HRESULT HandleProductUpdateIsAvailable(
      const ProductData& product_data,
      const UpdateResponse& response,
      Job** product_job,
      Request* ping_request,
      CString* event_log_text,
      CompletionInfo* completion_info);

  HRESULT CreateGoopdateJob(const UpdateResponses& responses,
                            const ProductDataVector& products,
                            Jobs* jobs,
                            Request* ping_request,
                            CString* event_log_text,
                            CompletionInfo* completion_info);

  bool IsGoopdateUpdateAvailable(const UpdateResponses& responses);

  void HandleResponseNotFound(const AppData& app_data,
                              AppRequestData* ping_app_request_data,
                              CString* event_log_text);
  void HandleUpdateIsAvailable(const AppData& app_data,
                               const UpdateResponseData& response_data,
                               AppRequestData* ping_app_request_data,
                               CString* event_log_text,
                               Job** job);
  HRESULT HandleUpdateNotAvailable(const AppData& app_data,
                                   const UpdateResponseData& response_data,
                                   AppRequestData* ping_app_request_data,
                                   CString* event_log_text,
                                   CompletionInfo* completion_info);
  CompletionInfo UpdateResponseDataToCompletionInfo(
      const UpdateResponseData& response_data,
      const CString& display_name);
  bool IsUpdateAvailable(const UpdateResponseData& response_data);
  bool IsUpdateAvailable(const UpdateResponse& response);
  void AddAppUpdateDeferredPing(const AppData& product_data,
                                Request* ping_request);

  Ping* ping_;
  bool is_machine_;
  bool is_update_;
  // True, if the jobs is intended to do an update check only, for instance, in
  // an on demand check for updates case.
  bool is_auto_update_;
  bool is_update_check_only_;
  bool fail_if_update_not_available_;

  friend class JobCreatorTest;
  DISALLOW_EVIL_CONSTRUCTORS(JobCreator);
};

}  // namespace omaha.

#endif  // OMAHA_WORKER_JOB_CREATOR_H__


