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
// Utility class to format metrics to a string suitable for posting to
// TB stats server.
#ifndef OMAHA_STATSREPORT_FORMATTER_H__
#define OMAHA_STATSREPORT_FORMATTER_H__

#include "base/basictypes.h"
#include "metrics.h"
#include <strstream>

namespace stats_report {

/// A utility class that knows how to turn metrics into a string for
/// reporting to the Toolbar stats server.
/// This code is mostly appropriated from the toolbars stats formatter
class Formatter {
public:
  /// @param name the name of the application to report stats against
  Formatter(const char *name, uint32 measurement_secs);
  ~Formatter();

  /// Add metric to the output string
  void AddMetric(MetricBase *metric);

  /// Add typed metrics to the output string
  /// @{
  void AddCount(const char *name, uint64 value);
  void AddTiming(const char *name, uint64 num, uint64 avg, uint64 min,
                 uint64 max);
  void AddInteger(const char *name, uint64 value);
  void AddBoolean(const char *name, bool value);
  /// @}

  /// Terminates the output string and returns it.
  /// It is an error to add metrics after output() is called.
  const char *output() { output_ << std::ends; return output_.str(); }

private:
  DISALLOW_EVIL_CONSTRUCTORS(Formatter);

  mutable std::strstream output_;
};

} // namespace stats_report

#endif  // OMAHA_STATSREPORT_FORMATTER_H__
