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

#ifndef OMAHA_WORKER_WORKER_JOB_H__
#define OMAHA_WORKER_WORKER_JOB_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/scoped_any.h"
#include "omaha/goopdate/update_response.h"
#include "omaha/worker/job.h"
#include "omaha/worker/job_observer.h"

namespace omaha {

class AppData;
class DownloadManager;
class Ping;
class Request;
class WorkerJobStrategy;

class WorkerJob : public JobObserver,
                  public ProgressWndEvents {
 public:
  explicit WorkerJob(WorkerJobStrategy* strategy, JobObserver* job_observer);
  ~WorkerJob();
  HRESULT DoProcess();
  HRESULT Cancel();

  Ping* ping() { return ping_.get(); }
  bool is_machine() const { return is_machine_; }
  const Jobs& jobs() const { return jobs_; }
  WorkerJobStrategy* strategy() const { return strategy_.get(); }

  // Observer interface.
  virtual void OnShow();
  virtual void OnCheckingForUpdate();
  virtual void OnUpdateAvailable(const TCHAR* version_string);
  virtual void OnWaitingToDownload();
  virtual void OnDownloading(int time_remaining_ms, int pos);
  virtual void OnWaitingToInstall();
  virtual void OnInstalling();
  virtual void OnPause();
  virtual void OnComplete(CompletionCodes code,
                          const TCHAR* text,
                          DWORD error_code);
  virtual void SetEventSink(ProgressWndEvents* event_sink);
  virtual void Uninitialize() {}

  // ProgressWndEvents implementation.
  virtual void DoPause();
  virtual void DoResume();
  virtual void DoClose();
  virtual void DoRestartBrowsers();
  virtual void DoReboot();
  virtual void DoLaunchBrowser(const CString& url);


  // TODO(omaha): Consider making these methods private.
  // The reason we need this bloated interface is because the strategies
  // call into the workerjob that is treated as the context for the calls.
  // One way to achieve this is to take in the workerjob in these methods
  // and make them static non member methods.
  HRESULT DownloadJobs();
  HRESULT InstallJobs();
  void NotifyCompleted(JobCompletionStatus status,
                       DWORD error,
                       const CString& text,
                       const ProductDataVector& products);
  void CompleteAllNonCompletedJobs(JobCompletionStatus status,
                                   DWORD error,
                                   const CString& text);
  HRESULT DoUpdateCheck(const ProductDataVector& products);
  HRESULT CreateOfflineJobs(const ProductDataVector& products);

  HRESULT error_code() const { return error_code_; }

  bool is_canceled() const { return !!is_canceled_; }

 private:
  void HandleSuccessfulUpdateCheckRequestSend(
      const UpdateResponses& responses, const ProductDataVector& products);

  HRESULT CreateJobs(bool is_offline,
                     const UpdateResponses& responses,
                     const ProductDataVector& products);
  HRESULT DoProcessInternal(ProductDataVector* products);

  void UpdateResponseToCompletionInfo(const UpdateResponse& response,
                                      CompletionInfo* info);

  static bool IsUpdateAvailable(const UpdateResponse& response);
  HRESULT CreateRequestFromApplications(const AppDataVector& applications,
                                        Request** request);
  void CreateRequestFromProducts(const ProductDataVector& products,
                                 Request** request,
                                 bool* encrypt_connection);

  // Sets the UI displayed event, telling the other instance not to display an
  // error UI.
  void SetUiDisplayedEvent() const;

  // Calculates the size of the download bundle.
  int CalculateBundleSize() const;

  Job* cur_job_;
  int bundle_dl_size_;
  int bundle_bytes_downloaded_;

  bool is_machine_;
  bool is_update_apps_worker_;
  std::vector<Job*> jobs_;
  scoped_ptr<NetworkRequest> network_request_;
  scoped_ptr<NetworkRequest> network_request_encrypted_;
  scoped_ptr<DownloadManager> download_manager_;
  CString running_version_;
  volatile LONG is_canceled_;
  bool no_jobs_completed_ping_sent_;
  scoped_ptr<Ping> ping_;
  JobObserver* job_observer_;
  scoped_ptr<WorkerJobStrategy> strategy_;
  HRESULT error_code_;
  friend class WorkerJobTest;

  DISALLOW_EVIL_CONSTRUCTORS(WorkerJob);
};

class WorkerJobFactory {
 public:
  static WorkerJob* CreateWorkerJob(bool is_machine,
                                    const CommandLineArgs& args,
                                    JobObserver* job_observer);

  static HRESULT CreateOnDemandWorkerJob(
      bool is_machine,
      bool is_update_check_only,
      const CString& lang,
      const GUID& guid,
      IJobObserver* observer,
      WorkerComWrapperShutdownCallBack* call_back,
      WorkerJob** worker_job);
};

// Creates a request containing the pings for the uninstalled products.
// The caller has the ownership of the request object. Building the
// uninstall ping is not an idempotent operation. The client state is
// cleaned up after building the request.
HRESULT BuildUninstallPing(bool is_machine, Request** uninstall_ping);

}  // namespace omaha

#endif  // OMAHA_WORKER_WORKER_JOB_H__
