// Copyright 2008-2010 Google Inc.
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

// Constants for the statsreport library.

#ifndef OMAHA_GOOPDATE_STATS_UPLOADER_H__
#define OMAHA_GOOPDATE_STATS_UPLOADER_H__

#include <windows.h>
#include <tchar.h>

namespace omaha {

// The product name is chosen so that the stats are persisted under
// the Google Update registry key for the machine or user, respectively.
const TCHAR* const kMetricsProductName           = _T("Update");

const TCHAR* const kMetricsServerParamSourceId   = _T("sourceid");
const TCHAR* const kMetricsServerParamVersion    = _T("v");
const TCHAR* const kMetricsServerParamIsMachine  = _T("ismachine");
const TCHAR* const kMetricsServerTestSource      = _T("testsource");

// Metrics are uploaded every 25 hours.
const int kMetricsUploadIntervalSec              = 25 * 60 * 60;

// Deletes existing metrics and initializes 'LastTransmission' to current time.
HRESULT ResetMetrics(bool is_machine);

// Aggregates metrics by saving them in registry.
HRESULT AggregateMetrics(bool is_machine);

// Aggregates and reports the metrics if needed, as defined by the metrics
// upload interval. The interval is ignored when 'force_report' is true.
HRESULT AggregateAndReportMetrics(bool is_machine, bool force_report);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_STATS_UPLOADER_H__

