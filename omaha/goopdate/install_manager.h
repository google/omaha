// Copyright 2009-2010 Google Inc.
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

// Defines a command object to be executed in the thread pool when the Install
// method is called.

#ifndef OMAHA_GOOPDATE_INSTALL_MANAGER_H_
#define OMAHA_GOOPDATE_INSTALL_MANAGER_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/thread_pool.h"

namespace omaha {

class App;
class AppVersion;
class InstallerWrapper;
struct InstallerResultInfo;
struct Lockable;

// Public interface for the InstallManager.
class InstallManagerInterface {
 public:
  virtual ~InstallManagerInterface() {}
  virtual CString install_working_dir() const = 0;
  virtual HRESULT Initialize() = 0;
  virtual void InstallApp(App* app, const CString& dir) = 0;
};

class InstallManager : public InstallManagerInterface {
 public:
  InstallManager(const Lockable* model_lock, bool is_machine_);
  virtual ~InstallManager();

  // Returns the base directory where the InstallManager expects application
  // packages to be available before the install.
  virtual CString install_working_dir() const;

  virtual HRESULT Initialize();

  // Installs an application. Expects the application packages to be present
  // in the specified directory.
  virtual void InstallApp(App* app, const CString& dir);

 private:
  // TODO(omaha): Rename to avoid overload.
  static HRESULT InstallApp(bool is_machine,
                            HANDLE user_token,
                            const CString& existing_version,
                            const Lockable& model_lock,
                            InstallerWrapper* installer_wrapper,
                            App* app,
                            const CString& dir);
  static void PopulateSuccessfulInstallResultInfo(
      const App* app,
      InstallerResultInfo* result_info);

  const Lockable* model_lock_;
  const bool is_machine_;

  // Base path where verified application packages are copied before install.
  CString install_working_dir_;

  scoped_ptr<InstallerWrapper> installer_wrapper_;

  friend class InstallManagerInstallAppTest;

  DISALLOW_COPY_AND_ASSIGN(InstallManager);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_INSTALL_MANAGER_H_
