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

#include <windows.h>

#include "omaha/goopdate/goopdate_helper.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/resource.h"
#include "omaha/setup/setup.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/job_observer.h"
#include "omaha/worker/ping_utils.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

// job_observer can be NULL.
HRESULT FinishGoogleUpdateInstall(const CommandLineArgs& args,
                                  bool is_machine,
                                  bool is_self_update,
                                  Ping* ping,
                                  JobObserver* job_observer) {
  CORE_LOG(L2, (_T("[FinishGoogleUpdateInstall]")));
  ASSERT1(args.mode == COMMANDLINE_MODE_IG ||
          args.mode == COMMANDLINE_MODE_UG);
  ASSERT1(ping);

  // Get the previous version before updating it.
  AppManager app_manager(is_machine);
  ProductData product_data;
  HRESULT hr = app_manager.ReadProductDataFromStore(kGoopdateGuid,
                                                    &product_data);
  if (FAILED(hr)) {
    CORE_LOG(L2, (_T("[ReadProductDataFromStore failed][0x%08x]"), hr));
  }
  const CString& previous_version = product_data.app_data().previous_version();

  // Send a ping for the "start" of Setup. This is actually the start of setup
  // phase 2. We can't ping at the true beginning of Setup because we can't
  // ping from the temp directory.
  HRESULT hr_ping = ping_utils::SendGoopdatePing(
      is_machine,
      args.extra,
      is_self_update ? PingEvent::EVENT_SETUP_UPDATE_BEGIN :
                       PingEvent::EVENT_SETUP_INSTALL_BEGIN,
      S_OK,
      0,
      NULL,
      ping);
  if (FAILED(hr_ping)) {
    CORE_LOG(LW, (_T("[SendSetupPing(started) failed][0x%08x]"), hr_ping));
  }

  Setup setup(is_machine, &args);
  hr = setup.SetupGoogleUpdate();
  // Do not return until the ping is sent.

  if (SUCCEEDED(hr)) {
    CORE_LOG(L1, (_T("[Setup successfully completed]")));
    app_manager.ClearUpdateAvailableStats(GUID_NULL, kGoopdateGuid);

    if (is_self_update) {
      ++metric_worker_self_updates_succeeded;
    }
  } else {
    CORE_LOG(LE, (_T("[Setup::SetupGoogleUpdate failed][0x%08x]"), hr));

    if (job_observer) {
      ASSERT1(!is_self_update);
      CString message;
      message.FormatMessage(IDS_SETUP_FAILED, hr);
      job_observer->OnComplete(COMPLETION_CODE_ERROR, message, hr);
    }
  }

  // Send a ping to report Setup has completed.
  hr_ping = ping_utils::SendPostSetupPing(hr,
                                          setup.extra_code1(),
                                          previous_version,
                                          is_machine,
                                          is_self_update,
                                          args.extra,
                                          ping);
  if (FAILED(hr_ping)) {
    CORE_LOG(LW, (_T("[SendSetupPing(completed) failed][0x%08x]"), hr_ping));
  }

  return hr;
}

}  // namespace omaha

