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
//
// After http://src.chromium.org/viewvc/chrome/trunk/src/crypto/p224.cc
//
// This is an implementation of the P224 elliptic curve group. It's written to
// be short and simple rather than fast, although it's still constant-time.
//
// WARNING: Implementing these functions in a constant-time manner far from
//          obvious. Be careful when touching this code.
//          Consult ise-team@ when compelled to attempt changes.
//
// See http://www.imperialviolet.org/2010/12/04/ecc.html ([1]) for background.

#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "p224.h"

typedef int32_t s32;
typedef uint64_t u64;

// Field element functions.
//
// The field that we're dealing with is ℤ/pℤ where p = 2**224 - 2**96 + 1.
//
// Field elements are represented by a felem, which is a typedef to an
// array of 8 u32's. The value of a felem, a, is:
//   a[0] + 2**28·a[1] + 2**56·a[1] + ... + 2**196·a[7]
//
// Using 28-bit limbs means that there's only 4 bits of headroom, which is less
// than we would really like. But it has the useful feature that we hit 2**224
// exactly, making the reflections during a reduce much nicer.

typedef u32 limb;
#define NLIMBS 8
typedef limb felem[NLIMBS];

// kP is the P224 prime.
static const felem kP = {
  1, 0, 0, 268431360,
  268435455, 268435455, 268435455, 268435455,
};

// kB is parameter of the elliptic curve.
static const felem kB = {
  55967668, 11768882, 265861671, 185302395,
  39211076, 180311059, 84673715, 188764328,
};

// kBasep224_point is the base point (generator) of the elliptic curve group.
static const p224_point kBasep224_point = {
  {22813985, 52956513, 34677300, 203240812,
   12143107, 133374265, 225162431, 191946955},
  {83918388, 223877528, 122119236, 123340192,
   266784067, 263504429, 146143011, 198407736},
  {1, 0, 0, 0, 0, 0, 0, 0},
};

static void Contract(felem inout);

// IsZero returns 0xffffffff if a == 0 mod p and 0 otherwise.
static u32 IsZero(const felem a) {
  int i;

  felem minimal;
  memcpy(minimal, a, sizeof(minimal));
  Contract(minimal);

  u32 is_zero = 0, is_p = 0;
  for (i = 0; i < NLIMBS; i++) {
    is_zero |= minimal[i];
    is_p |= minimal[i] - kP[i];
  }

  // If either is_zero or is_p is 0, then we should return 1.
  is_zero |= is_zero >> 16;
  is_zero |= is_zero >> 8;
  is_zero |= is_zero >> 4;
  is_zero |= is_zero >> 2;
  is_zero |= is_zero >> 1;

  is_p |= is_p >> 16;
  is_p |= is_p >> 8;
  is_p |= is_p >> 4;
  is_p |= is_p >> 2;
  is_p |= is_p >> 1;

  // For is_zero and is_p, the LSB is 0 iff all the bits are zero.
  is_zero &= is_p & 1;
  is_zero = (~is_zero) << 31;
  is_zero = (s32)(is_zero) >> 31;
  return is_zero;
}

// Add computes *out = a+b
//
// a[i] + b[i] < 2**32
static void Add(felem out, const felem a, const felem b) {
  int i;
  for (i = 0; i < NLIMBS; i++) {
    out[i] = a[i] + b[i];
  }
}

#define kTwo31p3  (1u<<31) + (1u<<3)
#define kTwo31m3  (1u<<31) - (1u<<3)
#define kTwo31m15m3  (1u<<31) - (1u<<15) - (1u<<3)
// kZero31ModP is 0 mod p where bit 31 is set in all limbs so that we can
// subtract smaller amounts without underflow. See the section "Subtraction" in
// [1] for why.
static const felem kZero31ModP = {
  kTwo31p3, kTwo31m3, kTwo31m3, kTwo31m15m3,
  kTwo31m3, kTwo31m3, kTwo31m3, kTwo31m3
};

