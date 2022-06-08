// Copyright 2022 Google Inc.
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
{0x0c,
0x04,
0x4c, 0x68, 0x73, 0xb8, 0x32, 0x47, 0x44, 0x5c,
0x7b, 0xff, 0xf6, 0x2a, 0xa9, 0xd6, 0x8d, 0x4d,
0x8d, 0xdd, 0x67, 0xc5, 0xfd, 0x5e, 0x15, 0x02,
0xd2, 0x8e, 0xdf, 0x99, 0x9b, 0x4e, 0x15, 0xef,
0x5c, 0x8c, 0xf3, 0x0b, 0xf4, 0xa5, 0x47, 0x3d,
0xad, 0x8c, 0xe6, 0x1c, 0x61, 0x25, 0x38, 0x48,
0x6d, 0x14, 0x0c, 0x12, 0x59, 0x2c, 0x1f, 0x2e,
0x25, 0x95, 0x9a, 0x16, 0x83, 0xa2, 0x93, 0x67};
