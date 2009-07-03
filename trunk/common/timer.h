// Copyright 2003-2009 Google Inc.
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
// Timing

#ifndef OMAHA_COMMON_TIMER_H__
#define OMAHA_COMMON_TIMER_H__

#include "omaha/common/debug.h"

namespace omaha {

// Low resolution timer which can be used to time loops. The resolution depends
// on the platform and can be expected to be about 10 ms on Windows 2000 and up.
class LowResTimer {
 public:
  explicit LowResTimer(bool running);
  ~LowResTimer();

  // LowResTimer keeps track of elapsed time, which can consist of multiple
  // Start()-Stop() intervals.
  void Start();

  // Returns time between Start and Stop call and increments the time elapsed.
  uint32 Stop();
  void Reset();

  // Return time in seconds, milliseconds.
  double GetSeconds() const;
  uint32 GetMilliseconds() const;

  // Gets the number of iteration this timer was started/stopped.
  uint32 GetIterations() const { return iterations_; }
  bool IsRunning() const { return running_; }

 private:

  bool running_;
  uint32 start_;
  uint32 elapsed_;
  uint32 iterations_;

  DISALLOW_EVIL_CONSTRUCTORS(LowResTimer);
};

inline double LowResTimer::GetSeconds() const {
  return static_cast<double>(GetMilliseconds()) / 1000;
}

// WARNING - Timer is implemented on top of RDTSC and
// QueryPerformanceCounter which have undefined behavior when running
// on multi-core, speedstep, bugs in HAL, certain chipsets, etc.
// Do not use the Timer in production code where the execution flow
// depends on timing. Timer is primarily intended to be used for
// code profiling and performance measurements.
class Timer {
 public:
  explicit Timer(bool running);
  ~Timer();

  // Timer keeps track of elapsed time, which can consist of multiple
  // Start()-Stop() intervals.
  void Start();

  // returns time for last split (elapsed time since Start() or time since
  // last Split(), whichever came last) as well as total elapsed time.
  void Split(double* split_time_ms, double* total_time_ms);

  // returns time elapsed (in hi-res perf-counts) between Start and Stop call
  // and increments the time elapsed_
  time64 Stop();
  void Reset();

  // return time in seconds, milliseconds, etc.
  double GetSeconds() const;
  double GetMilliseconds() const;
  double GetMicroseconds() const;
  double GetNanoseconds() const;
  time64 Get100Nanoseconds() const;

  // TODO(omaha): Probably should have been a static method, or even
  // standalone func convert the high-perf counter to nano-seconds
  double PerfCountToNanoSeconds(time64 perf_count) const;

  // get the number of iteration this timer was started/stopped
  uint32 GetIterations() const { return iterations_; }
  bool IsRunning() const { return running_; }

  // useful funcs beyond just the Timer class
  static time64 GetRdtscCounter();  // return perf-counter value
  static time64 GetRdtscFrequency();  // return perf-counter frequency

    // outputs total time, number of iterations, and the average time
#ifdef _DEBUG
  CString DebugString() const;
#endif

 private:

  bool running_;
  time64 start_;
  time64 split_;
  time64 elapsed_;
  uint32 iterations_;
  static time64 count_freq_;

  DISALLOW_EVIL_CONSTRUCTORS(Timer);
};

// lint -e{533}  Function should return a value
// lint -e{31}   Redefinition of symbol
__forceinline time64 Timer::GetRdtscCounter() { __asm rdtsc }

inline double Timer::PerfCountToNanoSeconds(time64 perf_count) const {
  return (static_cast<double>(perf_count) / static_cast<double>(count_freq_)) *
         static_cast<double>(1000000000.0);
}

inline double Timer::GetSeconds() const {
  return GetNanoseconds() / 1000000000;
}

inline double Timer::GetMilliseconds() const {
  return GetNanoseconds() / 1000000;
}

inline double Timer::GetMicroseconds() const {
  return GetNanoseconds() / 1000;
}

inline time64 Timer::Get100Nanoseconds() const {
  return (time64) GetNanoseconds() / 100;
}

// Helper class which starts the timer in its constructor and stops it
// in its destructor.  This prevents accidentally leaving the timer running
// if a function has an early exit.
//
// Usage:
//
// class A {
//   Timer timer_;
//
//   void foo(){
//     TimerScope (timer_);
//   ......
//   }  // end foo
//
// Everything is timed till the end of the function or when it returns
// from any place.

class TimerScope {
 public:
  explicit TimerScope(Timer *timer) : timer_(timer) {
    if (timer_) {
      timer_->Start();
    }
  }

  ~TimerScope() {
    if (timer_ && timer_->IsRunning()) {
      timer_->Stop();
    }
  }

 private:
  Timer *timer_;
  DISALLOW_EVIL_CONSTRUCTORS(TimerScope);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_TIMER_H__

