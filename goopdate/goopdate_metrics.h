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

// How many times we received a user/machine legacy handoff from an Omaha 1.0.x
// or 1.1.x metainstaller. These are determined early on in the handoff process
// whereas the 1.0 and 1.1 metrics are not determined until the XML is parsed so
// the version metrics may not sum up to the user/machine sum if errors occur.
DECLARE_METRIC_count(handoff_legacy_user);
DECLARE_METRIC_count(handoff_legacy_machine);
// How many times we received a handoff from an Omaha 1.0.x metainstaller.
DECLARE_METRIC_count(handoff_legacy_10);
// How many times we received a handoff from an Omaha 1.1.x metainstaller.
DECLARE_METRIC_count(handoff_legacy_11);

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
// total number of crashes handled by Omaha.
DECLARE_METRIC_count(oop_crashes_requested);
DECLARE_METRIC_count(oop_crashes_total);
DECLARE_METRIC_count(oop_crashes_uploaded);
DECLARE_METRIC_count(oop_crashes_throttled);
DECLARE_METRIC_count(oop_crashes_rejected);
DECLARE_METRIC_count(oop_crashes_failed);

// How many times StartCrashServer() was called.
DECLARE_METRIC_count(crash_start_server_total);
// How many times StartCrashServer() succeeded.
DECLARE_METRIC_count(crash_start_server_succeeded);

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

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_METRICS_H_
