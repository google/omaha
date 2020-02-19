// Copyright 2020 Google Inc.
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
{0x0a,
0x04,
0xcc, 0xea, 0x82, 0xf1, 0xc2, 0x8d, 0x51, 0x82,
0x22, 0xd1, 0x49, 0x0d, 0xbb, 0x4b, 0x26, 0x64,
0xa0, 0xd6, 0xf3, 0x0e, 0x7f, 0xd0, 0x49, 0x84,
0xc3, 0x52, 0x90, 0xe8, 0x08, 0xff, 0xe4, 0x95,
0x81, 0x31, 0xd5, 0x15, 0x9b, 0x5d, 0xc4, 0x21,
0x5c, 0x05, 0xc0, 0xf9, 0x64, 0x3b, 0xf5, 0x3a,
0xbd, 0x76, 0xbe, 0x8a, 0xee, 0x30, 0xc9, 0x40,
0x7c, 0xf4, 0x61, 0x5a, 0x95, 0xd2, 0x7b, 0x06};
