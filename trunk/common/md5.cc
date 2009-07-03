// Copyright 2004-2009 Google Inc.
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

/*
 * md5_opt.c V1.0 - optimized md5c.c from RFC1321 reference implementation
 *
 * Copyright (c) 1995 University of Southern California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, and other materials related to such distribution and use
 * acknowledge that the software was developed by the University of
 * Southern California, Information Sciences Institute.  The name of the
 * University may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 J. Touch / touch@isi.edu
 5/1/95

*/
/* MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
*/

#include "md5.h"
#include "common/debug.h"

namespace omaha {

#if (defined(i386) || defined (__i386__) || defined (_M_IX86) || defined(__alpha))  // little-endian
#undef REORDER
#else
#define REORDER 1
#endif

// Constants for MD5Transform routine
#define kS11 7
#define kS12 12
#define kS13 17
#define kS14 22
#define kS21 5
#define kS22 9
#define kS23 14
#define kS24 20
#define kS31 4
#define kS32 11
#define kS33 16
#define kS34 23
#define kS41 6
#define kS42 10
#define kS43 15
#define kS44 21

static void MD5Transform (uint32 [4], unsigned char [64]);
static void Encode (unsigned char *, uint32 *, unsigned int);
#ifdef REORDER
static void Decode (uint32 *, unsigned char *, unsigned int);
#endif

// SELECTANY static unsigned char PADDING[64] = {
static unsigned char PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// F, G, H and I are basic MD5 functions.
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

// ROTATE_LEFT rotates x left n bits.
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
// Rotation is separate from addition to prevent recomputation.
#define FF(a, b, c, d, x, s, ac) { \
    (a) += F ((b), (c), (d)) + (x) + (uint32)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
    }
#define GG(a, b, c, d, x, s, ac) { \
    (a) += G ((b), (c), (d)) + (x) + (uint32)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
    }
#define HH(a, b, c, d, x, s, ac) { \
    (a) += H ((b), (c), (d)) + (x) + (uint32)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
    }
#define II(a, b, c, d, x, s, ac) { \
    (a) += I ((b), (c), (d)) + (x) + (uint32)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
    }

