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
#ifndef OMAHA_COMMON_HIGHRES_TIMER_WIN32_H__
#define OMAHA_COMMON_HIGHRES_TIMER_WIN32_H__

#include <windows.h>

namespace omaha {

/// A handy class for reliably measuring wall-clock time with decent resolution,
/// even on multi-processor machines and on laptops (where RDTSC potentially
/// returns different results on different processors and/or the RDTSC timer
/// clocks at different rates depending on the power state of the CPU,
/// respectively).
class HighresTimer {
 public:
  /// Captures the current start time
  HighresTimer();

  /// Captures the current tick, can be used to reset a timer for reuse.
  void Start();

  /// Returns the elapsed ticks with full resolution
  ULONGLONG GetElapsedTicks() const;

  /// Returns the elapsed time in milliseconds, rounded to the nearest
  /// millisecond.
  ULONGLONG GetElapsedMs() const;

  /// Returns the elapsed time in seconds, rounded to the nearest second.
  ULONGLONG GetElapsedSec() const;

  ULONGLONG start_ticks() const { return start_ticks_; }

  /// Returns timer frequency from cache, should be less
  /// overhead than ::QueryPerformanceFrequency
  static ULONGLONG GetTimerFrequency();
  /// Returns current ticks
  static ULONGLONG GetCurrentTicks();

 private:
  static void CollectPerfFreq();

  /// Captured start time
  ULONGLONG start_ticks_;

  /// Captured performance counter frequency
  static bool perf_freq_collected_;
  static ULONGLONG perf_freq_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline ULONGLONG HighresTimer::GetTimerFrequency() {
  if (!perf_freq_collected_)
    CollectPerfFreq();
  return perf_freq_;
}

inline ULONGLONG HighresTimer::GetCurrentTicks() {
  LARGE_INTEGER ticks;
  ::QueryPerformanceCounter(&ticks);
  return ticks.QuadPart;
}

inline ULONGLONG HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

}  // namespace omaha

#endif  // OMAHA_COMMON_HIGHRES_TIMER_WIN32_H__
