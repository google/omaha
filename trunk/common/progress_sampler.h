// Copyright 2011 Google Inc.
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

#ifndef OMAHA_COMMON_PROGRESS_SAMPLER_H_
#define OMAHA_COMMON_PROGRESS_SAMPLER_H_

#include <windows.h>
#include <algorithm>
#include <queue>
#include "omaha/base/debug.h"
#include "omaha/base/time.h"

namespace omaha {

// This class keeps track of the input data (samples) and helps to calculate
// the average progress based on that.
//
// Example usage:
//   // Create a sampler that keep samples within last 500ms and calculates
//   // average progress only if minimum time range 100ms is reached.
//   ProgressSampler<int> progress_sampler(500, 100);
//   ASSERT1(!progress_sampler.HasEnoughSamples());
//
//   progress_sampler.AddSample(0, 100);
//   ASSERT1(!progress_sampler.HasEnoughSamples());
//
//   progress_sampler.AddSample(20, 200);
//   // Minimum time range 100ms has not been reached yet.
//   ASSERT1(!progress_sampler.HasEnoughSamples());
//
//   progress_sampler.AddSample(200, 300);
//   ASSERT1(progress_sampler.HasEnoughSamples());
//   // Samples in queue: [(0, 100), (20, 200), (200, 300)].
//   // Average speed: (300-100) / (200-0) = 1.
//   ASSERT1(1 == progress_sampler.GetAverageProgressPerMs());
//
//   progress_sampler.AddSample(520, 450);
//   // The first sample value was added at timeline 0 is now out of
//   // range (500ms) and thus discarded.
//   // Samples in queue now: [(20, 200), (200, 300), [520, 450)].
//   // Average speed: (520-20) / (450-200) = 2.
//   ASSERT1(2 == progress_sampler.GetAverageProgressPerMs());
template<typename T> class ProgressSampler {
 public:
  ProgressSampler(int sample_time_range_ms, int minimum_range_required_ms)
      : sample_time_range_ms_(sample_time_range_ms),
        minimum_range_required_ms_(minimum_range_required_ms) {
      ASSERT1(minimum_range_required_ms > 0);
  }

  void AddSampleWithCurrentTimeStamp(T sample_value) {
    AddSample(GetCurrent100NSTime() / kMillisecsTo100ns, sample_value);
  }

  void AddSample(uint64 timestamp_in_ms, T sample_value) {
    if (!samples_.empty() &&
        (sample_value < samples_.back().value ||          // Value regression.
         timestamp_in_ms < samples_.back().timestamp)) {  // Clock regression.
      Reset();
      return;
    }

    samples_.push(Sample(timestamp_in_ms, sample_value));

    // Discard old data that is out of range.
    while (samples_.back().timestamp - samples_.front().timestamp >
           sample_time_range_ms_ && samples_.size() > 2) {
      samples_.pop();
    }
  }

  bool HasEnoughSamples() const {
    if (samples_.size() < 2) {
      return false;
    }

    ASSERT1(samples_.back().timestamp >= samples_.front().timestamp);
    return (samples_.back().timestamp - samples_.front().timestamp >
            minimum_range_required_ms_);
  }

  T GetAverageProgressPerMs() const {
    if (!HasEnoughSamples()) {
      return kUnknownProgressPerMs;
    }

    uint64 time_diff = samples_.back().timestamp - samples_.front().timestamp;
    ASSERT1(time_diff > 0);
    return (samples_.back().value - samples_.front().value) /
            static_cast<T>(time_diff);
  }

  void Reset() {
    std::queue<Sample> empty_queue;
    std::swap(samples_, empty_queue);
  }

  static const T kUnknownProgressPerMs = static_cast<T>(-1);

 private:
  const uint64 sample_time_range_ms_;
  const uint64 minimum_range_required_ms_;

  struct Sample {
    Sample(uint64 local_timestamp, T local_value)
        : timestamp(local_timestamp), value(local_value) {
    }

    uint64  timestamp;
    T value;
  };
  std::queue<Sample> samples_;
};

}  // namespace omaha

#endif  // OMAHA_COMMON_PROGRESS_SAMPLER_H_
