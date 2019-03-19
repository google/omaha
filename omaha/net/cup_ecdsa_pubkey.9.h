// Copyright 2019 Google Inc.
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
{0x09,
0x04,
0xb1, 0x5c, 0x15, 0x32, 0x62, 0x09, 0x69, 0x60,
0x63, 0x92, 0xd4, 0xb1, 0xf6, 0x6d, 0x49, 0xad,
0x95, 0x98, 0x06, 0xf3, 0x26, 0x6e, 0xca, 0xc6,
0x19, 0x03, 0xe1, 0x49, 0xc0, 0xed, 0x6a, 0x8f,
0x83, 0x96, 0x80, 0xfc, 0xef, 0x56, 0x26, 0x11,
0xeb, 0xa7, 0x01, 0xa4, 0x2f, 0x44, 0xc6, 0x60,
0x0e, 0x1d, 0x67, 0x09, 0xa5, 0xa4, 0x54, 0x6e,
0xb0, 0xa3, 0xbe, 0x7e, 0xc8, 0x36, 0xca, 0x0c};
