// Copyright 2017 Google Inc.
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
{0x07,
0x04,
0x8f, 0x44, 0x0a, 0xb9, 0xf5, 0xc8, 0x38, 0x13,
0x77, 0xd0, 0x3b, 0x4a, 0x78, 0xe6, 0x00, 0xe4,
0xd5, 0x7a, 0xe0, 0x57, 0xd8, 0x1c, 0x3a, 0x2e,
0xe2, 0xc1, 0xaa, 0xb5, 0xc3, 0x54, 0x22, 0x5c,
0x69, 0x4f, 0x32, 0x1b, 0x3b, 0x8e, 0x6b, 0x07,
0x8e, 0x50, 0x20, 0xb8, 0x56, 0xe9, 0xa0, 0xd3,
0xc3, 0x08, 0xd6, 0x2d, 0x1d, 0x58, 0x0a, 0xaa,
0x44, 0x00, 0x62, 0x02, 0xbc, 0x5b, 0x3c, 0x75};
