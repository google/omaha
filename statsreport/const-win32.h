// Copyright 2006-2009 Google Inc.
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
//
// Constants for Win32 stats aggregation and uploading
#ifndef OMAHA_STATSREPORT_CONST_WIN32_H__
#define OMAHA_STATSREPORT_CONST_WIN32_H__

namespace stats_report {

extern const wchar_t kCountsKeyName[];
extern const wchar_t kTimingsKeyName[];
extern const wchar_t kIntegersKeyName[];
extern const wchar_t kBooleansKeyName[];
extern const wchar_t kStatsKeyFormatString[];
extern const wchar_t kLastTransmissionTimeValueName[];

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_CONST_WIN32_H__
