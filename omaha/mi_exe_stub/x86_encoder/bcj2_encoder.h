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
// Implementation taken from Bcj2Coder implementation in LZMA SDK and converted
// to use std::string as the interface.

#ifndef OMAHA_MI_EXE_STUB_X86_ENCODER_BCJ2_ENCODER_H_
#define OMAHA_MI_EXE_STUB_X86_ENCODER_BCJ2_ENCODER_H_

#include <string>

namespace omaha {

// TODO(omaha): this is currently a single-shot interface. The entire buffer
// to be encoded must be loaded in memory. It'd be nice to make work in chunks.
// TODO(omaha): consider converting this interface to use std::vector. The
// reason std::string is used is for the auto-resize convenience.
// All input/output parameters from this function are *binary* strings.
// Use std::string::size() to get the length of the buffer. Do not use
// std::string::c_str() on these strings.
bool Bcj2Encode(const std::string& input,
                std::string* main_output,
                std::string* call_output,
                std::string* jump_output,
                std::string* misc_output);

}  // namespace omaha

#endif  // OMAHA_MI_EXE_STUB_X86_ENCODER_BCJ2_ENCODER_H_
