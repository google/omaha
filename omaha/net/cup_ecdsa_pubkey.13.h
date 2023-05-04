// Copyright 2023 Google Inc.
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
{0x0d,
0x04,
0xf3, 0x65, 0x8a, 0x9c, 0xc9, 0x1b, 0xe2, 0x77,
0x95, 0x45, 0x88, 0x32, 0x19, 0xa5, 0xe8, 0x11,
0x8e, 0x67, 0x0d, 0xa8, 0x8e, 0xad, 0xe9, 0xe3,
0xb7, 0x42, 0xcc, 0x48, 0xaf, 0xd6, 0x88, 0xfb,
0x38, 0xf9, 0xfb, 0x23, 0xcf, 0xd9, 0x10, 0xb3,
0xe4, 0xf5, 0x3e, 0x32, 0x51, 0xec, 0x7b, 0xf6,
0xba, 0x91, 0xab, 0xb0, 0x97, 0x12, 0x68, 0x5f,
0x04, 0xd6, 0x72, 0xb8, 0x5a, 0xb4, 0xae, 0x70};
