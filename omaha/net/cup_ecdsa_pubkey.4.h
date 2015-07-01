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
{0x04,
0x04,
0x91, 0xc8, 0xdc, 0x96, 0xef, 0x3d, 0xfb, 0x17,
0x4d, 0x58, 0xe6, 0x66, 0xd3, 0xbb, 0x70, 0xd1,
0xf5, 0xbf, 0xda, 0x09, 0xb9, 0x00, 0xb0, 0x05,
0x9a, 0x3a, 0x21, 0xec, 0x3d, 0x48, 0xfb, 0xc8,
0xf8, 0x22, 0x9e, 0x32, 0xc0, 0x4d, 0xaf, 0x08,
0x38, 0xb9, 0xef, 0x29, 0x67, 0x71, 0xce, 0x4d,
0x58, 0x4e, 0xc3, 0x73, 0xa1, 0x49, 0x7f, 0xa7,
0x0c, 0x73, 0x9c, 0xce, 0xf5, 0x47, 0xc1, 0xc7};
