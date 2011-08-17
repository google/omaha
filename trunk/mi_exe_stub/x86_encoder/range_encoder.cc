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

#include "omaha/mi_exe_stub/x86_encoder/range_encoder.h"

#include "base/basictypes.h"

namespace omaha {

RangeEncoder::RangeEncoder(std::string* output)
    : output_(output),
      cache_(0),
      cache_size_(1),
      low_(0),
      range_(0xFFFFFFFF) {
}

void RangeEncoder::Encode(uint32 start, uint32 size, uint32 total) {
  low_ += start * (range_ /= total);
  range_ *= size;
  while (range_ < kTopValue) {
    range_ <<= 8;
    ShiftLow();
  }
}

void RangeEncoder::ShiftLow() {
  if (static_cast<uint32>(low_) < static_cast<uint32>(0xFF000000) ||
      static_cast<int>(low_ >> 32) != 0) {
    uint8 temp = cache_;
    do {
      *output_ += static_cast<uint8>(temp + static_cast<uint8>(low_ >> 32));
      temp = 0xFF;
    } while (--cache_size_ != 0);
    cache_ = static_cast<uint8>(static_cast<uint32>(low_) >> 24);
  }
  cache_size_++;
  low_ = static_cast<uint32>(low_) << 8;
}

void RangeEncoder::Flush() {
  for (int i = 0; i < 5; ++i) {
    ShiftLow();
  }
}

}  // namespace