// Subtract computes *out = a-b
//
// a[i], b[i] < 2**30
// out[i] < 2**32
static void Subtract(felem out, const felem a, const felem b) {
  int i;
  for (i = 0; i < NLIMBS; i++) {
    // See the section on "Subtraction" in [1] for details.
    out[i] = a[i] + kZero31ModP[i] - b[i];
  }
}

#define kTwo63p35  (1ull<<63) + (1ull<<35)
#define kTwo63m35  (1ull<<63) - (1ull<<35)
#define kTwo63m35m19  (1ull<<63) - (1ull<<35) - (1ull<<19)
// kZero63ModP is 0 mod p where bit 63 is set in all limbs. See the section
// "Subtraction" in [1] for why.
static const u64 kZero63ModP[NLIMBS] = {
  kTwo63p35, kTwo63m35, kTwo63m35, kTwo63m35,
  kTwo63m35m19, kTwo63m35, kTwo63m35, kTwo63m35,
};

static const u32 kBottom28Bits = 0xfffffff;

// lfelem also represents an element of the field. The limbs are
// still spaced 28-bits apart and in little-endian order. So the limbs are at
// 0, 28, 56, ..., 392 bits, each 64-bits wide.
typedef u64 lfelem[15];

// ReduceLarge converts a lfelem to a felem.
//
// in[i] < 2**62
static void ReduceLarge(felem out, lfelem in) {
  int i;

  for (i = 0; i < NLIMBS; i += 4) {
    in[i+0] += kZero63ModP[i+0];
    in[i+1] += kZero63ModP[i+1];
    in[i+2] += kZero63ModP[i+2];
    in[i+3] += kZero63ModP[i+3];
  }

  // Eliminate the coefficients at 2**224 and greater while maintaining the
  // same value mod p.
  for (i = 14; i >= 8; i--) {
    in[i-8] -= in[i];  // reflection off the "+1" term of p.
    in[i-5] += (in[i] & 0xffff) << 12;  // part of the "-2**96" reflection.
    in[i-4] += in[i] >> 16;  // the rest of the "-2**96" reflection.
  }
  in[8] = 0;
  // in[0..8] < 2**64

  // As the values become small enough, we start to store them in |out| and use
  // 32-bit operations.
  for (i = 1; i < 8; i++) {
    in[i+1] += in[i] >> 28;
    out[i] = (u32)(in[i] & kBottom28Bits);
  }
  // Eliminate the term at 2*224 that we introduced while keeping the same
  // value mod p.
  in[0] -= in[8];  // reflection off the "+1" term of p.
  out[3] += (u32)(in[8] & 0xffff) << 12; // "-2**96" term
  out[4] += (u32)(in[8] >> 16);  // rest of "-2**96" term
  // in[0] < 2**64
  // out[3] < 2**29
  // out[4] < 2**29
  // out[1,2,5..7] < 2**28

  out[0] = (u32)(in[0] & kBottom28Bits);
  out[1] += (u32)((in[0] >> 28) & kBottom28Bits);
  out[2] += (u32)(in[0] >> 56);
  // out[0] < 2**28
  // out[1..4] < 2**29
  // out[5..7] < 2**28
}

// Mul computes *out = a*b
//
// a[i] < 2**29, b[i] < 2**30 (or vice versa)
// out[i] < 2**29
static void Mul(felem out, const felem a, const felem b) {
  int i, j;
  lfelem tmp = { 0 };

  for (i = 0; i < NLIMBS; i++) {
    for (j = 0; j < NLIMBS; j += 4) {
      tmp[i+j+0] += (u64)(a[i]) * (u64)(b[j+0]);
      tmp[i+j+1] += (u64)(a[i]) * (u64)(b[j+1]);
      tmp[i+j+2] += (u64)(a[i]) * (u64)(b[j+2]);
      tmp[i+j+3] += (u64)(a[i]) * (u64)(b[j+3]);
    }
  }

  ReduceLarge(out, tmp);
}

