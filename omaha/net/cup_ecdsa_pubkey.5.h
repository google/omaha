// Copyright 2014 Google Inc.
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
{0x05,
0x04,
0x07, 0xe6, 0x22, 0xfb, 0x74, 0x9d, 0x24, 0xa0,
0xb2, 0x14, 0x99, 0xa6, 0xfa, 0xcb, 0x96, 0xdc,
0x2c, 0x97, 0xca, 0x0b, 0xd5, 0xb1, 0xb0, 0xdb,
0x3e, 0x72, 0x60, 0xa0, 0x25, 0xf8, 0x19, 0xe4,
0xed, 0xa0, 0xbf, 0x10, 0xfd, 0x68, 0xcf, 0xc7,
0xb0, 0x86, 0xb5, 0x73, 0x8b, 0xd5, 0x78, 0xdb,
0xc7, 0x45, 0x4f, 0x3c, 0xdd, 0xc3, 0xae, 0xfa,
0x99, 0x48, 0xbf, 0xe8, 0x5f, 0x2f, 0x61, 0x57};
