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


#ifndef OMAHA_WORKER_WORKER_JOB_STRATEGY_H__
#define OMAHA_WORKER_WORKER_JOB_STRATEGY_H__

#include <windows.h>
#include <atlstr.h>
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/worker/com_wrapper_shutdown_handler.h"
#include "omaha/worker/product_data.h"

namespace omaha {

class JobObserver;
class JobObserverCOMDecorator;
class NetworkStrategy;
class Ping;
class WorkerJob;
struct CommandLineArgs;

class WorkerJobStrategy {
 public:
  WorkerJobStrategy(bool is_machine, const CString& lang);
  virtual ~WorkerJobStrategy() {}

  void set_worker_job(WorkerJob* worker_job) {
    ASSERT1(worker_job);
    worker_job_ = worker_job;
  }

  virtual HRESULT PreUpdateCheck(ProductDataVector* products) = 0;
  virtual HRESULT RemoveDisallowedApps(ProductDataVector* products) = 0;
  virtual HRESULT DoUpdateCheck(const ProductDataVector& products);
  virtual HRESULT PostUpdateCheck() = 0;
  virtual HRESULT ProcessApps();
  virtual HRESULT PostInstall();

  virtual bool ShouldFailOnUpdateNotAvailable() const { return false; }
  virtual bool IsAutoUpdate() const { return false; }
  virtual bool IsUpdate() const { return false; }
  virtual bool IsUpdateCheckOnly() const { return false; }
  virtual bool ShouldLaunchBrowserOnUpdateCheckError() const { return false; }

  const CString language() const { return language_; }
  bool is_machine() const { return is_machine_; }
  CString first_disallowed_app_name() const {
      return first_disallowed_app_name_;
  }

 protected:
  WorkerJob* worker_job() const { return worker_job_; }
  NetworkStrategy* network_strategy() const { return network_strategy_.get(); }
  virtual void set_network_strategy(NetworkStrategy* network_strategy) {
    network_strategy_.reset(network_strategy);
  }
  CString first_disallowed_app_name_;

 private:
  bool is_machine_;
  CString language_;
  WorkerJob* worker_job_;
  scoped_ptr<NetworkStrategy> network_strategy_;

  friend class WorkerJobStrategyFactory;
  friend class NetworkStrategy;

  DISALLOW_EVIL_CONSTRUCTORS(WorkerJobStrategy);
};

class UpdateAppsStrategy : public WorkerJobStrategy {
 public:
  virtual HRESULT PreUpdateCheck(ProductDataVector* products);
  virtual HRESULT RemoveDisallowedApps(ProductDataVector* products_);
  virtual HRESULT DoUpdateCheck(const ProductDataVector& products);
  virtual HRESULT PostUpdateCheck();

  virtual bool IsAutoUpdate() const { return true; }
  virtual bool IsUpdate() const { return true; }

 private:
  explicit UpdateAppsStrategy(bool is_machine, const CommandLineArgs& args)
      : WorkerJobStrategy(is_machine, args.extra.language),
        args_(args) {
  }
  virtual HRESULT PingUninstalledProducts() const;

  // Sends an "install by OEM" ping for any of the products that were installed
  // by an OEM. If the ping is sent successfully, the OEM install flag is
  // deleted so the ping will not be sent again.
  virtual HRESULT SendInstalledByOemPing(
      const ProductDataVector& products) const;

  const CommandLineArgs& args_;

  friend class WorkerJobStrategyFactory;
};

class InstallAppsStrategy : public WorkerJobStrategy {
 public:
  virtual HRESULT PreUpdateCheck(ProductDataVector* products);
  virtual HRESULT RemoveDisallowedApps(ProductDataVector* products_);
  virtual HRESULT PostUpdateCheck();
  virtual HRESULT PostInstall();

  virtual bool ShouldFailOnUpdateNotAvailable() const { return true; }
  virtual bool ShouldLaunchBrowserOnUpdateCheckError() const;

 protected:
  InstallAppsStrategy(bool is_machine,
                      const CommandLineArgs& args);
  const CommandLineArgs& args_;

  friend class WorkerJobStrategyFactory;
};

class InstallGoopdateAndAppsStrategy : public InstallAppsStrategy {
 public:
  virtual HRESULT PreUpdateCheck(ProductDataVector* products);

 private:
  InstallGoopdateAndAppsStrategy(bool is_machine,
                                 const CommandLineArgs& args,
                                 JobObserver* job_observer)
      : InstallAppsStrategy(is_machine, args),
        job_observer_(job_observer) {}
  JobObserver* job_observer_;

  friend class WorkerJobStrategyFactory;
};

class OnDemandUpdateStrategy : public WorkerJobStrategy {
 public:
  HRESULT Init(GUID guid,
               IJobObserver* observer,
               WorkerComWrapperShutdownCallBack* call_back);
  virtual JobObserver* GetJobObserver() const;
  virtual HRESULT PreUpdateCheck(ProductDataVector* products);
  virtual HRESULT RemoveDisallowedApps(ProductDataVector* products_);
  virtual HRESULT PostUpdateCheck();
  virtual HRESULT ProcessApps();

  virtual bool ShouldFailOnUpdateNotAvailable() const { return true; }
  virtual bool IsUpdate() const { return true; }
  virtual bool IsUpdateCheckOnly() const { return is_update_check_only_; }

 private:
  OnDemandUpdateStrategy(bool is_update_check_only,
                         const CString& lang,
                         bool is_machine)
      : WorkerJobStrategy(is_machine, lang),
        is_update_check_only_(is_update_check_only),
        job_observer_com_(NULL) {}
  HRESULT InitializeDecorator(IJobObserver* observer,
                              WorkerComWrapperShutdownCallBack* call_back);

  bool is_update_check_only_;
  CComObject<JobObserverCOMDecorator>* job_observer_com_;
  CComPtr<IUnknown> job_observer_com_scoped_holder_;
  ProductDataVector products_;

  friend class WorkerJobStrategyFactory;
};

class WorkerJobStrategyFactory {
 public:
  static WorkerJobStrategy* CreateInstallStrategy(bool is_machine,
                                                  const CommandLineArgs& args,
                                                  JobObserver* job_observer);
  static WorkerJobStrategy* CreateUpdateAppsStrategy(
      bool is_machine, const CommandLineArgs& args);
  static OnDemandUpdateStrategy* CreateOnDemandStrategy(bool update_check_only,
                                                        const CString& lang,
                                                        bool is_machine);
};

class NetworkStrategy {
 public:
  virtual ~NetworkStrategy() {}
  virtual HRESULT DoUpdateCheck(const ProductDataVector& products) = 0;
  virtual HRESULT DownloadJobs() = 0;
 protected:
  explicit NetworkStrategy(WorkerJobStrategy* worker_job_strategy)
      : worker_job_strategy_(worker_job_strategy) {}
  WorkerJob* worker_job() {
    return worker_job_strategy_->worker_job();
  }

  WorkerJobStrategy* worker_job_strategy_;
};

}  // namespace omaha

#endif  // OMAHA_WORKER_WORKER_JOB_STRATEGY_H__
