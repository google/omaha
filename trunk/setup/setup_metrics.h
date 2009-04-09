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

// Declares the usage metrics used by the setup module.

#ifndef OMAHA_SETUP_SETUP_METRICS_H__
#define OMAHA_SETUP_SETUP_METRICS_H__

#include "omaha/statsreport/metrics.h"

namespace omaha {

// How many times the Install() was called.
DECLARE_METRIC_count(setup_install_total);
// How many times the Install() method succeeded.
DECLARE_METRIC_count(setup_install_succeeded);

// How many times the UpdateSelfSilently() was called.
DECLARE_METRIC_count(setup_update_self_total);
// How many times the UpdateSelfSilently() method succeeded.
DECLARE_METRIC_count(setup_update_self_succeeded);

// How many times the Install() resulted in a Google Update install attempt.
DECLARE_METRIC_count(setup_do_self_install_total);
// How many times the Install() succeeded in installing Google Update.
DECLARE_METRIC_count(setup_do_self_install_succeeded);

// How many times the Install() resulted in a handoff attempt.
// Does not include legacy handoffs, which do not go through Setup.
DECLARE_METRIC_count(setup_handoff_only_total);
// How many times the Install() successfully handed off.
// Does not include legacy handoffs, which do not go through Setup.
DECLARE_METRIC_count(setup_handoff_only_succeeded);

// Total time (ms) spent installing Google Update as measured by time between
// when DoInstall() is called and Setup phase 2 completes. Excludes handoffs.
DECLARE_METRIC_timing(setup_install_google_update_total_ms);

// Time (ms) from Setup starts until hands off to existing Omaha installation.
DECLARE_METRIC_timing(setup_handoff_ms);
// Time (ms) from Setup starts until the hand off process displays its UI.
DECLARE_METRIC_timing(setup_handoff_ui_ms);

// How many times Setup was run when Omaha was already installed but the core
// was not running. Only incremented if existing Omaha version >= 1.2.0.0.
DECLARE_METRIC_count(setup_installed_core_not_running);

//
// ShouldInstall metrics
//

// Times ShouldInstall() was called.
DECLARE_METRIC_count(setup_should_install_total);

// Times ShouldInstall() returned false because of OneClick case.
DECLARE_METRIC_count(setup_should_install_false_oc);

// Times ShouldInstall() returned true because not already installed.
DECLARE_METRIC_count(setup_should_install_true_fresh_install);

// Times ShouldInstall() returned false because older version than installed.
DECLARE_METRIC_count(setup_should_install_false_older);

// Times ShouldInstall() returned true because newer version than installed.
DECLARE_METRIC_count(setup_should_install_true_newer);

// Times ShouldInstall() returned false because same version fully installed.
DECLARE_METRIC_count(setup_should_install_false_same);
// Times ShouldInstall() returned true because same version not fully installed.
DECLARE_METRIC_count(setup_should_install_true_same);
// Times ShouldOverinstallSameVersion() returned true because the successful
// install completion indicator was not present.
DECLARE_METRIC_count(setup_should_install_true_same_completion_missing);

// Times ShouldInstall() was called during a metainstaller installation when
// some Omaha version was already installed.
DECLARE_METRIC_count(setup_subsequent_install_total);
// Times ShouldInstall() returned true during a metainstaller installation when
// some Omaha version was already installed.
DECLARE_METRIC_count(setup_subsequent_install_should_install_true);

//
// Setup Files
//

// How many times Setup attempted to install the files.
DECLARE_METRIC_count(setup_files_total);
// How many times Setup successfully installed the files.
DECLARE_METRIC_count(setup_files_verification_succeeded);
// How many times file install failed due to file verification before copy.
DECLARE_METRIC_count(setup_files_verification_failed_pre);
// How many times file install failed due to file verification after copy.
DECLARE_METRIC_count(setup_files_verification_failed_post);

// Total time (ms) spent installing files.
DECLARE_METRIC_timing(setup_files_ms);

// How many times the shell was replaced.
DECLARE_METRIC_count(setup_files_replace_shell);

//
// Setup Phase 2
//

// Total time (ms) spent in Setup phase 2.
DECLARE_METRIC_timing(setup_phase2_ms);

// How many times Setup attempted to install the service and scheduled task.
DECLARE_METRIC_count(setup_install_service_task_total);
// How many times Setup successfully installed the service.
DECLARE_METRIC_count(setup_install_service_succeeded);
// How many times Setup successfully installed the scheduled task.
DECLARE_METRIC_count(setup_install_task_succeeded);
// How many times Setup successfully installed both service and scheduled task.
DECLARE_METRIC_count(setup_install_service_and_task_succeeded);
// How many times Setup failed to install both the service and scheduled task.
DECLARE_METRIC_count(setup_install_service_and_task_failed);
// The error returned by InstallService().
DECLARE_METRIC_integer(setup_install_service_error);
// The error returned by InstallScheduledTask().
DECLARE_METRIC_integer(setup_install_task_error);

// Time (ms) it took to install the service.
DECLARE_METRIC_timing(setup_install_service_ms);
// Time (ms) waited for the service to install when it failed to install.
DECLARE_METRIC_timing(setup_install_service_failed_ms);
// Time (ms) it took to install the scheduled task.
DECLARE_METRIC_timing(setup_install_task_ms);


// How many times Setup attempted to start the service.
DECLARE_METRIC_count(setup_start_service_total);
// How many times Setup successfully started the service.
DECLARE_METRIC_count(setup_start_service_succeeded);
// The error returned by StartService().
DECLARE_METRIC_integer(setup_start_service_error);

// Time (ms) it took to start the service.
DECLARE_METRIC_timing(setup_start_service_ms);
// Time (ms) waited for the service to start when it failed to start.
DECLARE_METRIC_timing(setup_start_service_failed_ms);

// How many times Setup attempted to start the scheduled task.
DECLARE_METRIC_count(setup_start_task_total);
// How many times Setup successfully started the scheduled task.
DECLARE_METRIC_count(setup_start_task_succeeded);
// The error returned by StartScheduledTask().
DECLARE_METRIC_integer(setup_start_task_error);

// Time (ms) it took to start scheduled task.
DECLARE_METRIC_timing(setup_start_task_ms);

// How many times Setup attempted to install the helper MSI.
DECLARE_METRIC_count(setup_helper_msi_install_total);
// How many times Setup successfully installed the helper MSI.
DECLARE_METRIC_count(setup_helper_msi_install_succeeded);

// Time (ms) it took to install the helper MSI.
DECLARE_METRIC_timing(setup_helper_msi_install_ms);

//
// Specific Setup Failures
//

// How many times Setup failed to get any of the Setup locks.
DECLARE_METRIC_count(setup_locks_failed);
// How many times Setup failed to get the 1.1 Setup Lock.
DECLARE_METRIC_count(setup_lock11_failed);
// How many times Setup failed to get the 1.2 Setup Lock.
DECLARE_METRIC_count(setup_lock12_failed);

// Time (ms) it took to acquire all Setup locks.
DECLARE_METRIC_timing(setup_lock_acquire_ms);


// How many times Setup failed waiting for processes to stop - all modes.
DECLARE_METRIC_count(setup_process_wait_failed);
// How many times Setup failed waiting for processes to stop - specific modes.
// The sum of these should equal setup_process_wait_failed.
DECLARE_METRIC_count(setup_process_wait_failed_unknown);
DECLARE_METRIC_count(setup_process_wait_failed_core);
DECLARE_METRIC_count(setup_process_wait_failed_report);
DECLARE_METRIC_count(setup_process_wait_failed_update);
DECLARE_METRIC_count(setup_process_wait_failed_ig);
DECLARE_METRIC_count(setup_process_wait_failed_handoff);
DECLARE_METRIC_count(setup_process_wait_failed_ug);
DECLARE_METRIC_count(setup_process_wait_failed_ua);
DECLARE_METRIC_count(setup_process_wait_failed_cr);
DECLARE_METRIC_count(setup_process_wait_failed_legacy);  // Any legacy mode.
DECLARE_METRIC_count(setup_process_wait_failed_other);   // All other modes.

// Time (ms) spent waiting for processes to exit - both successes and failures.
DECLARE_METRIC_timing(setup_process_wait_ms);

//
// Other Setup Statistics
//

// How many times Setup rolled back the version.
DECLARE_METRIC_count(setup_rollback_version);
// How many times Setup rolled back file installation.
DECLARE_METRIC_count(setup_rollback_files);
// How many times Setup rolled back the shell.
DECLARE_METRIC_count(setup_files_rollback_shell);

// How many times Setup ran after elevating.
DECLARE_METRIC_count(setup_uac_succeeded);

// How many times Setup installed a user app as an elevated admin on >= Vista.
DECLARE_METRIC_count(setup_user_app_admin);
// How many times Setup attempted to install machine app as non-admin on <Vista.
DECLARE_METRIC_count(setup_machine_app_non_admin);

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_METRICS_H__
