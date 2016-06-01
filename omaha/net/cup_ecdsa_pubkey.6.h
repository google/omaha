// Copyright 2016 Google Inc.
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
{0x06,
0x04,
0x08, 0x40, 0xd2, 0x45, 0xc6, 0x4f, 0x2a, 0xff,
0x24, 0xe0, 0x91, 0x18, 0x6c, 0x5e, 0xcc, 0x27,
0x1c, 0x71, 0xe7, 0xa9, 0xbe, 0xc7, 0xc6, 0x7e,
0xb2, 0xf8, 0x39, 0x80, 0x19, 0x91, 0x5f, 0x52,
0x7c, 0x03, 0xd0, 0xd8, 0x44, 0xbe, 0x8d, 0xbc,
0x59, 0x0d, 0x72, 0x67, 0x11, 0xb5, 0x23, 0x5e,
0x99, 0x49, 0xbb, 0xfe, 0x2a, 0xab, 0x52, 0x7c,
0x5a, 0x69, 0xd9, 0x2a, 0x71, 0x64, 0x8e, 0x0f};
