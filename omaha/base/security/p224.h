// Copyright 2014 Google Inc.
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

#ifndef OMAHA_BASE_SECURITY_P224_H_
#define OMAHA_BASE_SECURITY_P224_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;
typedef uint32_t u32;

typedef struct {
  u32 x[8], y[8], z[8];
} p224_point;

// Parse and check validity of a point from binary input.
// Input size should be 2 * 28 (56) bytes.
// Returns 0 on failure.
int p224_point_from_bin(const void* input, int size, p224_point* out);

// Render a point to binary output.
// output should be able to hold 2 * 28 (56) bytes.
void p224_point_to_bin(const p224_point* in, void* output);

// Multiply the curve generator by scalar.
void p224_base_point_mul(const u8 scalar[28], p224_point* out);

// Multiply input point by scalar.
// out cannot point to input.
void p224_point_mul(const p224_point* input,
                    const u8 scalar[28],
                    p224_point* out);

// Add two points.
// out cannot point to a or b.
void p224_point_add(const p224_point* a, const p224_point* b, p224_point* out);

// Negate point.
// out cannot point to in.
void p224_point_negate(const p224_point* in, p224_point* out);

#ifdef __cplusplus
}
#endif

#endif  // OMAHA_BASE_SECURITY_P224_H_
