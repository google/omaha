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

#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/timer.h"

namespace omaha {

// LowResTimer class is implemented on top of ::GetTickCount.
// ::GetTickCount wraps around every 49.7 days. The code can't handle
// this condition so there is a small probability that something can go
// wrong.
LowResTimer::LowResTimer(bool running)
    : running_(false), iterations_(0), elapsed_(0), start_(0) {
  if (running) {
    Start();
  }
}

LowResTimer::~LowResTimer() {
}

void LowResTimer::Reset() {
  elapsed_ = 0;
  running_ = 0;
  iterations_ = 0;
}

void LowResTimer::Start() {
  ASSERT1(!running_);

  start_ = ::GetTickCount();
  running_ = 1;
}

uint32 LowResTimer::Stop() {
  ASSERT1(running_);

  uint32 stop = ::GetTickCount();
  ASSERT1(stop >= start_);
  uint32 diff = stop - start_;
  elapsed_ += diff;
  iterations_++;
  running_ = 0;
  return diff;
}

uint32 LowResTimer::GetMilliseconds() const {
  uint32 running_time = 0;
  if (running_) {
    uint32 now = ::GetTickCount();
    ASSERT1(now >= start_);
    running_time = now - start_;
  }
  return elapsed_ + running_time;
}

// statics
// get the frequency only once
SELECTANY time64 Timer::count_freq_ = 0;

Timer::Timer(bool running)
    : running_(0), iterations_(0), elapsed_(0), start_(0), split_(0) {
  // initialize only once
  if (count_freq_ == 0) {
    count_freq_ = GetRdtscFrequency();
    if (count_freq_ <= 1) {
       UTIL_LOG(LEVEL_ERROR,
          (_T("[Timer::Timer - high-res counter not supported]")));
       count_freq_ = 1;
    }
  }
  if (running) {
    Start();
  }
}

Timer::~Timer() {
}

void Timer::Reset() {
  elapsed_ = 0;
  running_ = 0;
  iterations_ = 0;
}

void Timer::Start() {
  ASSERT1(!running_);

  start_ = GetRdtscCounter();
  split_ = start_;
  running_ = 1;
}

void Timer::Split(double* split_time_ms, double* total_time_ms) {
  ASSERT1(running_);

  time64 now = GetRdtscCounter();
  if (split_time_ms) {
    *split_time_ms = PerfCountToNanoSeconds(now - split_)/ 1000000;
  }
  if (total_time_ms) {
    *total_time_ms =
        PerfCountToNanoSeconds(elapsed_ + (now - start_)) / 1000000;
  }
  split_ = now;
}

time64 Timer::Stop() {
  ASSERT1(running_);

  time64 stop = GetRdtscCounter();
  time64 diff = stop - start_;
  elapsed_ += diff;
  iterations_++;
  running_ = 0;
  return diff;
}

double Timer::GetNanoseconds() const {
  time64 running_time = 0;
  if (running_) {
    time64 now = GetRdtscCounter();
    running_time = now - start_;
  }
  return PerfCountToNanoSeconds(elapsed_ + running_time);
}

#ifdef _DEBUG
CString Timer::DebugString() const {
  CString s;
  double seconds = GetSeconds();
  if (iterations_) {
    s.Format(_T("%s sec %d iterations %s sec/iteration"),
             String_DoubleToString(seconds, 3), iterations_,
             String_DoubleToString(seconds/iterations_, 3));
  } else {
    s.Format(_T("%s sec"), String_DoubleToString(seconds, 3));
  }
  return s;
}
#endif

// Computes the frequency (ticks/sec) for the CPU tick-count timer (RDTSC)
// Don't call this function frequently, because computing the frequency is slow
// (relatively).
//
// TODO(omaha): check return values, and return 0 on failure.
// But hard to imagine a machine where our program will install/run but this
// will fail.
time64 Timer::GetRdtscFrequency() {
  //
  // Get elapsed RDTSC and elapsed QPC over same time period
  //

  // compute length of time period to measure
  time64 freq_qpc = 0;  // ticks per second
  QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq_qpc));

  // fraction of second to run timers for; tradeoff b/w speed and accuracy;
  // 1/1000 (1 msec) seems like good tradeoff
  time64 interval_qpc = freq_qpc / 1000;

  // get timer values over same time period
  time64 begin_qpc = 0;
  time64 end_qpc = 0;

  QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&begin_qpc));

  time64 begin_rdtsc = Timer::GetRdtscCounter();

  // spin and protect against infinite loop, if QPC does something wacky
  int count = 0;
  const int count_max = 50000;
  do {
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&end_qpc));
    ++count;
  } while ((end_qpc - begin_qpc) < interval_qpc  &&  count < count_max);

  QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&end_qpc));
  time64 end_rdtsc = Timer::GetRdtscCounter();

  ASSERT1(count < count_max);

  //
  // Compute RDTSC frequency from QPC frequency
  //

  time64 diff_qpc = end_qpc - begin_qpc;
  time64 diff_rdtsc = end_rdtsc - begin_rdtsc;

  time64 freq_rdtsc = freq_qpc * diff_rdtsc / diff_qpc;

  return freq_rdtsc;
}

}  // namespace omaha

