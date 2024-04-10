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

// Functions for installing applications.

#ifndef OMAHA_CLIENT_INSTALL_APPS_H_
#define OMAHA_CLIENT_INSTALL_APPS_H_

#include <windows.h>
#include <atlstr.h>
#include "omaha/client/install_progress_observer.h"

namespace omaha {

struct CommandLineExtraArgs;

// TODO(omaha): Similar definitions with just DoClose() appear elsewhere.
// Is there a common name we can give these?
class OnDemandEventsInterface {
 public:
  virtual ~OnDemandEventsInterface() {}
  virtual void DoClose() = 0;
  virtual void DoExit() = 0;
};

class OnDemandObserver : public InstallProgressObserver {
 public:
  virtual void SetEventSink(OnDemandEventsInterface* event_sink) = 0;
};

// TODO(omaha3): Should these be in a class or namespace?

HRESULT UpdateAppOnDemand(bool is_machine,
                          const CString& app_id,
                          bool is_update_check_only,
                          const CString& session_id,
                          HANDLE impersonation_token,
                          HANDLE primary_token,
                          OnDemandObserver* observer);

HRESULT InstallApps(bool is_machine,
                    bool is_interactive,
                    bool always_launch_cmd,
                    bool is_eula_accepted,
                    bool is_oem_install,
                    bool is_offline,
                    bool is_enterprise_install,
                    const CString& offline_dir,
                    const CommandLineExtraArgs& extra_args,
                    const CString& install_source,
                    const CString& session_id,
                    bool* has_ui_been_displayed);

HRESULT InstallForceInstallApps(bool is_machine,
                                bool is_interactive,
                                const CString& install_source,
                                const CString& display_language,
                                const CString& session_id,
                                bool* has_ui_been_displayed);

HRESULT UpdateAllApps(bool is_machine,
                      bool is_interactive,
                      const CString& install_source,
                      const CString& display_language,
                      const CString& session_id,
                      bool* has_ui_been_displayed);

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_APPS_H_