// Square computes *out = a*a
//
// a[i] < 2**29
// out[i] < 2**29
static void Square(felem out, const felem a) {
  int i, j;
  lfelem tmp = { 0 };

  tmp[0] = (u64)(a[0]) * (u64)(a[0]);

  for (i = 1; i < NLIMBS; i++) {
    tmp[i] += ((u64)(a[i]) * (u64)(a[0])) << 1;
    for (j = 1; j < i; j++) {
      u64 r = (u64)(a[i]) * (u64)(a[j]);
      tmp[i+j] += r << 1;
    }
    tmp[i+j] += (u64)(a[i]) * (u64)(a[j]);
  }

  ReduceLarge(out, tmp);
}

// Reduce reduces the coefficients of in_out to smaller bounds.
//
// On entry: a[i] < 2**31 + 2**30
// On exit: a[i] < 2**29
static void Reduce(felem a) {
  int i;

  for (i = 0; i < 7; i++) {
    a[i+1] += a[i] >> 28;
    a[i] &= kBottom28Bits;
  }
  u32 top = a[7] >> 28;
  a[7] &= kBottom28Bits;

  // top < 2**4
  // Constant-time: mask = (top != 0) ? 0xffffffff : 0
  u32 mask = top;
  mask |= mask >> 2;
  mask |= mask >> 1;
  mask <<= 31;
  mask = (u32)((s32)(mask) >> 31);

  // Eliminate top while maintaining the same value mod p.
  a[0] -= top;
  a[3] += top << 12;

  // We may have just made a[0] negative but, if we did, then we must
  // have added something to a[3], thus it's > 2**12. Therefore we can
  // carry down to a[0].
  a[3] -= 1 & mask;
  a[2] += mask & ((1<<28) - 1);
  a[1] += mask & ((1<<28) - 1);
  a[0] += mask & (1<<28);
}

// Invert calcuates *out = in**-1 by computing in**(2**224 - 2**96 - 1), i.e.
// Fermat's little theorem.
static void Invert(felem out, const felem in) {
  int i;
  felem f1, f2, f3, f4;

  Square(f1, in);                        // 2
  Mul(f1, f1, in);                       // 2**2 - 1
  Square(f1, f1);                        // 2**3 - 2
  Mul(f1, f1, in);                       // 2**3 - 1
  Square(f2, f1);                        // 2**4 - 2
  Square(f2, f2);                        // 2**5 - 4
  Square(f2, f2);                        // 2**6 - 8
  Mul(f1, f1, f2);                       // 2**6 - 1
  Square(f2, f1);                        // 2**7 - 2
  for (i = 0; i < 5; i++) {              // 2**12 - 2**6
    Square(f2, f2);
  }
  Mul(f2, f2, f1);                       // 2**12 - 1
  Square(f3, f2);                        // 2**13 - 2
  for (i = 0; i < 11; i++) {             // 2**24 - 2**12
    Square(f3, f3);
  }
  Mul(f2, f3, f2);                       // 2**24 - 1
  Square(f3, f2);                        // 2**25 - 2
  for (i = 0; i < 23; i++) {             // 2**48 - 2**24
    Square(f3, f3);
  }
  Mul(f3, f3, f2);                       // 2**48 - 1
  Square(f4, f3);                        // 2**49 - 2
  for (i = 0; i < 47; i++) {             // 2**96 - 2**48
    Square(f4, f4);
  }
  Mul(f3, f3, f4);                       // 2**96 - 1
  Square(f4, f3);                        // 2**97 - 2
  for (i = 0; i < 23; i++) {             // 2**120 - 2**24
    Square(f4, f4);
  }
  Mul(f2, f4, f2);                       // 2**120 - 1
  for (i = 0; i < 6; i++) {              // 2**126 - 2**6
    Square(f2, f2);
  }
  Mul(f1, f1, f2);                       // 2**126 - 1
  Square(f1, f1);                        // 2**127 - 2
  Mul(f1, f1, in);                       // 2**127 - 1
  for (i = 0; i < 97; i++) {             // 2**224 - 2**97
    Square(f1, f1);
  }
  Mul(out, f1, f3);                      // 2**224 - 2**96 - 1
}

