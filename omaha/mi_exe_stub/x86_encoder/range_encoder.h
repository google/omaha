// Copyright 2009 Google Inc.
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
// Derived from RangeCoder.h and RangeCoderBit.h in the LZMA SDK
//
// A description of how range encoding works can be found at
// http://en.wikipedia.org/wiki/Range_encoding

#ifndef OMAHA_MI_EXE_STUB_X86_ENCODER_RANGE_ENCODER_H_
#define OMAHA_MI_EXE_STUB_X86_ENCODER_RANGE_ENCODER_H_

#include <string>

#include "base/basictypes.h"

namespace omaha {

const int kNumberOfBitsModelTotalBits = 11;
const uint32 kBitModelTotal = 1 << kNumberOfBitsModelTotalBits;

const int kNumTopBits = 24;
const uint32 kTopValue = 1 << kNumTopBits;

class RangeEncoder {
 public:
  // RangeEncoder does not take ownership. Lifetime must be managed by caller.
  explicit RangeEncoder(std::string* output);
  void Encode(uint32 start, uint32 size, uint32 total);
  void ShiftLow();
  void Flush();

  uint64 low() const { return low_; }
  void set_low(uint64 low) { low_ = low; }
  uint32 range() const { return range_; }
  void set_range(uint32 range) { range_ = range; }

 private:
  uint8 cache_;
  uint32 cache_size_;

  uint64 low_;
  uint32 range_;

  std::string* output_;

  DISALLOW_COPY_AND_ASSIGN(RangeEncoder);
};

template<int kNumberOfMoveBits>
class RangeEncoderBit {
 public:
  RangeEncoderBit() : probability_(kBitModelTotal / 2) {
  }

  void Encode(uint32 symbol, RangeEncoder* encoder) {
    uint32 new_bound = (encoder->range() >> kNumberOfBitsModelTotalBits) *
                       probability_;
    if (0 == symbol) {
      encoder->set_range(new_bound);
      probability_ += (kBitModelTotal - probability_) >> kNumberOfMoveBits;
    } else {
      encoder->set_low(encoder->low() + new_bound);
      encoder->set_range(encoder->range() - new_bound);
      probability_ -= probability_ >> kNumberOfMoveBits;
    }
    if (encoder->range() < kTopValue) {
      encoder->set_range(encoder->range() << 8);
      encoder->ShiftLow();
    }
  }

 private:
  uint32 probability_;

  DISALLOW_COPY_AND_ASSIGN(RangeEncoderBit);
};

}  // namespace omaha

#endif  // OMAHA_MI_EXE_STUB_X86_ENCODER_RANGE_ENCODER_H_
