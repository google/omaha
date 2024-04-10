// Copyright 2024 Google Inc.
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
{0x0e,
0x04,
0x9d, 0x0f, 0x34, 0x55, 0x70, 0x0e, 0x3d, 0xb3,
0x60, 0x06, 0x21, 0xb2, 0x20, 0xf6, 0xdb, 0x7d,
0xb5, 0x24, 0x71, 0x6e, 0xf9, 0xe5, 0xc3, 0xfb,
0xc2, 0xa8, 0xa7, 0xad, 0x87, 0x01, 0x7a, 0x1d,
0x26, 0xb6, 0x22, 0x03, 0x04, 0x69, 0xd3, 0xad,
0x32, 0xf1, 0x33, 0x8b, 0xa1, 0x5e, 0x33, 0x81,
0xd0, 0x4b, 0x32, 0xf5, 0x81, 0xcc, 0xfe, 0x8b,
0x9b, 0x1f, 0x9c, 0x39, 0x2a, 0x38, 0xa7, 0x60};