// Contract converts a felem to its minimal, distinguished form.
//
// On entry, in[i] < 2**29
// On exit, in[i] < 2**28
static void Contract(felem out) {
  int i;

  // Reduce the coefficients to < 2**28.
  for (i = 0; i < 7; i++) {
    out[i+1] += out[i] >> 28;
    out[i] &= kBottom28Bits;
  }
  u32 top = out[7] >> 28;
  out[7] &= kBottom28Bits;

  // Eliminate top while maintaining the same value mod p.
  out[0] -= top;
  out[3] += top << 12;

  // We may just have made out[0] negative. So we carry down. If we made
  // out[0] negative then we know that out[3] is sufficiently positive
  // because we just added to it.
  for (i = 0; i < 3; i++) {
    u32 mask = (u32)((s32)(out[i]) >> 31);
    out[i] += (1 << 28) & mask;
    out[i+1] -= 1 & mask;
  }

  // We might have pushed out[3] over 2**28 so we perform another, partial
  // carry chain->
  for (i = 3; i < 7; i++) {
    out[i+1] += out[i] >> 28;
    out[i] &= kBottom28Bits;
  }
  top = out[7] >> 28;
  out[7] &= kBottom28Bits;

  // Eliminate top while maintaining the same value mod p.
  out[0] -= top;
  out[3] += top << 12;

  // There are two cases to consider for out[3]:
  //   1) The first time that we eliminated top, we didn't push out[3] over
  //      2**28. In this case, the partial carry chain didn't change any values
  //      and top is zero.
  //   2) We did push out[3] over 2**28 the first time that we eliminated top.
  //      The first value of top was in [0..16), therefore, prior to eliminating
  //      the first top, 0xfff1000 <= out[3] <= 0xfffffff. Therefore, after
  //      overflowing and being reduced by the second carry chain, out[3] <=
  //      0xf000. Thus it cannot have overflowed when we eliminated top for the
  //      second time.

  // Again, we may just have made out[0] negative, so do the same carry down.
  // As before, if we made out[0] negative then we know that out[3] is
  // sufficiently positive.
  for (i = 0; i < 3; i++) {
    u32 mask = (u32)((s32)(out[i]) >> 31);
    out[i] += (1 << 28) & mask;
    out[i+1] -= 1 & mask;
  }

  // The value is < 2**224, but maybe greater than p. In order to reduce to a
  // unique, minimal value we see if the value is >= p and, if so, subtract p.

  // First we build a mask from the top four limbs, which must all be
  // equal to bottom28Bits if the whole value is >= p. If top_4_all_ones
  // ends up with any zero bits in the bottom 28 bits, then this wasn't
  // true.
  u32 top_4_all_ones = 0xffffffffu;
  for (i = 4; i < 8; i++) {
    top_4_all_ones &= out[i];
  }
  top_4_all_ones |= 0xf0000000;
  // Now we replicate any zero bits to all the bits in top_4_all_ones.
  top_4_all_ones &= top_4_all_ones >> 16;
  top_4_all_ones &= top_4_all_ones >> 8;
  top_4_all_ones &= top_4_all_ones >> 4;
  top_4_all_ones &= top_4_all_ones >> 2;
  top_4_all_ones &= top_4_all_ones >> 1;
  top_4_all_ones =
      (u32)((s32)(top_4_all_ones << 31) >> 31);

  // Now we test whether the bottom three limbs are non-zero.
  u32 bottom_3_non_zero = out[0] | out[1] | out[2];
  bottom_3_non_zero |= bottom_3_non_zero >> 16;
  bottom_3_non_zero |= bottom_3_non_zero >> 8;
  bottom_3_non_zero |= bottom_3_non_zero >> 4;
  bottom_3_non_zero |= bottom_3_non_zero >> 2;
  bottom_3_non_zero |= bottom_3_non_zero >> 1;
  bottom_3_non_zero =
      (u32)((s32)(bottom_3_non_zero) >> 31);

  // Everything depends on the value of out[3].
  //    If it's > 0xffff000 and top_4_all_ones != 0 then the whole value is >= p
  //    If it's = 0xffff000 and top_4_all_ones != 0 and bottom_3_non_zero != 0,
  //      then the whole value is >= p
  //    If it's < 0xffff000, then the whole value is < p
  u32 n = out[3] - 0xffff000;
  u32 out_3_equal = n;
  out_3_equal |= out_3_equal >> 16;
  out_3_equal |= out_3_equal >> 8;
  out_3_equal |= out_3_equal >> 4;
  out_3_equal |= out_3_equal >> 2;
  out_3_equal |= out_3_equal >> 1;
  out_3_equal =
      ~(u32)((s32)(out_3_equal << 31) >> 31);

  // If out[3] > 0xffff000 then n's MSB will be zero.
  u32 out_3_gt = ~(u32)((s32)(n << 31) >> 31);

  u32 mask = top_4_all_ones & ((out_3_equal & bottom_3_non_zero) | out_3_gt);
  out[0] -= 1 & mask;
  out[3] -= 0xffff000 & mask;
  out[4] -= 0xfffffff & mask;
  out[5] -= 0xfffffff & mask;
  out[6] -= 0xfffffff & mask;
  out[7] -= 0xfffffff & mask;
}


