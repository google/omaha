// Copyright 2021 Google Inc.
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
{0x0b,
0x04,
0x80, 0x7d, 0xf4, 0x59, 0x12, 0x5f, 0xe2, 0x0e,
0x88, 0xd8, 0x2d, 0x45, 0x2a, 0xc0, 0x50, 0x17,
0x7a, 0x87, 0x00, 0xd2, 0xf0, 0xb6, 0x1c, 0x18,
0xb0, 0xdb, 0x76, 0x3d, 0x64, 0xc3, 0x40, 0x14,
0xb4, 0xb9, 0xf4, 0x91, 0x13, 0xcd, 0xe9, 0x88,
0xe3, 0xe8, 0x25, 0x07, 0x1e, 0x3f, 0x34, 0xe4,
0x31, 0xf6, 0xe0, 0x86, 0x39, 0xd9, 0x7a, 0xb0,
0xe3, 0x2c, 0x98, 0x41, 0xb0, 0x39, 0x1f, 0x16};
