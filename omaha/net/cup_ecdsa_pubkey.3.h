// Copyright 2013 Google Inc.
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
{0x03,
0x04,
0x19, 0x3e, 0x1b, 0xa3, 0x6b, 0xfd, 0x7c, 0x4a,
0x30, 0xec, 0x5d, 0xf3, 0x75, 0x2a, 0xf8, 0x77,
0x39, 0x45, 0x23, 0x1e, 0x9e, 0xb6, 0x8e, 0x44,
0x1c, 0x74, 0xc2, 0x9a, 0xce, 0xb8, 0x5c, 0x87,
0x43, 0x87, 0xa5, 0x25, 0x47, 0x07, 0xc0, 0x08,
0xaa, 0x10, 0x97, 0xc5, 0x79, 0x27, 0x83, 0x87,
0x37, 0x37, 0x4f, 0x13, 0x7e, 0x63, 0xfd, 0x80,
0x70, 0x23, 0x08, 0xa1, 0xa7, 0x7f, 0xae, 0x49};
