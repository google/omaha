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


#include "omaha/goopdate/worker_metrics.h"

namespace omaha {

DEFINE_METRIC_count(worker_download_total);
DEFINE_METRIC_count(worker_download_succeeded);

DEFINE_METRIC_count(worker_download_skipped_bits_machine);

DEFINE_METRIC_count(worker_package_cache_put_total);
DEFINE_METRIC_count(worker_package_cache_put_succeeded);

DEFINE_METRIC_count(worker_install_execute_total);
DEFINE_METRIC_count(worker_install_execute_msi_total);

DEFINE_METRIC_count(worker_install_msi_in_progress_detected_update);
DEFINE_METRIC_count(worker_install_msi_in_progress_retry_succeeded_update);
DEFINE_METRIC_integer(
    worker_install_msi_in_progress_retry_succeeded_tries_update);

DEFINE_METRIC_count(worker_install_msi_in_progress_detected_install);
DEFINE_METRIC_count(worker_install_msi_in_progress_retry_succeeded_install);
DEFINE_METRIC_integer(
    worker_install_msi_in_progress_retry_succeeded_tries_install);

DEFINE_METRIC_integer(worker_shell_version);

DEFINE_METRIC_bool(worker_is_windows_installing);

DEFINE_METRIC_bool(worker_is_uac_disabled);

DEFINE_METRIC_bool(worker_has_software_firewall);

DEFINE_METRIC_count(worker_silent_update_running_on_batteries);

DEFINE_METRIC_count(worker_update_check_total);
DEFINE_METRIC_count(worker_update_check_succeeded);

DEFINE_METRIC_integer(worker_apps_not_updated_eula);
DEFINE_METRIC_integer(worker_apps_not_updated_group_policy);
DEFINE_METRIC_integer(worker_apps_not_installed_group_policy);

DEFINE_METRIC_count(worker_skipped_app_update_for_self_update);

DEFINE_METRIC_count(worker_self_updates_available);
DEFINE_METRIC_count(worker_self_updates_succeeded);

DEFINE_METRIC_count(worker_app_updates_available);
DEFINE_METRIC_count(worker_app_updates_succeeded);

DEFINE_METRIC_integer(worker_self_update_responses);
DEFINE_METRIC_integer(worker_self_update_response_time_since_first_ms);

DEFINE_METRIC_integer(worker_app_max_update_responses_app_high);
DEFINE_METRIC_integer(worker_app_max_update_responses);
DEFINE_METRIC_integer(worker_app_max_update_responses_ms_since_first);

DEFINE_METRIC_timing(ping_failed_ms);
DEFINE_METRIC_timing(ping_succeeded_ms);

DEFINE_METRIC_timing(updatecheck_failed_ms);
DEFINE_METRIC_timing(updatecheck_succeeded_ms);

}  // namespace omaha
