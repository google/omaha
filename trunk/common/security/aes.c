// Copyright 2007-2009 Google Inc.
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
// Optimized for code size. Lacking decrypt functionality.
// Currently 1042 bytes of code.

#include "aes.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>

static const uint8_t sbox_e[256]= {
    99  , 124 , 119 , 123 , 242 , 107 , 111 , 197
  , 48  , 1   , 103 , 43  , 254 , 215 , 171 , 118
  , 202 , 130 , 201 , 125 , 250 , 89  , 71  , 240
  , 173 , 212 , 162 , 175 , 156 , 164 , 114 , 192
  , 183 , 253 , 147 , 38  , 54  , 63  , 247 , 204
  , 52  , 165 , 229 , 241 , 113 , 216 , 49  , 21
  , 4   , 199 , 35  , 195 , 24  , 150 , 5   , 154
  , 7   , 18  , 128 , 226 , 235 , 39  , 178 , 117
  , 9   , 131 , 44  , 26  , 27  , 110 , 90  , 160
  , 82  , 59  , 214 , 179 , 41  , 227 , 47  , 132
  , 83  , 209 , 0   , 237 , 32  , 252 , 177 , 91
  , 106 , 203 , 190 , 57  , 74  , 76  , 88  , 207
  , 208 , 239 , 170 , 251 , 67  , 77  , 51  , 133
  , 69  , 249 , 2   , 127 , 80  , 60  , 159 , 168
  , 81  , 163 , 64  , 143 , 146 , 157 , 56  , 245
  , 188 , 182 , 218 , 33  , 16  , 255 , 243 , 210
  , 205 , 12  , 19  , 236 , 95  , 151 , 68  , 23
  , 196 , 167 , 126 , 61  , 100 , 93  , 25  , 115
  , 96  , 129 , 79  , 220 , 34  , 42  , 144 , 136
  , 70  , 238 , 184 , 20  , 222 , 94  , 11  , 219
  , 224 , 50  , 58  , 10  , 73  , 6   , 36  , 92
  , 194 , 211 , 172 , 98  , 145 , 149 , 228 , 121
  , 231 , 200 , 55  , 109 , 141 , 213 , 78  , 169
  , 108 , 86  , 244 , 234 , 101 , 122 , 174 , 8
  , 186 , 120 , 37  , 46  , 28  , 166 , 180 , 198
  , 232 , 221 , 116 , 31  , 75  , 189 , 139 , 138
  , 112 , 62  , 181 , 102 , 72  , 3   , 246 , 14
  , 97  , 53  , 87  , 185 , 134 , 193 , 29  , 158
  , 225 , 248 , 152 , 17  , 105 , 217 , 142 , 148
  , 155 , 30  , 135 , 233 , 206 , 85  , 40  , 223
  , 140 , 161 , 137 , 13  , 191 , 230 , 66  , 104
  , 65  , 153 , 45  , 15  , 176 , 84  , 187 , 22
  };

static uint8_t xtime(int in) {
  in <<= 1;
  if( in&0x100 ) in ^= 0x11b;
  return (uint8_t)in;
  }

static void expand_key(const uint8_t* key, uint32_t* expanded_key) {
  int   nrounds;
  union {
    uint8_t b[16];
    uint32_t w[4];
    } W;
  uint8_t xor = 1;

  memcpy( &W, key, 16 );
  memcpy( expanded_key, &W, 16 );

  for( nrounds = 0; nrounds < 10; ++nrounds ) {

      // update key schedule
    W.b[0] ^= sbox_e[W.b[12+1]] ^ xor;
    W.b[1] ^= sbox_e[W.b[12+2]];
    W.b[2] ^= sbox_e[W.b[12+3]];
    W.b[3] ^= sbox_e[W.b[12+0]];
    W.w[1] ^= W.w[0];
    W.w[2] ^= W.w[1];
    W.w[3] ^= W.w[2];

    xor = xtime( xor );

    expanded_key += 4;
    memcpy( expanded_key, &W, 16 );
    }
  }

void AES_encrypt_block(const uint8_t* key, const uint8_t* in, uint8_t* out) {
  int j, nrounds;
  union {
    uint8_t b[16];
    uint32_t w[4];
    } rd_state;
  uint32_t expanded_key[11 * 4];
  uint32_t* expkey = &expanded_key[0];

  expand_key( key, expanded_key );

  memcpy( &rd_state, in, 16 );

    // xor with initial key
  rd_state.w[0] ^= *expkey++;
  rd_state.w[1] ^= *expkey++;
  rd_state.w[2] ^= *expkey++;
  rd_state.w[3] ^= *expkey++;

  nrounds = 10;

  do {
    uint8_t tmp;

      // bytesub && shiftrow

      // 1st
    rd_state.b[0] = sbox_e[rd_state.b[0]];
    rd_state.b[4] = sbox_e[rd_state.b[4]];
    rd_state.b[8] = sbox_e[rd_state.b[8]];
    rd_state.b[12] = sbox_e[rd_state.b[12]];

      // 2nd
    tmp = rd_state.b[1];
    rd_state.b[1] = sbox_e[rd_state.b[5]];
    rd_state.b[5] = sbox_e[rd_state.b[9]];
    rd_state.b[9] = sbox_e[rd_state.b[13]];
    rd_state.b[13] = sbox_e[tmp];

      // 3th
    tmp = rd_state.b[2];
    rd_state.b[2] = sbox_e[rd_state.b[10]];
    rd_state.b[10] = sbox_e[tmp];
    tmp = rd_state.b[6];
    rd_state.b[6] = sbox_e[rd_state.b[14]];
    rd_state.b[14] = sbox_e[tmp];

      // 4th
    tmp = rd_state.b[3];
    rd_state.b[3] = sbox_e[rd_state.b[15]];
    rd_state.b[15] = sbox_e[rd_state.b[11]];
    rd_state.b[11] = sbox_e[rd_state.b[7]];
    rd_state.b[7] = sbox_e[tmp];

      // mixcolumn except for last round
    if( --nrounds ) {
      for( j = 0; j < 16; j += 4 ) {
        uint8_t tmp =
            rd_state.b[j+0] ^
            rd_state.b[j+1] ^
            rd_state.b[j+2] ^
            rd_state.b[j+3];

        rd_state.b[j+0] ^= xtime( rd_state.b[j+0] ^ rd_state.b[j+1] ) ^ tmp;
        rd_state.b[j+1] ^= xtime( rd_state.b[j+1] ^ rd_state.b[j+2] ) ^ tmp;
        rd_state.b[j+2] ^= xtime( rd_state.b[j+2] ^ rd_state.b[j+3] ) ^ tmp;
        rd_state.b[j+3] =
            rd_state.b[j+0] ^
            rd_state.b[j+1] ^
            rd_state.b[j+2] ^
            tmp;
        }
      }

    rd_state.w[0] ^= *expkey++;
    rd_state.w[1] ^= *expkey++;
    rd_state.w[2] ^= *expkey++;
    rd_state.w[3] ^= *expkey++;
  } while( nrounds );

  memcpy( out, &rd_state, 16 );
}
