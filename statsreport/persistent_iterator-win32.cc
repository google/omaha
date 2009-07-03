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
// Iterator over persisted metrics
#include "persistent_iterator-win32.h"

namespace stats_report {

void PersistentMetricsIteratorWin32::Next() {
  current_value_.reset();

  // Try to open the top-level key if we didn't already.
  if (NULL == key_.m_hKey) {
    HKEY parent_key = is_machine_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    LONG err = key_.Open(parent_key, key_name_, KEY_READ);
    if (err != ERROR_SUCCESS)
      return;
  }

  // Loop until we find a value
  while (state_ != kFinished) {
    if (NULL == sub_key_.m_hKey) {
      const wchar_t *subkey_name = NULL;
      switch (state_) {
       case kUninitialized:
        state_ = kCounts;
        subkey_name = kCountsKeyName;
        break;
       case kCounts:
        state_ = kTimings;
        subkey_name = kTimingsKeyName;
        break;
       case kTimings:
        state_ = kIntegers;
        subkey_name = kIntegersKeyName;
        break;
       case kIntegers:
        state_ = kBooleans;
        subkey_name = kBooleansKeyName;
        break;
       case kBooleans:
        state_ = kFinished;
        break;
       case kFinished:
        break;
      }

      if (NULL != subkey_name) {
        LONG err = sub_key_.Open(key_, subkey_name, KEY_READ);
        // go around the loop on error to try the next key type
        if (ERROR_SUCCESS != err)
          continue;
      }

      // reset value enumeration
      value_index_ = 0;
    }

    if (state_ != kFinished) {
      DCHECK(NULL != sub_key_.m_hKey);
      CString wide_value_name;
      DWORD value_name_len = 255;
      DWORD value_type = 0;
      BYTE buf[sizeof(TimingMetric::TimingData)];
      DWORD value_len = sizeof(buf);

      // Get the next key and value
      LONG err = ::RegEnumValue(sub_key_, value_index_,
                    CStrBuf(wide_value_name, value_name_len), &value_name_len,
                    0, &value_type,
                    buf, &value_len);

      ++value_index_;

      if (ERROR_NO_MORE_ITEMS == err) {
        // done with this subkey, go around again
        sub_key_.Close();
        continue;
      } else if (ERROR_SUCCESS != err) {
        // some other error, broken into a separate case for ease of debugging
        DCHECK(false && "Unexpected error during reg value enumeration");
      } else {
        DCHECK(ERROR_SUCCESS == err);

        // convert value to ASCII
        current_value_name_ = wide_value_name;

        switch (state_) {
         case kCounts:
          if (value_len != sizeof(uint64))
            continue;
          current_value_.reset(new CountMetric(current_value_name_ .GetString(),
                                          *reinterpret_cast<uint64*>(&buf[0])));
          break;
         case kTimings:
          if (value_len != sizeof(TimingMetric::TimingData))
            continue;
          current_value_.reset(new TimingMetric(current_value_name_.GetString(),
                        *reinterpret_cast<TimingMetric::TimingData*>(&buf[0])));
          break;
         case kIntegers:
          if (value_len != sizeof(uint64))
            continue;
          current_value_.reset(new IntegerMetric(
                                      current_value_name_.GetString(),
                                      *reinterpret_cast<uint64*>(&buf[0])));
          break;
         case kBooleans:
          if (value_len != sizeof(uint32))
            continue;
          current_value_.reset(new BoolMetric(current_value_name_.GetString(),
                                          *reinterpret_cast<uint32*>(&buf[0])));
          break;
         default:
          DCHECK(false && "Impossible state during reg value enumeration");
          break;
        }

        if (current_value_.get())
          return;
      }
    }
  }
}

} // namespace stats_report
