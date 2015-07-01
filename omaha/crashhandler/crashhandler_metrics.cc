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

#include "omaha/crashhandler/crashhandler_metrics.h"

namespace omaha {

DEFINE_METRIC_count(oop_crashes_requested);
DEFINE_METRIC_count(oop_crashes_deferred);

DEFINE_METRIC_count(oop_crashes_crash_filename_empty);
DEFINE_METRIC_count(oop_crashes_convertcustomclientinfotomap_failed);
DEFINE_METRIC_count(oop_crashes_createcustominfofile_failed);
DEFINE_METRIC_count(oop_crashes_startsenderwithcommandline_failed);

DEFINE_METRIC_count(crash_start_server_total);
DEFINE_METRIC_count(crash_start_server_succeeded);

}  // namespace omaha

