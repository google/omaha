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

// Declares the usage metrics used by the crash handler.

#ifndef OMAHA_CRASHHANDLER_CRASHHANDLER_METRICS_H_
#define OMAHA_CRASHHANDLER_CRASHHANDLER_METRICS_H_

#include "omaha/statsreport/metrics.h"

namespace omaha {

// Out of process crash reporting metrics.
// The number of crashes requested by the applications should be close to the
// total number of crashes handled by Omaha.  (Note that goopdate also has some
// crash metrics that it reports, since it handles the uploads.)
DECLARE_METRIC_count(oop_crashes_requested);
DECLARE_METRIC_count(oop_crashes_deferred);

DECLARE_METRIC_count(oop_crashes_crash_filename_empty);
DECLARE_METRIC_count(oop_crashes_convertcustomclientinfotomap_failed);
DECLARE_METRIC_count(oop_crashes_createcustominfofile_failed);
DECLARE_METRIC_count(oop_crashes_startsenderwithcommandline_failed);

// How many times StartCrashServer() was called.
DECLARE_METRIC_count(crash_start_server_total);
// How many times StartCrashServer() succeeded.
DECLARE_METRIC_count(crash_start_server_succeeded);

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASHHANDLER_METRICS_H_
