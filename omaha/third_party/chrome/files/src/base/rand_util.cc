// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <windows.h>

// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

#include <algorithm>
#include <limits>

#include "omaha/base/debug.h"

namespace omaha {

bool RandBytes(void* output, size_t output_length) {
  ASSERT1(output);

  char* output_ptr = static_cast<char*>(output);
  while (output_length > 0) {
    const ULONG output_bytes_this_pass = static_cast<ULONG>(std::min(
        output_length, static_cast<size_t>(std::numeric_limits<ULONG>::max())));
    const bool success =
        RtlGenRandom(output_ptr, output_bytes_this_pass) != FALSE;
    if (!success) {
      return false;
    }

    output_length -= output_bytes_this_pass;
    output_ptr += output_bytes_this_pass;
  }

  return true;
}

bool RandUint32(uint32_t* number) {
  ASSERT1(number);

  return RandBytes(number, sizeof(*number));
}

}  // namespace omaha

