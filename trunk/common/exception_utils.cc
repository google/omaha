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

#include "exception_utils.h"


// Capture a CONTEXT that represents machine state at callsite
// TODO(omaha): 64 bit platforms can pass through to
//      RtlCaptureContext (or can they?)
__declspec(naked) PCONTEXT WINAPI CaptureContext(PCONTEXT runner) {
  runner;   // unreferenced formal parameter
  __asm {
    // set up a call frame
    push ebp
    mov ebp, esp

    // save ecx for later
    push ecx

    // fetch the context record pointer argument into ecx
    // which we use as pointer to context throughout the rest
    // of this function
    mov ecx, DWORD PTR [ebp + 8]

    // set flags
    mov [ecx]CONTEXT.ContextFlags, CONTEXT_SEGMENTS | CONTEXT_INTEGER | \
                                   CONTEXT_CONTROL | CONTEXT_FLOATING_POINT

    // stash the integer registers away
    mov [ecx]CONTEXT.Edi, edi
    mov [ecx]CONTEXT.Ebx, ebx
    mov [ecx]CONTEXT.Edx, edx
    mov [ecx]CONTEXT.Eax, eax
    mov [ecx]CONTEXT.Esi, esi
    // get the saved ecx
    pop eax
    mov [ecx]CONTEXT.Ecx, eax

    // now control registers
    pushfd
    pop eax
    mov [ecx]CONTEXT.EFlags, eax

    // get the old ebp, our FP points to it
    mov eax, [ebp]
    mov [ecx]CONTEXT.Ebp, eax

    // get return address and record as eip
    mov eax, [ebp + 4]
    mov [ecx]CONTEXT.Eip, eax

    // esp post-return is ...
    lea eax, [ebp + 0xC]
    mov [ecx]CONTEXT.Esp, eax

    // snarf segment registers
    mov word ptr [ecx]CONTEXT.SegSs, ss
    mov word ptr [ecx]CONTEXT.SegCs, cs
    mov word ptr [ecx]CONTEXT.SegGs, gs
    mov word ptr [ecx]CONTEXT.SegFs, fs
    mov word ptr [ecx]CONTEXT.SegEs, es
    mov word ptr [ecx]CONTEXT.SegDs, ds

    // and lastly grab floating point state
    fnsave [ecx]CONTEXT.FloatSave

    // return the CONTEXT pointer
    mov eax, ecx

    // and return
    pop ebp
    ret 4
  }
}