// Group element functions.
//
// These functions deal with group elements. The group is an elliptic curve
// group with a = -3 defined in FIPS 186-3, section D.2.2.

static void CopyConditional(p224_point* out, const p224_point* a, u32 mask);
static void DoubleJacobian(p224_point* out, const p224_point* a);

// AddJacobian computes *out = a+b where a != b->
static void AddJacobian(p224_point *out,
                        const p224_point* a,
                        const p224_point* b) {
  // See http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#addition-add-2007-bl
  int n;
  felem z1z1, z2z2, u1, u2, s1, s2, h, i, j, r, v;

  u32 z1_is_zero = IsZero(a->z);
  u32 z2_is_zero = IsZero(b->z);

  // Z1Z1 = Z1²
  Square(z1z1, a->z);

  // Z2Z2 = Z2²
  Square(z2z2, b->z);

  // U1 = X1*Z2Z2
  Mul(u1, a->x, z2z2);

  // U2 = X2*Z1Z1
  Mul(u2, b->x, z1z1);

  // S1 = Y1*Z2*Z2Z2
  Mul(s1, b->z, z2z2);
  Mul(s1, a->y, s1);

  // S2 = Y2*Z1*Z1Z1
  Mul(s2, a->z, z1z1);
  Mul(s2, b->y, s2);

  // H = U2-U1
  Subtract(h, u2, u1);
  Reduce(h);
  u32 x_equal = IsZero(h);

  // I = (2*H)²
  for (n = 0; n < NLIMBS; n++) {
    i[n] = h[n] << 1;
  }
  Reduce(i);
  Square(i, i);

  // J = H*I
  Mul(j, h, i);
  // r = 2*(S2-S1)
  Subtract(r, s2, s1);
  Reduce(r);

  u32 y_equal = IsZero(r);

  // All variables we test here are output from IsZero(), thus either 0 or -1.
  // We use a fixed timing evaluation of the total expression.
  // The if() body never gets executed during private scalar multiplies,
  // (e.g. ScalarMult()) but might trigger during public ecdsa verify computation.
  if (x_equal & y_equal & ~z1_is_zero & ~z2_is_zero) {
    // The two input points are the same (and finite),
    // therefore we must use the dedicated doubling function
    // as the slope of the line is undefined.
    DoubleJacobian(out, a);
    return;
  }

  for (n = 0; n < NLIMBS; n++) {
    r[n] <<= 1;
  }
  Reduce(r);

  // V = U1*I
  Mul(v, u1, i);

  // Z3 = ((Z1+Z2)²-Z1Z1-Z2Z2)*H
  Add(z1z1, z1z1, z2z2);
  Add(z2z2, a->z, b->z);
  Reduce(z2z2);
  Square(z2z2, z2z2);
  Subtract(out->z, z2z2, z1z1);
  Reduce(out->z);
  Mul(out->z, out->z, h);

  // X3 = r²-J-2*V
  for (n = 0; n < NLIMBS; n++) {
    z1z1[n] = v[n] << 1;
  }
  Add(z1z1, j, z1z1);
  Reduce(z1z1);
  Square(out->x, r);
  Subtract(out->x, out->x, z1z1);
  Reduce(out->x);

  // Y3 = r*(V-X3)-2*S1*J
  for (n = 0; n < NLIMBS; n++) {
    s1[n] <<= 1;
  }
  Mul(s1, s1, j);
  Subtract(z1z1, v, out->x);
  Reduce(z1z1);
  Mul(z1z1, z1z1, r);
  Subtract(out->y, z1z1, s1);
  Reduce(out->y);

  CopyConditional(out, a, z2_is_zero);
  CopyConditional(out, b, z1_is_zero);
}

