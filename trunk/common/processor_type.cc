// Copyright 2006-2009 Google Inc.
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
// Processor brand detection implementation
//
#include "omaha/common/processor_type.h"
#include "omaha/common/constants.h"
#include "omaha/common/utils.h"

namespace omaha {

// Returns "GenuineIntel", "AuthenticAMD", etc...
CString GetProcessorType() {
  // Reference for checking for chip type
  // http://en.wikipedia.org/wiki/CPUID

  // There are 12 characters in the chip id (e.g. "GenuineIntel")
  union {
    char str[16];
    uint32 n[4];
  } regs;
  SetZero(regs.str);
  __asm {
    xor eax, eax;  // Set EAX = 0 to get CPU type
    cpuid;         // Now ebx, edx, ecx will contain cpu brand
    mov regs.n[0], ebx;
    mov regs.n[4], edx;
    mov regs.n[8], ecx;
  }
  return CString(regs.str);
}

// Returns true if we're running on an Intel
bool IsIntelProcessor() {
  CString chip = GetProcessorType();
  return (chip == kIntelVendorId);
}

}  // namespace omaha
