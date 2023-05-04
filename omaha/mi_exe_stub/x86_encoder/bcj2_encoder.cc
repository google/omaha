// Copyright 2009 Google Inc.
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
// The BCJ2 algorithm takes advantage of the fact that a lot of relative jumps
// in x86 code are to the same address. It essentially performs the moral
// equivalent of the following conversion:
// ...
// JMP 8 bytes back
// ...
// JMP 28 bytes back
// ...
// JMP 40 bytes back
// ...
// to:
// ...
// JMP 0x1000
// ...
// JMP 0x1000
// ...
// JMP 0x1000
// ...
//
// The second form has a lot more repetition, and standard entropy coding can
// compress it further.
//
// Details:
// TODO(omaha): figure out what the byte before a CALL (0xE8) instruction means.
// TODO(omaha): document exactly how the range encoding is used. There are 258
// range encoding bits. The first 256 are used for CALL instructions based on
// the value of the previous byte; byte 256 is used for JMP, and byte 257 is
// used for JCC.
//
// BCJ2 converts the targets for the CALL (0xE8), JMP (0xE9), and certain JCC
// (0xF80-0xF8F) instructions into absolute jumps.  This is not a full x86 op
// code interpreter, and it is almost certain that bytes that are data rather
// than instructions will be encoded.
// The algorithm uses the following steps to perform the conversion:
// 1. Iterate through each byte while the current position is at least 5 bytes
//    away from the end.
// 1. If the byte is not a CALL , JMP, or JCC instruction, copy the byte to the
//    output and go back to step 1.
// 2. Otherwise, calculate the target of the jump. If the target of this jump is
//    not within this file (e.g. past the end of the input), we do not rewrite
//    this jump: write 0 to the range encoder to indicate the target was not
//    processed and go back to step 1.
// 3. If the instruction is CALL, write the absolute target to the call output
//    stream. Otherwise, write the absolute target to the jump output stream.
//    Write a 1 to the range encoder to indicate the target was processed and go
//    back to step 1.
// 4. Once within 5 bytes of the end of the input, flush out the remaining
//    bytes. If one of the bytes happens to be one of the op codes that should
//    be handled, write a 0 to the range encoder to indicate that it was not
//    processed.

#include "omaha/mi_exe_stub/x86_encoder/bcj2_encoder.h"

#include "base/basictypes.h"
#include "omaha/mi_exe_stub/x86_encoder/range_encoder.h"

namespace omaha {

namespace {

bool IsJcc(uint8 byte0, uint8 byte1) {
  return (byte0 == 0x0F && (byte1 & 0xF0) == 0x80);
}

bool IsJ(uint8 byte0, uint8 byte1) {
  return ((byte1 & 0xFE) == 0xE8 || IsJcc(byte0, byte1));
}

int GetIndex(uint8 byte0, uint8 byte1) {
  return ((byte1 == 0xE8) ? byte0 : ((byte1 == 0xE9) ? 256 : 257));
}

}  // namespace

// Conversions from signed char to uint8/unsigned char are preserving the
// bit pattern, which is the desired behavior for this implementation.
bool Bcj2Encode(const std::string& input,
                std::string* main_output,
                std::string* call_output,
                std::string* jump_output,
                std::string* misc_output) {
  if (!main_output || !call_output || !jump_output || !misc_output) {
    return false;
  }

  main_output->reserve(input.size());

  size_t input_position = 0;

  static const int kNumberOfMoveBits = 5;
  RangeEncoder range_encoder(misc_output);
  RangeEncoderBit<kNumberOfMoveBits> status_encoder[256 + 2];

  uint8 previous_byte = 0;

  while (true) {
    if (input.size() - input_position < 5) {
      for (; input_position < input.size(); ++input_position) {
        uint8 byte = input[input_position];
        *main_output += byte;

        size_t index;
        if (0xE8 == byte) {
          index = previous_byte;
        } else if (0xE9 == byte) {
          index = 256;
        } else if (IsJcc(previous_byte, byte)) {
          index = 257;
        } else {
          previous_byte = byte;
          continue;
        }
        status_encoder[index].Encode(0, &range_encoder);
        previous_byte = byte;
      }

      range_encoder.Flush();
      return true;
    }

    while (input_position <= input.size() - 5) {
      uint8 byte = input[input_position];
      *main_output += byte;

      if (!IsJ(previous_byte, byte)) {
        input_position++;
        previous_byte = byte;
        continue;
      }

      uint8 next_byte = input[input_position + 4];
      uint32 src =
        static_cast<uint8>(next_byte) << 24 |
        static_cast<uint8>(input[input_position + 3]) << 16 |
        static_cast<uint8>(input[input_position + 2]) << 8 |
        static_cast<uint8>(input[input_position + 1]);
      size_t dst = input_position + src + 5;

      uint32 index = GetIndex(previous_byte, byte);
      if (dst < input.size()) {
        status_encoder[index].Encode(1, &range_encoder);
        input_position += 5;
        std::string* s = (byte == 0xE8) ? call_output : jump_output;
        for (int i = 24; i >= 0; i -= 8) {
          *s += static_cast<uint8>(dst >> i);
        }
        previous_byte = next_byte;
      } else {
        status_encoder[index].Encode(0, &range_encoder);
        input_position++;
        previous_byte = byte;
      }
    }
  }
}

}  // namespace omaha