// DoubleJacobian computes *out = a+a->
static void DoubleJacobian(p224_point* out, const p224_point* a) {
  // See http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#doubling-dbl-2001-b
  int i;
  felem delta, gamma, beta, alpha, t;

  Square(delta, a->z);
  Square(gamma, a->y);
  Mul(beta, a->x, gamma);

  // alpha = 3*(X1-delta)*(X1+delta)
  Add(t, a->x, delta);
  for (i = 0; i < NLIMBS; i++) {
    t[i] += t[i] << 1;
  }
  Reduce(t);
  Subtract(alpha, a->x, delta);
  Reduce(alpha);
  Mul(alpha, alpha, t);

  // Z3 = (Y1+Z1)²-gamma-delta
  Add(out->z, a->y, a->z);
  Reduce(out->z);
  Square(out->z, out->z);
  Subtract(out->z, out->z, gamma);
  Reduce(out->z);
  Subtract(out->z, out->z, delta);
  Reduce(out->z);

  // X3 = alpha²-8*beta
  for (i = 0; i < NLIMBS; i++) {
    delta[i] = beta[i] << 3;
  }
  Reduce(delta);
  Square(out->x, alpha);
  Subtract(out->x, out->x, delta);
  Reduce(out->x);

  // Y3 = alpha*(4*beta-X3)-8*gamma²
  for (i = 0; i < NLIMBS; i++) {
    beta[i] <<= 2;
  }
  Reduce(beta);
  Subtract(beta, beta, out->x);
  Reduce(beta);
  Square(gamma, gamma);
  for (i = 0; i < NLIMBS; i++) {
    gamma[i] <<= 3;
  }
  Reduce(gamma);
  Mul(out->y, alpha, beta);
  Subtract(out->y, out->y, gamma);
  Reduce(out->y);
}

// CopyConditional sets *out=a if mask is 0xffffffff. mask must be either 0 or
// 0xffffffff.
static void CopyConditional(p224_point* out,
                            const p224_point* a,
                            u32 mask) {
  int i;
  for (i = 0; i < NLIMBS; i++) {
    out->x[i] ^= mask & (a->x[i] ^ out->x[i]);
    out->y[i] ^= mask & (a->y[i] ^ out->y[i]);
    out->z[i] ^= mask & (a->z[i] ^ out->z[i]);
  }
}

// NON_ZERO_TO_ALL_ONES returns:
//   0xffffffff for 0 < x <= 2**31
//   0 for x == 0 or x > 2**31.
//
// This macro assumes that right-shifting a signed number shifts in the MSB on
// the left. This is not ensured by the C standard, but is true on the CPUs
// that we're targetting with this code (x86 and ARM).
#define NON_ZERO_TO_ALL_ONES(x) (~((u32) (((s32) ((x)-1)) >> 31)))

