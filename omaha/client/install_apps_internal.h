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


#ifndef OMAHA_CLIENT_INSTALL_APPS_INTERNAL_H_
#define OMAHA_CLIENT_INSTALL_APPS_INTERNAL_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "omaha/base/browser_utils.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/ui/progress_wnd.h"

namespace omaha {

class AppBundle;
class BundleInstaller;
struct CommandLineExtraArgs;

namespace internal {

// TODO(omaha): Figure out how to handle pause requests.
class InstallAppsWndEvents : public ProgressWndEvents {
 public:
  InstallAppsWndEvents(bool is_machine,
                       BundleInstaller* installer,
                       BrowserType browser_type);

  virtual void DoClose();
  virtual void DoExit();
  virtual void DoCancel();
  virtual bool DoLaunchBrowser(const CString& url);

  // When a valid browser type is specified in the command line, that type of
  // browser will be restarted.
  virtual bool DoRestartBrowser(bool restart_all_browsers,
                                const std::vector<CString>& urls);
  virtual bool DoReboot();

 private:
  bool is_machine_;
  BundleInstaller* installer_;
  BrowserType browser_type_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InstallAppsWndEvents);
};

// On success, the caller takes ownership of observer and ui_sink.
HRESULT CreateClientUI(bool is_machine,
                       BrowserType browser_type,
                       BundleInstaller* installer,
                       AppBundle* app_bundle,
                       InstallProgressObserver** observer,
                       OmahaWndEvents** ui_sink);

// Does the work for InstallApps, allowing the COM server to be mocked.
HRESULT DoInstallApps(BundleInstaller* installer,
                      IAppBundle* app_bundle,
                      bool is_machine,
                      bool is_interactive,
                      BrowserType browser_type,
                      bool* has_ui_been_displayed);

// Displays an error message and reports the error as appropriate.
void HandleInstallAppsError(HRESULT error,
                            int extra_code1,
                            bool is_machine,
                            bool is_interactive,
                            bool is_eula_required,
                            bool is_oem_install,
                            const CommandLineExtraArgs& extra_args,
                            bool* has_ui_been_displayed);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_APPS_INTERNAL_H_
