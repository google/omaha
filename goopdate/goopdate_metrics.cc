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

#include "omaha/goopdate/goopdate_metrics.h"

namespace omaha {

DEFINE_METRIC_integer(windows_major_version);
DEFINE_METRIC_integer(windows_minor_version);
DEFINE_METRIC_integer(windows_sp_major_version);
DEFINE_METRIC_integer(windows_sp_minor_version);

DEFINE_METRIC_count(handoff_legacy_user);
DEFINE_METRIC_count(handoff_legacy_machine);
DEFINE_METRIC_count(handoff_legacy_10);
DEFINE_METRIC_count(handoff_legacy_11);

DEFINE_METRIC_count(crashes_total);
DEFINE_METRIC_count(crashes_uploaded);
DEFINE_METRIC_count(crashes_throttled);
DEFINE_METRIC_count(crashes_rejected);
DEFINE_METRIC_count(crashes_failed);

DEFINE_METRIC_count(oop_crashes_requested);
DEFINE_METRIC_count(oop_crashes_total);
DEFINE_METRIC_count(oop_crashes_uploaded);
DEFINE_METRIC_count(oop_crashes_throttled);
DEFINE_METRIC_count(oop_crashes_rejected);
DEFINE_METRIC_count(oop_crashes_failed);
DEFINE_METRIC_count(oop_crashes_crash_filename_empty);
DEFINE_METRIC_count(oop_crashes_createcustominfofile_failed);
DEFINE_METRIC_count(oop_crashes_startsenderwithcommandline_failed);
DEFINE_METRIC_count(oop_crash_start_sender);

DEFINE_METRIC_count(goopdate_handle_report_crash);

DEFINE_METRIC_count(crash_start_server_total);
DEFINE_METRIC_count(crash_start_server_succeeded);

DEFINE_METRIC_count(cr_process_total);
DEFINE_METRIC_count(cr_callback_total);
DEFINE_METRIC_count(cr_callback_status_200);
DEFINE_METRIC_count(cr_callback_status_204);
DEFINE_METRIC_count(cr_callback_status_other);

DEFINE_METRIC_count(load_resource_dll_failed);

DEFINE_METRIC_count(goopdate_constructor);
DEFINE_METRIC_count(goopdate_destructor);
DEFINE_METRIC_count(goopdate_main);

}  // namespace omaha