// Select table entry at index.
// Access all entries in table in a fixed-timing manner.
static void SelectJacobian(const p224_point table[16],
                           u32 index,
                           p224_point* out) {
  int i;
  memset(out, 0, sizeof(*out));
  for (i = 1; i < 16; ++i) {
    CopyConditional(out, &table[i], ~NON_ZERO_TO_ALL_ONES(i ^ index));
  }
}

// ScalarMult calculates *out = a*scalar where scalar is a big-endian number of
// length scalar_len and != 0.
static void ScalarMult(p224_point* out,
                       const p224_point* a,
                       const u8* scalar,
                       int scalar_len) {
  p224_point tbl[16], tmp;
  u32 out_is_infinity_mask, tmp_is_noninfinite_mask, mask;
  int i;

  memset(out, 0, sizeof(*out));
  tbl[0] = *out;
  tbl[1] = *a;
  for (i = 2; i < 16; i += 2) {
    DoubleJacobian(&tbl[i], &tbl[i / 2]);
    AddJacobian(&tbl[i + 1], &tbl[i], a);
  }
  out_is_infinity_mask = -1;

  for (i = 0; i < scalar_len; i++) {
    u32 idx;

    if (i) {
      DoubleJacobian(out, out);
      DoubleJacobian(out, out);
      DoubleJacobian(out, out);
      DoubleJacobian(out, out);
    }

    idx = (scalar[i] >> 4) & 15;
    SelectJacobian(tbl, idx, &tmp);
    AddJacobian(&tbl[0], &tmp, out);
    CopyConditional(out, &tmp, out_is_infinity_mask);

    tmp_is_noninfinite_mask = NON_ZERO_TO_ALL_ONES(idx);
    mask = tmp_is_noninfinite_mask & ~out_is_infinity_mask;

    CopyConditional(out, &tbl[0], mask);

    out_is_infinity_mask &= ~tmp_is_noninfinite_mask;

    DoubleJacobian(out, out);
    DoubleJacobian(out, out);
    DoubleJacobian(out, out);
    DoubleJacobian(out, out);

    idx = scalar[i] & 15;
    SelectJacobian(tbl, idx, &tmp);
    AddJacobian(&tbl[0], &tmp, out);
    CopyConditional(out, &tmp, out_is_infinity_mask);

    tmp_is_noninfinite_mask = NON_ZERO_TO_ALL_ONES(idx);
    mask = tmp_is_noninfinite_mask & ~out_is_infinity_mask;

    CopyConditional(out, &tbl[0], mask);

    out_is_infinity_mask &= ~tmp_is_noninfinite_mask;
  }
}

static u32 u32_from_bin(const u8* v) {
  return (v[0] << 24) |
         (v[1] << 16) |
         (v[2] << 8) |
         v[3];
}

static void u32_to_bin(u8* p, u32 v) {
  p[0] = v >> 24;
  p[1] = v >> 16;
  p[2] = v >> 8;
  p[3] = v;
}

// Get224Bits reads 7 words from in and scatters their contents in
// little-endian form into 8 words at out, 28 bits per output word.
static void Get224Bits(felem out, const u8* in) {
  out[0] =   u32_from_bin(&in[4*6]) & kBottom28Bits;
  out[1] = ((u32_from_bin(&in[4*5]) << 4) |
            (u32_from_bin(&in[4*6]) >> 28)) & kBottom28Bits;
  out[2] = ((u32_from_bin(&in[4*4]) << 8) |
            (u32_from_bin(&in[4*5]) >> 24)) & kBottom28Bits;
  out[3] = ((u32_from_bin(&in[4*3]) << 12) |
            (u32_from_bin(&in[4*4]) >> 20)) & kBottom28Bits;
  out[4] = ((u32_from_bin(&in[4*2]) << 16) |
            (u32_from_bin(&in[4*3]) >> 16)) & kBottom28Bits;
  out[5] = ((u32_from_bin(&in[4*1]) << 20) |
            (u32_from_bin(&in[4*2]) >> 12)) & kBottom28Bits;
  out[6] = ((u32_from_bin(&in[4*0]) << 24) |
            (u32_from_bin(&in[4*1]) >> 8)) & kBottom28Bits;
  out[7] =  (u32_from_bin(&in[4*0]) >> 4) & kBottom28Bits;
}

