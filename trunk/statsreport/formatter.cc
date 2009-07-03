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
//
#include "formatter.h"

namespace stats_report {

Formatter::Formatter(const char *name, uint32 measurement_secs) {
  output_ << name << "&" << measurement_secs;
}

Formatter::~Formatter() {
}

void Formatter::AddCount(const char *name, uint64 value) {
  output_ << "&" << name << ":c=" << value;
}

void Formatter::AddTiming(const char *name, uint64 num, uint64 avg,
                          uint64 min, uint64 max) {
  output_ << "&" << name << ":t=" << num << ";"
                                  << avg << ";" << min << ";" << max;
}

void Formatter::AddInteger(const char *name, uint64 value) {
  output_ << "&" << name << ":i=" << value;
}

void Formatter::AddBoolean(const char *name, bool value) {
  output_ << "&" << name << ":b=" << (value ? "t" : "f");
}

void Formatter::AddMetric(MetricBase *metric) {
  switch (metric->type()) {
    case kCountType: {
      CountMetric &count = metric->AsCount();
      AddCount(count.name(), count.value());
    }
    break;

    case kTimingType: {
      TimingMetric &timing = metric->AsTiming();
      AddTiming(timing.name(), timing.count(), timing.average(),
                timing.minimum(), timing.maximum());
    }
    break;

    case kIntegerType: {
      IntegerMetric &integer = metric->AsInteger();
      AddInteger(integer.name(), integer.value());
    }
    break;

    case kBoolType: {
      BoolMetric &boolean = metric->AsBool();
      // TODO(omaha): boolean.value() returns a TristateBoolValue. The
      // formatter is going to serialize kBoolUnset to true.
      DCHECK_NE(boolean.value(), BoolMetric::kBoolUnset);
      AddBoolean(boolean.name(), boolean.value() != BoolMetric::kBoolFalse);
    }
    break;

    default:
      DCHECK(false && "Impossible metric type");
  }
}

} // namespace stats_report
