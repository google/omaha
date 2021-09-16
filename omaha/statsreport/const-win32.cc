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
#include "const-win32.h"

#include <tchar.h>

namespace stats_report {

const wchar_t kTimingsKeyName[] = L"Timings";
const wchar_t kCountsKeyName[] = L"Counts";
const wchar_t kIntegersKeyName[] = L"Integers";
const wchar_t kBooleansKeyName[] = L"Booleans";
const wchar_t kStatsKeyFormatString[] = L"Software\\"
                                        _T(PATH_COMPANY_NAME_ANSI)
                                        L"\\%ws\\UsageStats\\Daily";
const wchar_t kLastTransmissionTimeValueName[] = L"LastTransmission";

} // namespace stats_report