// Put224Bits performs the inverse operation to Get224Bits: taking 28 bits from
// each of 8 input words and writing them in big-endian order to 7 words at
// out.
static void Put224Bits(u8* out, const felem in) {
  u32_to_bin(&out[4*6], (in[0] >> 0) | (in[1] << 28));
  u32_to_bin(&out[4*5], (in[1] >> 4) | (in[2] << 24));
  u32_to_bin(&out[4*4], (in[2] >> 8) | (in[3] << 20));
  u32_to_bin(&out[4*3], (in[3] >> 12) | (in[4] << 16));
  u32_to_bin(&out[4*2], (in[4] >> 16) | (in[5] << 12));
  u32_to_bin(&out[4*1], (in[5] >> 20) | (in[6] << 8));
  u32_to_bin(&out[4*0], (in[6] >> 24) | (in[7] << 4));
}

// Public API functions.

int p224_point_from_bin(const void* in, int size, p224_point* out) {
  int i;
  if (size != 56) return 0;

  const u8* inbytes = (const u8*)(in);
  Get224Bits(out->x, inbytes);
  Get224Bits(out->y, inbytes + 28);

  memset(out->z, 0, sizeof(out->z));
  out->z[0] = 1;

  // Check that the point is on the curve, i.e. that y² = x³ - 3x + b
  felem lhs;
  Square(lhs, out->y);
  Contract(lhs);

  felem rhs;
  Square(rhs, out->x);
  Mul(rhs, out->x, rhs);

  felem three_x;
  for (i = 0; i < 8; i++) {
    three_x[i] = out->x[i] * 3;
  }
  Reduce(three_x);
  Subtract(rhs, rhs, three_x);
  Reduce(rhs);

  Add(rhs, rhs, kB);
  Contract(rhs);
  return memcmp(lhs, rhs, sizeof(lhs)) == 0;
}

void p224_point_to_bin(const p224_point* in, void* out) {
  felem zinv, zinv_sq, x, y;

  // If this is the point at infinity we return a string of all zeros.
  if (IsZero(in->z)) {
    memset(out, 0, 56);
    return;
  }

  Invert(zinv, in->z);
  Square(zinv_sq, zinv);
  Mul(x, in->x, zinv_sq);
  Mul(zinv_sq, zinv_sq, zinv);
  Mul(y, in->y, zinv_sq);

  Contract(x);
  Contract(y);

  u8* outbytes = (u8*)out;
  Put224Bits(outbytes, x);
  Put224Bits(outbytes + 28, y);
}

void p224_point_mul(const p224_point* in,
                    const u8 scalar[28],
                    p224_point* out) {
  ScalarMult(out, in, scalar, 28);
}

void p224_base_point_mul(const u8 scalar[28], p224_point* out) {
  ScalarMult(out, &kBasep224_point, scalar, 28);
}

void p224_point_add(const p224_point* a, const p224_point* b, p224_point* out) {
  AddJacobian(out, a, b);
}

void p224_point_negate(const p224_point* in, p224_point* out) {
  // Guide to elliptic curve cryptography, page 89 suggests that (X : X+Y : Z)
  // is the negative in Jacobian coordinates, but it doesn't actually appear to
  // be true in testing so this performs the negation in affine coordinates.
  felem zinv, zinv_sq, y;
  Invert(zinv, in->z);
  Square(zinv_sq, zinv);
  Mul(out->x, in->x, zinv_sq);
  Mul(zinv_sq, zinv_sq, zinv);
  Mul(y, in->y, zinv_sq);

  Subtract(out->y, kP, y);
  Reduce(out->y);

  memset(out->z, 0, sizeof(out->z));
  out->z[0] = 1;
}
