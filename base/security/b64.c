// Copyright 2007-2009 Google Inc.
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

#include "b64.h"

static const char b64outmap[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'
};

static const char b64inmap[96] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 63, 0, 0,
  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 0, 0, 0, 0, 0, 0,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 0, 64,
  0, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 0, 0, 0, 0, 0,
};

int B64_encode(const uint8_t* input,
               int input_length,
               char* output,
               int output_max) {
  unsigned int accu = 0;
  int shift = 0;
  int output_size = 0;

  while (input_length--) {
    accu <<= 8;
    accu |= *input++;
    shift += 8;

    while (shift >= 6) {
      if (output_size >= output_max) return -1;  // out of output space
      output[output_size++] = b64outmap[(accu >> (shift - 6)) & 63];
      shift -= 6;
    }
  }
  if (shift) {
    if (output_size >= output_max) return -1;  // out of output space
    accu <<= 8;  // pad with 0 byte really
    shift += 8;
    output[output_size++] = b64outmap[(accu >> (shift - 6)) & 63];
  }

  // Output terminating 0
  if (output_size >= output_max) return -1;  // out of output space
  output[output_size] = '\0';

  return output_size;
}

int B64_decode(const char* input,
               uint8_t* output,
               int output_max) {
  unsigned int accu = 0;
  int shift = 0;
  int output_size = 0;

  while (*input) {
    unsigned char in = *input++ & 255;
    if (in < 32 || in > 127 || !b64inmap[in - 32]) return -1;  // invalid input
    accu <<= 6;
    accu |= (b64inmap[in - 32] - 1);
    shift += 6;
    if (shift >= 8) {
      if (output_size >= output_max) return -1;  // out of output space
      output[output_size++] = (accu >> (shift - 8)) & 255;
      shift -= 8;
    }
  }
  return output_size;
}
