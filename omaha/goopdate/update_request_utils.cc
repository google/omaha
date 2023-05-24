// Copyright 2010 Google Inc.
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

#include "omaha/goopdate/update_request_utils.h"
#include "omaha/base/logging.h"
#include "omaha/common/xml_const.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace update_request_utils {

void BuildRequest(const App* app,
                  bool is_update_check,
                  xml::UpdateRequest* update_request) {
  ASSERT1(app);
  ASSERT1(update_request);

  if (!is_update_check && app->ping_events().empty()) {
    return;
  }

  if (!app->is_eula_accepted() && !app->app_bundle()->is_offline_install()) {
    CORE_LOG(L3, (_T("[App EULA not accepted - not including app in ping][%s]"),
                  app->app_guid_string()));
    return;
  }

  xml::request::App request_app;

  // Pick up the current and next versions.
  request_app.version        = app->current_version()->version();
  request_app.next_version   = app->next_version()->version();

  request_app.app_id         = app->app_guid_string();
  request_app.lang           = app->language();
  request_app.iid            = GuidToString(app->iid());
  request_app.brand_code     = app->brand_code();
  request_app.client_id      = app->client_id();
  request_app.experiments    = app->GetExperimentLabelsNoTimestamps();
  request_app.ap             = app->ap();

  // referral_id is not sent.

  request_app.app_defined_attributes = app->app_defined_attributes();
  request_app.install_time_diff_sec  = app->install_time_diff_sec();
  request_app.day_of_install  = app->day_of_install();

  request_app.cohort      = app->cohort().cohort;
  request_app.cohort_hint = app->cohort().hint;
  request_app.cohort_name = app->cohort().name;

  if (is_update_check) {
    if (!app->server_install_data_index().IsEmpty()) {
      xml::request::Data install_data = { xml::value::kInstallData,
                                          app->server_install_data_index(),
                                          _T("") };
      request_app.data.push_back(install_data);
    }

    if (!app->untrusted_data().IsEmpty()) {
      xml::request::Data untrusted_data = { xml::value::kUntrusted,
                                            _T(""),
                                            app->untrusted_data() };
      request_app.data.push_back(untrusted_data);
    }

    request_app.ping.active = app->did_run();
    request_app.ping.days_since_last_active_ping =
        app->days_since_last_active_ping();
    request_app.ping.days_since_last_roll_call =
        app->days_since_last_roll_call();
    request_app.ping.day_of_last_activity = app->day_of_last_activity();
    request_app.ping.day_of_last_roll_call = app->day_of_last_roll_call();
    request_app.ping.ping_freshness = app->ping_freshness();

    request_app.update_check.is_valid = true;
    request_app.update_check.is_update_disabled =
        FAILED(app->CheckGroupPolicy());
    request_app.update_check.tt_token = app->tt_token();
    request_app.update_check.is_rollback_allowed =
        app->IsRollbackToTargetVersionAllowed();
    request_app.update_check.target_version_prefix =
        app->GetTargetVersionPrefix();
    request_app.update_check.target_channel = app->GetTargetChannel();
  }

  request_app.ping_events = app->ping_events();

  update_request->AddApp(request_app);
}

}  // namespace update_request_utils

}  // namespace omaha
