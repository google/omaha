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

// Declares the usage metrics used by goopdate module.

#ifndef OMAHA_GOOPDATE_GOOPDATE_METRICS_H_
#define OMAHA_GOOPDATE_GOOPDATE_METRICS_H_

#include "omaha/statsreport/metrics.h"

namespace omaha {

DECLARE_METRIC_integer(windows_major_version);
DECLARE_METRIC_integer(windows_minor_version);
DECLARE_METRIC_integer(windows_sp_major_version);
DECLARE_METRIC_integer(windows_sp_minor_version);

DECLARE_METRIC_integer(windows_user_profile_type);

// Crash metrics.
//
// A crash can be handled in one of the following ways: uploaded, rejected by
// the server, rejected by the client due to metering, or failed for other
// reasons, such as the sender could not communicate with the crash server.

// In process crash reporting metrics.
DECLARE_METRIC_count(crashes_total);
DECLARE_METRIC_count(crashes_uploaded);
DECLARE_METRIC_count(crashes_throttled);
DECLARE_METRIC_count(crashes_rejected);
DECLARE_METRIC_count(crashes_failed);

// Out of process crash reporting metrics.
// The number of crashes requested by the applications should be close to the
// total number of crashes handled by Omaha.  (Note that crashhandler also has
// some metrics reported here.)
DECLARE_METRIC_count(oop_crashes_total);
DECLARE_METRIC_count(oop_crashes_uploaded);
DECLARE_METRIC_count(oop_crashes_throttled);
DECLARE_METRIC_count(oop_crashes_rejected);
DECLARE_METRIC_count(oop_crashes_failed);
DECLARE_METRIC_count(oop_crash_start_sender);

// Increments every time GoopdateImpl::HandleReportCrash is called.
DECLARE_METRIC_count(goopdate_handle_report_crash);

// How many times the Code Red check process was launched.
DECLARE_METRIC_count(cr_process_total);
// How many times the Code Red download callback was called.
DECLARE_METRIC_count(cr_callback_total);
// How many times the Code Red download callback received 200 - OK.
DECLARE_METRIC_count(cr_callback_status_200);
// How many times the Code Red download callback received 204 - no content.
DECLARE_METRIC_count(cr_callback_status_204);
// How many times the Code Red download callback received some other status.
// Only incremented if DownloadFile() succeeded.
DECLARE_METRIC_count(cr_callback_status_other);

// How many times GoopdateImpl::LoadResourceDll() failed to load the resource
// DLL. Does not include modes that do not need the DLL.
DECLARE_METRIC_count(load_resource_dll_failed);

DECLARE_METRIC_count(goopdate_constructor);
DECLARE_METRIC_count(goopdate_destructor);
DECLARE_METRIC_count(goopdate_main);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_METRICS_H_
