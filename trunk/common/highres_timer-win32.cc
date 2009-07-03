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
#include "omaha/common/highres_timer-win32.h"

namespace omaha {

bool HighresTimer::perf_freq_collected_ = false;
ULONGLONG HighresTimer::perf_freq_ = 0;

ULONGLONG HighresTimer::GetElapsedMs() const {
  ULONGLONG end_time = GetCurrentTicks();

  // Scale to ms and round to nearerst ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  // Given infinite resolution, this expression could be written as:
  //  trunc((end - start (units:freq*sec))/freq (units:sec) *
  //                1000 (unit:ms) + 1/2 (unit:ms))
  ULONGLONG freq = GetTimerFrequency();
  return ((end_time - start_ticks_) * 1000L + freq / 2) / freq;
}

ULONGLONG HighresTimer::GetElapsedSec() const {
  ULONGLONG end_time = GetCurrentTicks();

  // Scale to ms and round to nearerst ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  // Given infinite resolution, this expression could be written as:
  //  trunc((end - start (units:freq*sec))/freq (unit:sec) + 1/2 (unit:sec))
  ULONGLONG freq = GetTimerFrequency();
  return ((end_time - start_ticks_) + freq / 2) / freq;
}

void HighresTimer::CollectPerfFreq() {
  LARGE_INTEGER freq;

  // Note that this is racy.
  // It's OK, however, because even concurrent executions of this
  // are idempotent.
  if (::QueryPerformanceFrequency(&freq)) {
    perf_freq_ = freq.QuadPart;
    perf_freq_collected_ = true;
  }
}

}  // namespace omaha

