// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_H_
#define BASE_RAND_UTIL_H_

#include <stddef.h>
#include <stdint.h>

namespace omaha {

// Fills |output_length| bytes of |output| with random data.
bool RandBytes(void* output, size_t output_length);

// Returns a random number in range [0, kuint32max]. Thread-safe.
bool RandUint32(uint32_t* number);

}  // namespace omaha

#endif  // BASE_RAND_UTIL_H_

