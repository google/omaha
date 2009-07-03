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


#include "omaha/setup/setup_metrics.h"

namespace omaha {

DEFINE_METRIC_count(setup_install_total);
DEFINE_METRIC_count(setup_install_succeeded);

DEFINE_METRIC_count(setup_update_self_total);
DEFINE_METRIC_count(setup_update_self_succeeded);

DEFINE_METRIC_count(setup_do_self_install_total);
DEFINE_METRIC_count(setup_do_self_install_succeeded);

DEFINE_METRIC_count(setup_handoff_only_total);
DEFINE_METRIC_count(setup_handoff_only_succeeded);

DEFINE_METRIC_timing(setup_install_google_update_total_ms);

DEFINE_METRIC_timing(setup_handoff_ms);
DEFINE_METRIC_timing(setup_handoff_ui_ms);

DEFINE_METRIC_count(setup_installed_core_not_running);

DEFINE_METRIC_count(setup_should_install_total);
DEFINE_METRIC_count(setup_should_install_false_oc);
DEFINE_METRIC_count(setup_should_install_true_fresh_install);
DEFINE_METRIC_count(setup_should_install_false_older);
DEFINE_METRIC_count(setup_should_install_true_newer);
DEFINE_METRIC_count(setup_should_install_false_same);
DEFINE_METRIC_count(setup_should_install_true_same);
DEFINE_METRIC_count(setup_should_install_true_same_completion_missing);
DEFINE_METRIC_count(setup_subsequent_install_total);
DEFINE_METRIC_count(setup_subsequent_install_should_install_true);

DEFINE_METRIC_count(setup_files_total);
DEFINE_METRIC_count(setup_files_verification_succeeded);
DEFINE_METRIC_count(setup_files_verification_failed_pre);
DEFINE_METRIC_count(setup_files_verification_failed_post);

DEFINE_METRIC_timing(setup_files_ms);

DEFINE_METRIC_count(setup_files_replace_shell);

DEFINE_METRIC_timing(setup_phase2_ms);

DEFINE_METRIC_count(setup_install_service_task_total);
DEFINE_METRIC_count(setup_install_service_succeeded);
DEFINE_METRIC_count(setup_install_task_succeeded);
DEFINE_METRIC_count(setup_install_service_and_task_succeeded);
DEFINE_METRIC_count(setup_install_service_and_task_failed);
DEFINE_METRIC_integer(setup_install_service_error);
DEFINE_METRIC_integer(setup_install_task_error);

DEFINE_METRIC_timing(setup_install_service_ms);
DEFINE_METRIC_timing(setup_install_service_failed_ms);
DEFINE_METRIC_timing(setup_install_task_ms);

DEFINE_METRIC_count(setup_start_service_total);
DEFINE_METRIC_count(setup_start_service_succeeded);
DEFINE_METRIC_integer(setup_start_service_error);

DEFINE_METRIC_timing(setup_start_service_ms);
DEFINE_METRIC_timing(setup_start_service_failed_ms);

DEFINE_METRIC_count(setup_start_task_total);
DEFINE_METRIC_count(setup_start_task_succeeded);
DEFINE_METRIC_integer(setup_start_task_error);

DEFINE_METRIC_timing(setup_start_task_ms);

DEFINE_METRIC_count(setup_helper_msi_install_total);
DEFINE_METRIC_count(setup_helper_msi_install_succeeded);

DEFINE_METRIC_timing(setup_helper_msi_install_ms);

DEFINE_METRIC_count(setup_locks_failed);
DEFINE_METRIC_count(setup_lock11_failed);
DEFINE_METRIC_count(setup_lock12_failed);

DEFINE_METRIC_timing(setup_lock_acquire_ms);

DEFINE_METRIC_count(setup_process_wait_failed);
DEFINE_METRIC_count(setup_process_wait_failed_unknown);
DEFINE_METRIC_count(setup_process_wait_failed_core);
DEFINE_METRIC_count(setup_process_wait_failed_report);
DEFINE_METRIC_count(setup_process_wait_failed_update);
DEFINE_METRIC_count(setup_process_wait_failed_ig);
DEFINE_METRIC_count(setup_process_wait_failed_handoff);
DEFINE_METRIC_count(setup_process_wait_failed_ug);
DEFINE_METRIC_count(setup_process_wait_failed_ua);
DEFINE_METRIC_count(setup_process_wait_failed_cr);
DEFINE_METRIC_count(setup_process_wait_failed_legacy);
DEFINE_METRIC_count(setup_process_wait_failed_other);

DEFINE_METRIC_timing(setup_process_wait_ms);

DEFINE_METRIC_count(setup_rollback_version);
DEFINE_METRIC_count(setup_rollback_files);
DEFINE_METRIC_count(setup_files_rollback_shell);

DEFINE_METRIC_count(setup_uac_succeeded);

DEFINE_METRIC_count(setup_user_app_admin);
DEFINE_METRIC_count(setup_machine_app_non_admin);

}  // namespace omaha

