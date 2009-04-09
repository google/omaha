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

#include "omaha/core/core_metrics.h"

namespace omaha {

DEFINE_METRIC_integer(core_working_set);
DEFINE_METRIC_integer(core_peak_working_set);

DEFINE_METRIC_integer(core_handle_count);

DEFINE_METRIC_integer(core_uptime_ms);
DEFINE_METRIC_integer(core_kernel_time_ms);
DEFINE_METRIC_integer(core_user_time_ms);

DEFINE_METRIC_integer(core_disk_space_available);

DEFINE_METRIC_count(core_worker_total);
DEFINE_METRIC_count(core_worker_succeeded);
DEFINE_METRIC_count(core_cr_total);
DEFINE_METRIC_count(core_cr_succeeded);

DEFINE_METRIC_integer(core_cr_expected_timer_interval_ms);
DEFINE_METRIC_integer(core_cr_actual_timer_interval_ms);

DEFINE_METRIC_count(core_start_crash_handler_total);
DEFINE_METRIC_count(core_start_crash_handler_succeeded);

DEFINE_METRIC_count(core_run_scheduled_task_missing);
DEFINE_METRIC_count(core_run_service_missing);
}  // namespace omaha

