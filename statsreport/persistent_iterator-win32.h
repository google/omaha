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
#ifndef OMAHA_STATSREPORT_PERSISTENT_ITERATOR_WIN32_H__
#define OMAHA_STATSREPORT_PERSISTENT_ITERATOR_WIN32_H__

#include "base/scoped_ptr.h"
#include "metrics.h"
#include "const-win32.h"

#include <atlbase.h>
#include <atlstr.h>
#include <iterator>

namespace stats_report {

/// Forward iterator for persisted metrics
class PersistentMetricsIteratorWin32
    : public std::iterator<std::forward_iterator_tag, const MetricBase *> {
public:
  /// @param app_name see MetricsAggregatorWin32
  explicit PersistentMetricsIteratorWin32(const wchar_t *app_name)
      : state_(kUninitialized),
       is_machine_(false) {
    key_name_.Format(kStatsKeyFormatString, app_name);
    Next();
  }

  /// @param app_name see MetricsAggregatorWin32
  /// @param is_machine specifies the registry hive
  PersistentMetricsIteratorWin32(const wchar_t *app_name, bool is_machine)
      : state_(kUninitialized),
        is_machine_(is_machine) {
    key_name_.Format(kStatsKeyFormatString, app_name);
    Next();
  }

  /// Constructs the at-end iterator
  PersistentMetricsIteratorWin32() : state_(kUninitialized) {
  }

  MetricBase *operator* () {
    return Current();
  }
  MetricBase *operator-> () {
    return Current();
  }

  /// Preincrement, we don't implement postincrement because we don't
  /// want to deal with making iterators copyable, comparable etc.
  PersistentMetricsIteratorWin32 &operator++() {
    Next();

    return (*this);
  }

  /// Compare for equality with o.
  bool equals(const PersistentMetricsIteratorWin32 &o) const {
    // compare equal to self, and end iterators compare equal
    if ((this == &o) || (NULL == current_value_.get() &&
                         NULL == o.current_value_.get()))
      return true;

    return false;
  }

private:
  MetricBase *Current() {
    DCHECK(current_value_.get());
    return current_value_.get();
  }

  enum IterationState {
    kUninitialized,
    kCounts,
    kTimings,
    kIntegers,
    kBooleans,
    kFinished,
  };

  /// Walk to the next key/value under iteration
  void Next();

  /// Keeps track of which subkey we're iterating over
  IterationState state_;

  /// The full path from HKCU to the key we iterate over
  CString key_name_;

  /// The top-level key we're iterating over, valid only
  /// after first call to Next().
  CRegKey key_;

  /// The subkey we're currently enumerating over
  CRegKey sub_key_;

  /// Current value we're indexing over
  DWORD value_index_;

  /// Name of the value under the iterator
  CStringA current_value_name_;

  /// The metric under the iterator
  scoped_ptr<MetricBase> current_value_;

  /// Specifies HKLM or HKCU, respectively.
  bool is_machine_;
};

inline bool operator == (const PersistentMetricsIteratorWin32 &a,
                         const PersistentMetricsIteratorWin32 &b) {
  return a.equals(b);
}

inline bool operator != (const PersistentMetricsIteratorWin32 &a,
                         const PersistentMetricsIteratorWin32 &b) {
  return !a.equals(b);
}

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_PERSISTENT_ITERATOR_WIN32_H__
