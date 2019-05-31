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

#ifndef OMAHA_GOOPDATE_WORKER_H_
#define OMAHA_GOOPDATE_WORKER_H_

#include <windows.h>
#include <atlstr.h>
#include <memory>

#include "base/basictypes.h"
#include "omaha/base/program_instance.h"
#include "omaha/base/shutdown_callback.h"
#include "omaha/base/shutdown_handler.h"
#include "omaha/base/wtl_atlapp_wrapper.h"

namespace omaha {

namespace xml {

class UpdateRequest;
class UpdateResponse;

}  // namespace xml

class AppBundle;
class DownloadManagerInterface;
class InstallManagerInterface;
class Model;
class Package;
class Reactor;

// Limited subset of Worker interface that the Model needs.
class WorkerModelInterface {
 public:
  virtual ~WorkerModelInterface() {}
  virtual HRESULT CheckForUpdateAsync(AppBundle* app_bundle) = 0;
  virtual HRESULT DownloadAsync(AppBundle* app_bundle) = 0;
  virtual HRESULT DownloadAndInstallAsync(AppBundle* app_bundle) = 0;
  virtual HRESULT UpdateAllAppsAsync(AppBundle* app_bundle) = 0;
  virtual HRESULT DownloadPackageAsync(Package* package) = 0;
  virtual HRESULT Stop(AppBundle* app_bundle) = 0;
  virtual HRESULT Pause(AppBundle* app_bundle) = 0;
  virtual HRESULT Resume(AppBundle* app_bundle) = 0;
  virtual HRESULT GetPackage(const Package* package, const CString& dir) = 0;
  virtual bool IsPackageAvailable(const Package* package) const = 0;
  virtual HRESULT PurgeAppLowerVersions(const CString& app_id,
                                        const CString& version) = 0;
  virtual int Lock() = 0;
  virtual int Unlock() = 0;
};

// Worker is a singleton.
class Worker : public WorkerModelInterface, public ShutdownCallback {
 public:
  // Instance, Initialize, and DeleteInstance methods below are not thread safe.
  // The caller must initialize and cleanup the instance before going
  // multithreaded.

  // Gets the singleton instance of the class.
  static Worker& Instance();

  // Initializes the instance.
  HRESULT Initialize(bool is_machine);

  HRESULT EnsureSingleInstance();

  // Cleans up the class instance.
  static void DeleteInstance();

  HRESULT Run();

  HRESULT Shutdown();
  HRESULT InitializeShutDownHandler();

  // TODO(omaha): not clear how to make this an atomic operation. Consider
  // making the model instance a bare pointer instead of smart pointer.
  Model* model() { return model_.get(); }
  const Model* model() const { return model_.get(); }

  // Initiates an update check for all apps in the bundle.
  virtual HRESULT CheckForUpdateAsync(AppBundle* app_bundle);

  // Initiates download of files necessary to install all apps in the bundle.
  virtual HRESULT DownloadAsync(AppBundle* app_bundle);

  // Initiates Download, if necessary, and install all app in the bundle.
  virtual HRESULT DownloadAndInstallAsync(AppBundle* app_bundle);

  // Initiates an update of all registered apps and performs periodic tasks
  // related to all apps. Primarily for use by Omaha's /ua client. Includes
  // update check, download and install.
  virtual HRESULT UpdateAllAppsAsync(AppBundle* app_bundle);

  // Initiates download of a package.
  virtual HRESULT DownloadPackageAsync(Package* package);

  virtual HRESULT Stop(AppBundle* app_bundle);
  virtual HRESULT Pause(AppBundle* app_bundle);
  virtual HRESULT Resume(AppBundle* app_bundle);

  virtual HRESULT GetPackage(const Package*, const CString& dir);

  virtual bool IsPackageAvailable(const Package* package) const;

  virtual HRESULT PurgeAppLowerVersions(const CString& app_id,
                                        const CString& version);

  // Locks and unlocks the server module by incrementing or decrementing
  // the lock count of the module.
  virtual int Lock();
  virtual int Unlock();

 private:
  Worker();
  ~Worker();

  HRESULT DoRun();

  // These functions execute code in the thread pool. They hold an outstanding
  // reference to the application bundle to prevent the application bundle
  // object from being deleted before the functions complete.
  void CheckForUpdate(std::shared_ptr<AppBundle> app_bundle);
  void Download(std::shared_ptr<AppBundle> app_bundle);
  void DownloadAndInstall(std::shared_ptr<AppBundle> app_bundle);
  void DownloadPackage(std::shared_ptr<AppBundle> app_bundle, Package* package);
  void UpdateAllApps(std::shared_ptr<AppBundle> app_bundle);

  // These functions do the work for the corresponding functions but do not call
  // CompleteAsyncCall().
  void CheckForUpdateHelper(AppBundle* app_bundle, bool* is_check_successful);
  void DownloadAndInstallHelper(AppBundle* app_bundle);

  // Stops and destroys the Worker and its members.
  // TODO(omaha): rename this as it overloads WorkerModelInterface::Stop.
  void Stop();

  void CollectAmbientUsageStats();

  void DoPreUpdateCheck(AppBundle* app_bundle,
                        xml::UpdateRequest* update_request);

  HRESULT CacheOfflinePackages(AppBundle* app_bundle);

  HRESULT DoUpdateCheck(AppBundle* app_bundle,
                        const xml::UpdateRequest* update_request,
                        xml::UpdateResponse* update_response);
  void DoPostUpdateCheck(AppBundle* app_bundle,
                         HRESULT update_check_result,
                         xml::UpdateResponse* update_response);

  void PersistRetryAfter(int retry_after_sec) const;

  HRESULT QueueDeferredFunctionCall0(
      std::shared_ptr<AppBundle> app_bundle,
      void (Worker::*deferred_function)(std::shared_ptr<AppBundle>));

  template <typename P1>
  HRESULT QueueDeferredFunctionCall1(
      std::shared_ptr<AppBundle> app_bundle,
      P1 p1,
      void (Worker::*deferred_function)(std::shared_ptr<AppBundle>, P1));

  void WriteEventLog(int event_type,
                     int event_id,
                     const CString& event_description,
                     const CString& event_text);

  bool is_machine_;
  int lock_count_;
  HRESULT single_instance_hr_;
  std::unique_ptr<ProgramInstance> single_instance_;
  std::unique_ptr<Reactor>         reactor_;
  std::unique_ptr<ShutdownHandler> shutdown_handler_;
  std::unique_ptr<Model>           model_;
  std::unique_ptr<DownloadManagerInterface> download_manager_;
  std::unique_ptr<InstallManagerInterface> install_manager_;

  CMessageLoop message_loop_;

  static Worker* const kInvalidInstance;
  static Worker* instance_;

  friend class WorkerTest;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

// For unittests, where creation and termination of Worker instances may be
// required, the implementation disables the dead reference detection. This
// forces an inline of the code below for unit tests, so different behavior
// can be achieved, even though the rest of the implementation compiles in
// a library. It is somehow brittle but good enough for now.

#ifdef UNITTEST
__forceinline
#else
  inline
#endif
void Worker::DeleteInstance() {
  delete instance_;
#ifdef UNITTEST
  instance_ = NULL;
#else
  instance_ = kInvalidInstance;
#endif
}

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_WORKER_H_