// MD5 initialization. Begins an MD5 operation, writing a new context.
void MD5Init (MD5_CTX *context) {
    ASSERT(context, (L""));

    context->count[0] = context->count[1] = 0;
    // Load magic initialization constants.
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

// MD5 block update operation. Continues an MD5 message-digest
// operation, processing another message block, and updating the context.
void MD5Update (MD5_CTX *context, unsigned char *input, unsigned int inputLen) {
    ASSERT(input, (L""));
    ASSERT(context, (L""));

    unsigned int i, index, partLen;

    // Compute number of bytes mod 64
    index = (unsigned int)((context->count[0] >> 3) & 0x3F);

    // Update number of bits
    if ((context->count[0] += ((uint32)inputLen << 3)) < ((uint32)inputLen << 3))
        context->count[1]++;

    context->count[1] += ((uint32)inputLen >> 29);
    partLen = 64 - index;

    // Transform as many times as possible
    if (inputLen >= partLen) {
       memcpy ((POINTER)&context->buffer[index], (POINTER)input, partLen);
       MD5Transform (context->state, context->buffer);

       for (i = partLen; i + 63 < inputLen; i += 64) MD5Transform (context->state, &input[i]);
       index = 0;
       }
    else
       i = 0;

    // Buffer remaining input
    memcpy ((POINTER)&context->buffer[index], (POINTER)&input[i], inputLen-i);
}

// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
void MD5Final (unsigned char digest[16], MD5_CTX *context) {
    ASSERT(context, (L""));

    unsigned char bits[8];
    unsigned int index, padLen;

    // Save number of bits
    Encode (bits, context->count, 8);

    // Pad out to 56 mod 64.
    index = (unsigned int)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    MD5Update (context, PADDING, padLen);

    // Append length (before padding)
    MD5Update (context, bits, 8);
    // Store state in digest
    Encode (digest, context->state, 16);

    // Zeroize sensitive information.
    memset ((POINTER)context, 0, sizeof (*context));
}

// MD5 basic transformation. Transforms state based on block.
void MD5Transform (uint32 state[4], unsigned char block[64]) {
    // USC/ISI J. Touch - encourage pushing state variables into registers
    register uint32 a = state[0], b = state[1], c = state[2], d = state[3];

    /* USC/ISI J. Touch
    decode and using copied data vs. direct use of input buffer
    depends on whether reordering is required, which is a combination
    of the byte order of the architecture and the command-line option
    override
    */

#ifdef REORDER
    uint32 x[16];

    Decode (x, block, 64);

    /* Round 1 */
    FF (a, b, c, d, x[ 0], kS11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, x[ 1], kS12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, x[ 2], kS13, 0x242070db); /* 3 */
    FF (b, c, d, a, x[ 3], kS14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, x[ 4], kS11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, x[ 5], kS12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, x[ 6], kS13, 0xa8304613); /* 7 */
    FF (b, c, d, a, x[ 7], kS14, 0xfd469501); /* 8 */
    FF (a, b, c, d, x[ 8], kS11, 0x698098d8); /* 9 */
    FF (d, a, b, c, x[ 9], kS12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, x[10], kS13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, x[11], kS14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, x[12], kS11, 0x6b901122); /* 13 */
    FF (d, a, b, c, x[13], kS12, 0xfd987193); /* 14 */
    FF (c, d, a, b, x[14], kS13, 0xa679438e); /* 15 */
    FF (b, c, d, a, x[15], kS14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG (a, b, c, d, x[ 1], kS21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, x[ 6], kS22, 0xc040b340); /* 18 */
    GG (c, d, a, b, x[11], kS23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, x[ 0], kS24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, x[ 5], kS21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, x[10], kS22,  0x2441453); /* 22 */
    GG (c, d, a, b, x[15], kS23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, x[ 4], kS24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, x[ 9], kS21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, x[14], kS22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, x[ 3], kS23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, x[ 8], kS24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, x[13], kS21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, x[ 2], kS22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, x[ 7], kS23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, x[12], kS24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, x[ 5], kS31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, x[ 8], kS32, 0x8771f681); /* 34 */
    HH (c, d, a, b, x[11], kS33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, x[14], kS34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, x[ 1], kS31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, x[ 4], kS32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, x[ 7], kS33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, x[10], kS34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, x[13], kS31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, x[ 0], kS32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, x[ 3], kS33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, x[ 6], kS34,  0x4881d05); /* 44 */
    HH (a, b, c, d, x[ 9], kS31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, x[12], kS32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, x[15], kS33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, x[ 2], kS34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, x[ 0], kS41, 0xf4292244); /* 49 */
    II (d, a, b, c, x[ 7], kS42, 0x432aff97); /* 50 */
    II (c, d, a, b, x[14], kS43, 0xab9423a7); /* 51 */
    II (b, c, d, a, x[ 5], kS44, 0xfc93a039); /* 52 */
    II (a, b, c, d, x[12], kS41, 0x655b59c3); /* 53 */
    II (d, a, b, c, x[ 3], kS42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, x[10], kS43, 0xffeff47d); /* 55 */
    II (b, c, d, a, x[ 1], kS44, 0x85845dd1); /* 56 */
    II (a, b, c, d, x[ 8], kS41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, x[15], kS42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, x[ 6], kS43, 0xa3014314); /* 59 */
    II (b, c, d, a, x[13], kS44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, x[ 4], kS41, 0xf7537e82); /* 61 */
    II (d, a, b, c, x[11], kS42, 0xbd3af235); /* 62 */
    II (c, d, a, b, x[ 2], kS43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, x[ 9], kS44, 0xeb86d391); /* 64 */

#else
    // USC/ISI J. Touch
    // omit reordering, and use the input block as source data
    /* Round 1 */
    FF (a, b, c, d, ((uint32 *)block)[ 0], kS11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, ((uint32 *)block)[ 1], kS12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, ((uint32 *)block)[ 2], kS13, 0x242070db); /* 3 */
    FF (b, c, d, a, ((uint32 *)block)[ 3], kS14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, ((uint32 *)block)[ 4], kS11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, ((uint32 *)block)[ 5], kS12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, ((uint32 *)block)[ 6], kS13, 0xa8304613); /* 7 */
    FF (b, c, d, a, ((uint32 *)block)[ 7], kS14, 0xfd469501); /* 8 */
    FF (a, b, c, d, ((uint32 *)block)[ 8], kS11, 0x698098d8); /* 9 */
    FF (d, a, b, c, ((uint32 *)block)[ 9], kS12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, ((uint32 *)block)[10], kS13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, ((uint32 *)block)[11], kS14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, ((uint32 *)block)[12], kS11, 0x6b901122); /* 13 */
    FF (d, a, b, c, ((uint32 *)block)[13], kS12, 0xfd987193); /* 14 */
    FF (c, d, a, b, ((uint32 *)block)[14], kS13, 0xa679438e); /* 15 */
    FF (b, c, d, a, ((uint32 *)block)[15], kS14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG (a, b, c, d, ((uint32 *)block)[ 1], kS21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, ((uint32 *)block)[ 6], kS22, 0xc040b340); /* 18 */
    GG (c, d, a, b, ((uint32 *)block)[11], kS23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, ((uint32 *)block)[ 0], kS24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, ((uint32 *)block)[ 5], kS21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, ((uint32 *)block)[10], kS22,  0x2441453); /* 22 */
    GG (c, d, a, b, ((uint32 *)block)[15], kS23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, ((uint32 *)block)[ 4], kS24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, ((uint32 *)block)[ 9], kS21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, ((uint32 *)block)[14], kS22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, ((uint32 *)block)[ 3], kS23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, ((uint32 *)block)[ 8], kS24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, ((uint32 *)block)[13], kS21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, ((uint32 *)block)[ 2], kS22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, ((uint32 *)block)[ 7], kS23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, ((uint32 *)block)[12], kS24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, ((uint32 *)block)[ 5], kS31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, ((uint32 *)block)[ 8], kS32, 0x8771f681); /* 34 */
    HH (c, d, a, b, ((uint32 *)block)[11], kS33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, ((uint32 *)block)[14], kS34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, ((uint32 *)block)[ 1], kS31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, ((uint32 *)block)[ 4], kS32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, ((uint32 *)block)[ 7], kS33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, ((uint32 *)block)[10], kS34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, ((uint32 *)block)[13], kS31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, ((uint32 *)block)[ 0], kS32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, ((uint32 *)block)[ 3], kS33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, ((uint32 *)block)[ 6], kS34,  0x4881d05); /* 44 */
    HH (a, b, c, d, ((uint32 *)block)[ 9], kS31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, ((uint32 *)block)[12], kS32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, ((uint32 *)block)[15], kS33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, ((uint32 *)block)[ 2], kS34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, ((uint32 *)block)[ 0], kS41, 0xf4292244); /* 49 */
    II (d, a, b, c, ((uint32 *)block)[ 7], kS42, 0x432aff97); /* 50 */
    II (c, d, a, b, ((uint32 *)block)[14], kS43, 0xab9423a7); /* 51 */
    II (b, c, d, a, ((uint32 *)block)[ 5], kS44, 0xfc93a039); /* 52 */
    II (a, b, c, d, ((uint32 *)block)[12], kS41, 0x655b59c3); /* 53 */
    II (d, a, b, c, ((uint32 *)block)[ 3], kS42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, ((uint32 *)block)[10], kS43, 0xffeff47d); /* 55 */
    II (b, c, d, a, ((uint32 *)block)[ 1], kS44, 0x85845dd1); /* 56 */
    II (a, b, c, d, ((uint32 *)block)[ 8], kS41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, ((uint32 *)block)[15], kS42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, ((uint32 *)block)[ 6], kS43, 0xa3014314); /* 59 */
    II (b, c, d, a, ((uint32 *)block)[13], kS44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, ((uint32 *)block)[ 4], kS41, 0xf7537e82); /* 61 */
    II (d, a, b, c, ((uint32 *)block)[11], kS42, 0xbd3af235); /* 62 */
    II (c, d, a, b, ((uint32 *)block)[ 2], kS43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, ((uint32 *)block)[ 9], kS44, 0xeb86d391); /* 64 */
#endif  // REORDER

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

#ifdef REORDER
    memset ((POINTER)x, 0, sizeof (x));  // zero sensitive information
#endif
}

// Encodes input (uint32) into output (unsigned char). Assumes len is a multiple of 4.
void Encode (unsigned char *output, uint32 *input, unsigned int len) {
    ASSERT(input, (L""));
    ASSERT(output, (L""));

    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (unsigned char)(input[i] & 0xff);
        output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
        output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
        }
}

#ifdef REORDER

// Decodes input (unsigned char) into output (uint32). Assumes len is a multiple of 4.
void Decode (uint32 *output, unsigned char *input, unsigned int len) {
    ASSERT(input, (L""));
    ASSERT(output, (L""));

    register uint32 out,other;

    // for (i = 0, j = 0; j < len; i++, j += 4)
    // output[i] = ((uint32)input[j]) | (((uint32)input[j+1]) << 8) |
    // (((uint32)input[j+2]) << 16) | (((uint32)input[j+3]) << 24);

    // USC/ISI J. Touch
    // these are optimized swap routines, in C code they cost more in "computation" operations, but less in
    // "loads" than the above code, and run substantially faster as a result
#if (!defined(hpux))
#define swapbyte(src,dst) { \
     out = ROTATE_LEFT((src),16); \
     other = out >> 8; \
     other &= 0x00ff00ff; \
     out &= 0x00ff00ff; \
     out <<= 8; \
     (dst) = out | other; \
     }
#else
#define swapbyte(src,dst) { \
     (dst) = (ROTATE_LEFT((src),8) & 0x00ff00ff) | ROTATE_LEFT((src) & 0x00ff00ff,24); \
     }
#endif

    // USC/ISI J. Touch
    // unroll the loop above, because the code runs faster with constants for indices than even with variable indices
    // as conventional (automatic) unrolling would perform (!)
    swapbyte(((uint32 *)input)[0],output[0]);
    swapbyte(((uint32 *)input)[1],output[1]);
    swapbyte(((uint32 *)input)[2],output[2]);
    swapbyte(((uint32 *)input)[3],output[3]);
    swapbyte(((uint32 *)input)[4],output[4]);
    swapbyte(((uint32 *)input)[5],output[5]);
    swapbyte(((uint32 *)input)[6],output[6]);
    swapbyte(((uint32 *)input)[7],output[7]);
    swapbyte(((uint32 *)input)[8],output[8]);
    swapbyte(((uint32 *)input)[9],output[9]);
    swapbyte(((uint32 *)input)[10],output[10]);
    swapbyte(((uint32 *)input)[11],output[11]);
    swapbyte(((uint32 *)input)[12],output[12]);
    swapbyte(((uint32 *)input)[13],output[13]);
    swapbyte(((uint32 *)input)[14],output[14]);
    swapbyte(((uint32 *)input)[15],output[15]);
}

#endif // #ifdef REORDER

}  // namespace omaha

