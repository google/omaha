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
// Utility functions for Win32 stats aggregation and uploading
#ifndef OMAHA_STATSREPORT_UTIL_WIN32_H__
#define OMAHA_STATSREPORT_UTIL_WIN32_H__

namespace stats_report {

template <class ValueType>
bool GetData(CRegKey &parent, const wchar_t *value_name, ValueType *value) {
  ULONG len = sizeof(ValueType);
  LONG err = parent.QueryBinaryValue(value_name, value, &len);
  if (ERROR_SUCCESS != err || len != sizeof(ValueType)) {
    memset(value, 0, sizeof(ValueType));
    return false;
  }

  return true;
}

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_UTIL_WIN32_H__
