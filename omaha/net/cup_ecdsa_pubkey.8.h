// Copyright 2018 Google Inc.
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
// CUP-ECDSA public keys consist of a byte array, 66 bytes long, containing:
// * The key ID (one byte)
// * The public key in X9.62 uncompressed encoding (65 bytes):
//     * Uncompressed header byte (0x04)
//     * Gx coordinate (256-bit integer, big-endian)
//     * Gy coordinate (256-bit integer, big-endian)
{0x08,
0x04,
0xf8, 0x9d, 0xa2, 0x0a, 0x97, 0xe4, 0xf2, 0x54,
0xe1, 0x72, 0xe2, 0x94, 0x3f, 0x34, 0xda, 0x55,
0xc5, 0x23, 0x84, 0xd4, 0x77, 0x01, 0x81, 0xca,
0xfa, 0xd4, 0xde, 0x94, 0x67, 0x47, 0xbf, 0x21,
0x86, 0xc7, 0xb4, 0x4f, 0xec, 0x1a, 0x61, 0x61,
0x23, 0xe6, 0xa4, 0x7e, 0x8f, 0xe1, 0x5a, 0xfb,
0xd9, 0xa9, 0x34, 0x5b, 0x56, 0xb4, 0x6d, 0x6e,
0x79, 0x3b, 0xd1, 0xd6, 0xda, 0x8c, 0xf7, 0xad};
