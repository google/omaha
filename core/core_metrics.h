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

// Declares the usage metrics used by core module.

#ifndef OMAHA_CORE_CORE_METRICS_H_
#define OMAHA_CORE_CORE_METRICS_H_

#include "omaha/statsreport/metrics.h"

namespace omaha {

// Core process working set and peak working set.
DECLARE_METRIC_integer(core_working_set);
DECLARE_METRIC_integer(core_peak_working_set);

// Core process handle count.
DECLARE_METRIC_integer(core_handle_count);

// Core process uptime, kernel, and user times.
DECLARE_METRIC_integer(core_uptime_ms);
DECLARE_METRIC_integer(core_kernel_time_ms);
DECLARE_METRIC_integer(core_user_time_ms);

// How much free space is availble on the current drive where Omaha is
// installed.
DECLARE_METRIC_integer(core_disk_space_available);

// How many worker and code red processes are started by the core.
DECLARE_METRIC_count(core_worker_total);
DECLARE_METRIC_count(core_worker_succeeded);
DECLARE_METRIC_count(core_cr_total);
DECLARE_METRIC_count(core_cr_succeeded);

// The period of code red checks.
DECLARE_METRIC_integer(core_cr_expected_timer_interval_ms);
DECLARE_METRIC_integer(core_cr_actual_timer_interval_ms);

// How many times StartCrashHandler() was called.
DECLARE_METRIC_count(core_start_crash_handler_total);
// How many times StartCrashHandler() succeeded.
DECLARE_METRIC_count(core_start_crash_handler_succeeded);

// Service and scheduled task metrics.
DECLARE_METRIC_count(core_run_not_checking_for_updates);
DECLARE_METRIC_count(core_run_task_scheduler_not_running);
DECLARE_METRIC_count(core_run_scheduled_task_missing);
DECLARE_METRIC_count(core_run_scheduled_task_disabled);
DECLARE_METRIC_count(core_run_service_missing);
DECLARE_METRIC_count(core_run_service_disabled);
DECLARE_METRIC_integer(core_run_scheduled_task_exit_code);

}  // namespace omaha

#endif  // OMAHA_CORE_CORE_METRICS_H_

